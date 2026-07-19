#include "visual_homing/live_verification_capture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "visual_homing/gray8_resize_preprocessor.hpp"
#include "visual_homing/route_descriptor_index.hpp"
#include "visual_homing/route_signature_entry.hpp"

namespace vh {
namespace {

void require_config(bool condition, const char* message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

void require_frame(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::uint64_t timestamp_to_ns(Timestamp timestamp) {
    const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(
        timestamp.time_since_epoch()).count();
    require_frame(count >= 0, "Live verification frame timestamp must not be negative");
    return static_cast<std::uint64_t>(count);
}

double elapsed_ms(
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point finish) {
    return std::chrono::duration<double, std::milli>(finish - start).count();
}

const RouteSearchIndexRecord& find_search_index(
    const RoutePackageManifest& manifest,
    const std::string& id) {
    const auto found = std::find_if(
        manifest.search_indexes.begin(),
        manifest.search_indexes.end(),
        [&id](const RouteSearchIndexRecord& index) { return index.id == id; });
    require_config(found != manifest.search_indexes.end(),
        "Live verification search index is absent from the source manifest");
    return *found;
}

void validate_context(const LiveVerificationFrameContext& context) {
    require_frame(std::isfinite(context.route_progress)
            && context.route_progress >= 0.0 && context.route_progress <= 1.0,
        "Live verification route progress must be finite and within zero to one");
    require_frame(std::isfinite(context.altitude_m),
        "Live verification altitude must be finite");
    require_frame(std::isfinite(context.scale_ratio) && context.scale_ratio > 0.0,
        "Live verification scale ratio must be finite and positive");
    require_frame(std::isfinite(context.yaw_rad),
        "Live verification yaw must be finite");
    if (context.has_local_pose) {
        require_frame(std::isfinite(context.local_x_m)
                && std::isfinite(context.local_y_m)
                && std::isfinite(context.local_z_m)
                && std::isfinite(context.local_yaw_rad)
                && std::isfinite(context.local_position_uncertainty_m)
                && std::isfinite(context.approach_radius_m)
                && context.local_position_uncertainty_m >= 0.0
                && context.approach_radius_m >= 0.0,
            "Live verification local pose or quality is invalid");
    }
}

} // namespace

struct LiveVerificationCaptureSession::Impl {
    explicit Impl(LiveVerificationCaptureConfig session_config)
        : config(std::move(session_config)), writer(config.writer) {
        const auto& index_record = find_search_index(
            writer.manifest(), config.writer.search_index_id);
        require_config(index_record.descriptor_type == gray8_centered_block_mean_i8_v1,
            "Live verification supports only the centered block-mean VHIX descriptor");
        const auto index_path = config.writer.source_manifest_path.parent_path()
            / index_record.relative_path;
        descriptor_index = read_route_descriptor_index(index_path);
        const auto descriptor_dimensions = static_cast<std::uint32_t>(descriptor_index.grid_width)
            * static_cast<std::uint32_t>(descriptor_index.grid_height);
        require_config(descriptor_dimensions == index_record.descriptor_dimensions,
            "Live verification VHIX dimensions disagree with the source manifest");
        require_config(descriptor_dimensions == config.writer.selector.descriptor_dimensions,
            "Live verification selector dimensions disagree with VHIX");
        preprocessor = std::make_unique<Gray8ResizePreprocessor>(
            descriptor_index.source_width,
            descriptor_index.source_height);
    }

    LiveVerificationCaptureConfig config;
    VerificationPackageWriter writer;
    RouteDescriptorIndex descriptor_index;
    std::unique_ptr<Gray8ResizePreprocessor> preprocessor;
    LiveVerificationCaptureMetrics capture_metrics;
};

LiveVerificationCaptureSession::LiveVerificationCaptureSession(
    LiveVerificationCaptureConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

LiveVerificationCaptureSession::~LiveVerificationCaptureSession() = default;

LiveVerificationFrameResult LiveVerificationCaptureSession::observe(
    const Frame& native_frame,
    const LiveVerificationFrameContext& context) {
    ++impl_->capture_metrics.frames_observed;
    try {
        const auto& layer = impl_->config.writer.verification_layer;
        require_frame(native_frame.format == PixelFormat::Gray8,
            "Live verification native frame must be Gray8");
        require_frame(native_frame.width == layer.width && native_frame.height == layer.height,
            "Live verification native frame dimensions disagree with the verification layer");
        const auto expected_size = static_cast<std::size_t>(native_frame.width)
            * static_cast<std::size_t>(native_frame.height);
        require_frame(native_frame.data.size() == expected_size,
            "Live verification native frame payload size is invalid");
        validate_context(context);

        LiveVerificationFrameResult result;
        const auto descriptor_started = std::chrono::steady_clock::now();
        const auto tracking_frame = impl_->preprocessor->process(native_frame);
        NavigationEstimate navigation;
        navigation.timestamp = native_frame.timestamp;
        navigation.altitude_m = context.altitude_m;
        navigation.course_error_rad = context.yaw_rad;
        navigation.confidence = context.health_ready ? 1.0 : 0.0;
        const auto tracking_entry = make_route_signature_entry(tracking_frame, navigation);
        auto descriptor = make_gray8_centered_block_mean_descriptor(
            tracking_entry,
            impl_->descriptor_index.grid_width,
            impl_->descriptor_index.grid_height);
        const auto descriptor_finished = std::chrono::steady_clock::now();

        VerificationKeyframeObservation observation{
            .frame_id = native_frame.id,
            .timestamp_ns = timestamp_to_ns(native_frame.timestamp),
            .health_ready = context.health_ready,
            .route_progress = context.route_progress,
            .altitude_m = context.altitude_m,
            .scale_ratio = context.scale_ratio,
            .yaw_rad = context.yaw_rad,
            .descriptor = std::move(descriptor),
            .has_local_pose = context.has_local_pose,
            .local_x_m = context.local_x_m,
            .local_y_m = context.local_y_m,
            .local_z_m = context.local_z_m,
            .local_yaw_rad = context.local_yaw_rad,
            .local_position_uncertainty_m = context.local_position_uncertainty_m,
            .approach_radius_m = context.approach_radius_m,
        };
        const auto selector_started = std::chrono::steady_clock::now();
        result.decision = impl_->writer.evaluate(observation);
        const auto selector_finished = std::chrono::steady_clock::now();

        result.descriptor_latency_ms = elapsed_ms(descriptor_started, descriptor_finished);
        result.selector_latency_ms = elapsed_ms(selector_started, selector_finished);
        impl_->capture_metrics.total_descriptor_latency_ms += result.descriptor_latency_ms;
        impl_->capture_metrics.maximum_descriptor_latency_ms = std::max(
            impl_->capture_metrics.maximum_descriptor_latency_ms,
            result.descriptor_latency_ms);
        impl_->capture_metrics.total_selector_latency_ms += result.selector_latency_ms;
        impl_->capture_metrics.maximum_selector_latency_ms = std::max(
            impl_->capture_metrics.maximum_selector_latency_ms,
            result.selector_latency_ms);

        if (!result.decision.valid) {
            ++impl_->capture_metrics.invalid_observations;
            return result;
        }
        if (!result.decision.request_native_capture) {
            return result;
        }
        ++impl_->capture_metrics.capture_requests;

        const auto native_entry = make_route_signature_entry(native_frame, navigation);
        const auto publication_started = std::chrono::steady_clock::now();
        try {
            result.publication = impl_->writer.publish(
                native_entry,
                observation,
                result.decision,
                context.publication_metadata);
        } catch (...) {
            ++impl_->capture_metrics.publication_failures;
            throw;
        }
        const auto publication_finished = std::chrono::steady_clock::now();
        result.publication_latency_ms = elapsed_ms(publication_started, publication_finished);
        impl_->capture_metrics.total_publication_latency_ms += result.publication_latency_ms;
        impl_->capture_metrics.maximum_publication_latency_ms = std::max(
            impl_->capture_metrics.maximum_publication_latency_ms,
            result.publication_latency_ms);
        ++impl_->capture_metrics.publications;
        if (result.publication->gate_published) {
            ++impl_->capture_metrics.gates_published;
        }
        std::error_code chunk_size_error;
        std::error_code manifest_size_error;
        const auto chunk_bytes = std::filesystem::file_size(
            result.publication->chunk_path, chunk_size_error);
        const auto manifest_bytes = std::filesystem::file_size(
            result.publication->manifest_path, manifest_size_error);
        if (!chunk_size_error && !manifest_size_error) {
            impl_->capture_metrics.output_bytes_published += chunk_bytes + manifest_bytes;
        }
        impl_->capture_metrics.package_files_checked = impl_->writer.metrics().package_files_checked;
        return result;
    } catch (...) {
        ++impl_->capture_metrics.processing_failures;
        throw;
    }
}

LiveVerificationCaptureMetrics LiveVerificationCaptureSession::metrics() const {
    auto result = impl_->capture_metrics;
    result.package_files_checked = impl_->writer.metrics().package_files_checked;
    return result;
}

const RoutePackageManifest& LiveVerificationCaptureSession::manifest() const {
    return impl_->writer.manifest();
}

const std::filesystem::path& LiveVerificationCaptureSession::current_manifest_path() const {
    return impl_->writer.current_manifest_path();
}

} // namespace vh
