#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "visual_homing/gray8_resize_preprocessor.hpp"
#include "visual_homing/live_verification_capture.hpp"
#include "visual_homing/pi_camera_source.hpp"
#include "visual_homing/route_descriptor_index.hpp"
#include "visual_homing/route_package_builder.hpp"
#include "visual_homing/route_signature_entry.hpp"
#include "visual_homing/time.hpp"

namespace {

std::uint32_t positive_u32(const char* text, const char* name) {
    const auto value = std::stoull(text);
    if (value == 0 || value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument(std::string(name) + " must be a positive uint32");
    }
    return static_cast<std::uint32_t>(value);
}

double positive_double(const char* text, const char* name) {
    const auto value = std::stod(text);
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and positive");
    }
    return value;
}

std::uint16_t dimension(const char* text, const char* name) {
    const auto value = positive_u32(text, name);
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument(std::string(name) + " exceeds uint16");
    }
    return static_cast<std::uint16_t>(value);
}

std::uint64_t seconds_to_ns(double seconds) {
    const auto value = seconds * 1'000'000'000.0;
    if (value > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        throw std::invalid_argument("capture interval exceeds uint64 nanoseconds");
    }
    return static_cast<std::uint64_t>(std::llround(value));
}

std::optional<vh::Frame> wait_for_frame(vh::PiCameraSource& source, double timeout_ms) {
    const auto started = vh::now();
    while (vh::milliseconds_between(started, vh::now()) < timeout_ms) {
        if (auto frame = source.poll()) {
            return frame;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

std::filesystem::path create_source_package(
    const std::filesystem::path& directory,
    const std::string& profile_id,
    const vh::Frame& native_frame,
    double altitude_m) {
    vh::Gray8ResizePreprocessor preprocessor(160, 100);
    const auto tracking_frame = preprocessor.process(native_frame);
    vh::NavigationEstimate navigation;
    navigation.timestamp = native_frame.timestamp;
    navigation.altitude_m = altitude_m;
    navigation.confidence = 1.0;

    vh::RoutePackageManifest manifest;
    manifest.route_id = "live-verification-capture-benchmark";
    manifest.route_frame = "ROUTE_FRD";
    manifest.local_frame_id = "benchmark-local-frame";
    manifest.local_frame_revision = "unlocalized-v1";
    manifest.local_frame_convention = "LOCAL_ENU";
    manifest.camera.profile_id = profile_id;
    manifest.camera.sensor_type = vh::CameraSensorType::Visible;
    manifest.camera.pixel_format = vh::PixelFormat::Gray8;
    manifest.camera.capture_width = static_cast<std::uint16_t>(native_frame.width);
    manifest.camera.capture_height = static_cast<std::uint16_t>(native_frame.height);
    manifest.camera.horizontal_fov_rad = 2.79;
    manifest.camera.vertical_fov_rad = 2.18;
    manifest.layers.push_back({
        .id = "tracking-160x100",
        .role = vh::RouteLayerRole::Tracking,
        .camera_profile_id = profile_id,
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 160,
        .height = 100,
        .minimum_altitude_m = 0.0,
        .maximum_altitude_m = 1000.0,
    });

    vh::RoutePackageBuilder builder({
        .package_directory = directory,
        .manifest_template = manifest,
        .tracking_layer_id = "tracking-160x100",
        .maximum_entries_per_chunk = 64,
        .checkpoint_interval_entries = 1,
        .recover_existing_recording = false,
    });
    builder.append(vh::make_route_signature_entry(tracking_frame, navigation));
    builder.finalize();

    vh::RouteDescriptorIndexBuildConfig index_config;
    index_config.input_manifest_path = directory / "route.vhrm";
    index_config.output_manifest_path = directory / "route-indexed.vhrm";
    index_config.index_relative_path = "index/coarse-v1.vhix";
    index_config.grid_width = 16;
    index_config.grid_height = 10;
    (void)vh::build_route_descriptor_index_package(index_config);
    return index_config.output_manifest_path;
}

vh::LiveVerificationCaptureConfig capture_config(
    const std::filesystem::path& source_manifest,
    const std::string& profile_id,
    std::uint16_t width,
    std::uint16_t height,
    double interval_seconds,
    double duration_seconds) {
    const auto interval_ns = seconds_to_ns(interval_seconds);
    const auto maximum_keyframes = static_cast<std::uint32_t>(
        std::min(4096.0, std::ceil(duration_seconds / interval_seconds) + 2.0));
    vh::VerificationPackageWriterConfig writer;
    writer.source_manifest_path = source_manifest;
    writer.output_manifest_base_path = source_manifest.parent_path() / "route-verification.vhrm";
    writer.verification_layer = {
        .id = "verification-native",
        .role = vh::RouteLayerRole::Verification,
        .camera_profile_id = profile_id,
        .pixel_format = vh::PixelFormat::Gray8,
        .width = width,
        .height = height,
        .minimum_altitude_m = 0.0,
        .maximum_altitude_m = 1000.0,
    };
    writer.search_index_id = "coarse-index-v1";
    writer.maximum_keyframes = maximum_keyframes;
    writer.selector = {
        .descriptor_dimensions = 160,
        .nominal_route_length_m = 1000.0,
        .minimum_capture_interval_ns = interval_ns,
        .maximum_capture_interval_ns = interval_ns,
        .minimum_displacement_m = 1'000'000.0,
        .minimum_altitude_change_m = 1'000'000.0,
        .minimum_scale_log_change = 100.0,
        .minimum_yaw_change_rad = 3.14159265358979323846,
        .minimum_scene_novelty = 1.0,
        .minimum_gate_separation_m = 1000.0,
        .minimum_gate_scene_novelty = 1.0,
        .maximum_gate_position_uncertainty_m = 0.0,
        .minimum_gate_approach_margin_m = 1.0,
    };
    return {.writer = std::move(writer)};
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 9 && argc != 10) {
        std::cerr
            << "usage: live_verification_capture_benchmark OUTPUT_DIRECTORY PROFILE_ID"
            << " WIDTH HEIGHT FPS DURATION_SECONDS WARMUP_FRAMES CAPTURE_INTERVAL_SECONDS"
            << " [ALTITUDE_M]\n";
        return 2;
    }
    try {
        const std::filesystem::path output_directory = argv[1];
        const std::string profile_id = argv[2];
        const auto width = dimension(argv[3], "width");
        const auto height = dimension(argv[4], "height");
        const auto fps = positive_u32(argv[5], "fps");
        const auto duration_seconds = positive_double(argv[6], "duration_seconds");
        const auto warmup_frames = positive_u32(argv[7], "warmup_frames");
        const auto capture_interval_seconds = positive_double(argv[8], "capture_interval_seconds");
        const auto altitude_m = argc == 10 ? positive_double(argv[9], "altitude_m") : 1.0;
        if (profile_id.empty()) {
            throw std::invalid_argument("profile_id must not be empty");
        }
        if (capture_interval_seconds > duration_seconds) {
            throw std::invalid_argument("capture interval must not exceed benchmark duration");
        }
        if (std::filesystem::exists(output_directory)) {
            throw std::invalid_argument("output directory already exists");
        }
        std::filesystem::create_directories(output_directory);

        vh::PiCameraSource camera({
            .width = width,
            .height = height,
            .frame_rate_hz = static_cast<int>(fps),
            .format = vh::PixelFormat::Gray8,
            .enable_live_capture = true,
        });
        std::cout
            << "live_verification_benchmark_start"
            << " flight_authority=false fc_uart=false mavlink_output=false"
            << " width=" << width
            << " height=" << height
            << " requested_fps=" << fps
            << " duration_seconds=" << duration_seconds
            << " capture_interval_seconds=" << capture_interval_seconds
            << " output=" << output_directory.string() << '\n';
        if (!camera.start()) {
            throw std::runtime_error("camera start failed: " + camera.last_error());
        }

        std::uint64_t frames_seen = 0;
        for (std::uint32_t index = 0; index < warmup_frames; ++index) {
            if (!wait_for_frame(camera, 5000.0)) {
                camera.stop();
                throw std::runtime_error("camera warmup timed out");
            }
            ++frames_seen;
        }
        auto first = wait_for_frame(camera, 5000.0);
        if (!first) {
            camera.stop();
            throw std::runtime_error("first benchmark frame timed out");
        }
        ++frames_seen;
        const auto source_manifest = create_source_package(
            output_directory, profile_id, *first, altitude_m);
        vh::LiveVerificationCaptureSession session(capture_config(
            source_manifest,
            profile_id,
            width,
            height,
            capture_interval_seconds,
            duration_seconds));
        vh::LiveVerificationFrameContext context;
        context.health_ready = true;
        context.altitude_m = altitude_m;

        const auto started = vh::now();
        auto result = session.observe(*first, context);
        if (result.publication) {
            std::cout
                << "live_verification_publication"
                << " index=" << result.publication->publication_index
                << " frame_id=" << first->id
                << " trigger=" << vh::verification_trigger_mask_to_string(result.decision.trigger_mask)
                << " latency_ms=" << result.publication_latency_ms
                << " manifest=" << result.publication->manifest_path.string() << '\n';
        }

        std::uint64_t empty_polls = 0;
        while (vh::milliseconds_between(started, vh::now()) < duration_seconds * 1000.0) {
            auto frame = camera.poll();
            if (!frame) {
                ++empty_polls;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            ++frames_seen;
            result = session.observe(*frame, context);
            if (result.publication) {
                std::cout
                    << "live_verification_publication"
                    << " index=" << result.publication->publication_index
                    << " frame_id=" << frame->id
                    << " trigger=" << vh::verification_trigger_mask_to_string(result.decision.trigger_mask)
                    << " descriptor_latency_ms=" << result.descriptor_latency_ms
                    << " selector_latency_ms=" << result.selector_latency_ms
                    << " publication_latency_ms=" << result.publication_latency_ms
                    << " files_checked=" << result.publication->package_files_checked
                    << " manifest=" << result.publication->manifest_path.string() << '\n';
            }
        }
        camera.stop();

        const auto metrics = session.metrics();
        const auto descriptor_average = metrics.frames_observed == 0 ? 0.0
            : metrics.total_descriptor_latency_ms / static_cast<double>(metrics.frames_observed);
        const auto selector_average = metrics.frames_observed == 0 ? 0.0
            : metrics.total_selector_latency_ms / static_cast<double>(metrics.frames_observed);
        const auto publication_average = metrics.publications == 0 ? 0.0
            : metrics.total_publication_latency_ms / static_cast<double>(metrics.publications);
        const auto elapsed_total_ms = vh::milliseconds_between(started, vh::now());
        const auto effective_observed_fps = elapsed_total_ms <= 0.0 ? 0.0
            : static_cast<double>(metrics.frames_observed) * 1000.0 / elapsed_total_ms;
        std::cout
            << "live_verification_benchmark_done"
            << " passed=" << (metrics.publications > 0 ? "true" : "false")
            << " flight_authority=false fc_uart=false mavlink_output=false"
            << " frames_seen=" << frames_seen
            << " frames_observed=" << metrics.frames_observed
            << " effective_observed_fps=" << effective_observed_fps
            << " empty_polls=" << empty_polls
            << " invalid_observations=" << metrics.invalid_observations
            << " capture_requests=" << metrics.capture_requests
            << " publications=" << metrics.publications
            << " processing_failures=" << metrics.processing_failures
            << " publication_failures=" << metrics.publication_failures
            << " gates_published=" << metrics.gates_published
            << " output_bytes_published=" << metrics.output_bytes_published
            << " package_files_checked=" << metrics.package_files_checked
            << " descriptor_latency_ms_avg=" << descriptor_average
            << " descriptor_latency_ms_max=" << metrics.maximum_descriptor_latency_ms
            << " selector_latency_ms_avg=" << selector_average
            << " selector_latency_ms_max=" << metrics.maximum_selector_latency_ms
            << " publication_latency_ms_avg=" << publication_average
            << " publication_latency_ms_max=" << metrics.maximum_publication_latency_ms
            << " elapsed_ms=" << elapsed_total_ms
            << " latest_manifest=" << session.current_manifest_path().string()
            << '\n';
        return metrics.publications > 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr
            << "live_verification_benchmark_failed"
            << " flight_authority=false fc_uart=false mavlink_output=false"
            << " error=" << error.what() << '\n';
        return 1;
    }
}
