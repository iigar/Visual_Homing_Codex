#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "visual_homing/live_verification_capture.hpp"
#include "visual_homing/route_descriptor_index.hpp"
#include "visual_homing/route_package_builder.hpp"
#include "visual_homing/route_package_manifest.hpp"

namespace {

std::filesystem::path unique_directory(const std::string& label) {
    return std::filesystem::temp_directory_path()
        / ("visual_homing_live_verification_" + label + "_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void cleanup(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

vh::RouteSignatureEntry tracking_entry(std::uint64_t id, std::uint8_t pixel) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = id * 1000;
    result.altitude_band_m = 1;
    result.width = 4;
    result.height = 4;
    result.format = vh::PixelFormat::Gray8;
    result.payload.assign(16, pixel);
    return result;
}

std::filesystem::path make_source_package(const std::filesystem::path& directory) {
    cleanup(directory);
    std::filesystem::create_directories(directory);
    vh::RoutePackageManifest manifest;
    manifest.route_id = "live-verification-test";
    manifest.route_frame = "ROUTE_FRD";
    manifest.local_frame_id = "test-local";
    manifest.local_frame_revision = "rev-1";
    manifest.local_frame_convention = "LOCAL_ENU";
    manifest.camera.profile_id = "test-camera";
    manifest.camera.sensor_type = vh::CameraSensorType::Visible;
    manifest.camera.pixel_format = vh::PixelFormat::Gray8;
    manifest.camera.capture_width = 8;
    manifest.camera.capture_height = 6;
    manifest.camera.horizontal_fov_rad = 1.5;
    manifest.camera.vertical_fov_rad = 1.0;
    manifest.layers.push_back({
        .id = "tracking-4x4",
        .role = vh::RouteLayerRole::Tracking,
        .camera_profile_id = "test-camera",
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 4,
        .height = 4,
        .minimum_altitude_m = 0.0,
        .maximum_altitude_m = 10.0,
    });
    vh::RoutePackageBuilder builder({
        .package_directory = directory,
        .manifest_template = manifest,
        .tracking_layer_id = "tracking-4x4",
        .maximum_entries_per_chunk = 4,
        .checkpoint_interval_entries = 1,
        .recover_existing_recording = false,
    });
    builder.append(tracking_entry(1, 10));
    builder.append(tracking_entry(2, 20));
    builder.finalize();

    vh::RouteDescriptorIndexBuildConfig index_config;
    index_config.input_manifest_path = directory / "route.vhrm";
    index_config.output_manifest_path = directory / "route-indexed.vhrm";
    index_config.index_relative_path = "index/coarse-v1.vhix";
    index_config.grid_width = 2;
    index_config.grid_height = 2;
    (void)vh::build_route_descriptor_index_package(index_config);
    return index_config.output_manifest_path;
}

vh::LiveVerificationCaptureConfig session_config(
    const std::filesystem::path& source,
    std::uint32_t descriptor_dimensions = 4) {
    vh::VerificationPackageWriterConfig writer;
    writer.source_manifest_path = source;
    writer.output_manifest_base_path = source.parent_path() / "route-verification.vhrm";
    writer.verification_layer = {
        .id = "verification-8x6",
        .role = vh::RouteLayerRole::Verification,
        .camera_profile_id = "test-camera",
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 8,
        .height = 6,
        .minimum_altitude_m = 0.0,
        .maximum_altitude_m = 10.0,
    };
    writer.search_index_id = "coarse-index-v1";
    writer.maximum_keyframes = 4;
    writer.selector = {
        .descriptor_dimensions = descriptor_dimensions,
        .nominal_route_length_m = 100.0,
        .minimum_capture_interval_ns = 100,
        .maximum_capture_interval_ns = 1000,
        .minimum_displacement_m = 10.0,
        .minimum_altitude_change_m = 2.0,
        .minimum_scale_log_change = 0.5,
        .minimum_yaw_change_rad = 0.5,
        .minimum_scene_novelty = 0.5,
        .minimum_gate_separation_m = 20.0,
        .minimum_gate_scene_novelty = 0.5,
        .maximum_gate_position_uncertainty_m = 1.0,
        .minimum_gate_approach_margin_m = 1.0,
    };
    return {.writer = std::move(writer)};
}

vh::Frame frame(std::uint64_t id, std::uint64_t timestamp_ns, std::uint8_t offset = 0) {
    vh::Frame result;
    result.id = id;
    result.timestamp = vh::Timestamp(std::chrono::nanoseconds(timestamp_ns));
    result.width = 8;
    result.height = 6;
    result.format = vh::PixelFormat::Gray8;
    result.data.resize(48);
    for (std::size_t index = 0; index < result.data.size(); ++index) {
        result.data[index] = static_cast<std::uint8_t>(offset + index);
    }
    return result;
}

vh::LiveVerificationFrameContext healthy_context() {
    vh::LiveVerificationFrameContext result;
    result.health_ready = true;
    result.route_progress = 0.25;
    result.altitude_m = 1.0;
    result.scale_ratio = 1.0;
    result.yaw_rad = 0.0;
    return result;
}

} // namespace

int main() {
    {
        const auto directory = unique_directory("success");
        const auto source = make_source_package(directory);
        {
            vh::LiveVerificationCaptureSession session(session_config(source));
            const auto context = healthy_context();
            const auto first_native = frame(10, 10'000);
            const auto first = session.observe(first_native, context);
            assert(first.decision.valid && first.decision.request_native_capture);
            assert(first.publication && first.publication->publication_index == 1);
            assert(first.decision.trigger_mask == vh::verification_trigger_initial);
            const auto published_native = vh::read_route_signature_file(first.publication->chunk_path);
            assert(published_native.entries.size() == 1);
            assert(published_native.entries[0].frame_id == first_native.id);
            assert(published_native.entries[0].timestamp_ns == 10'000);
            assert(published_native.entries[0].payload == first_native.data);

            const auto too_soon = session.observe(frame(11, 10'050), context);
            assert(too_soon.decision.valid && !too_soon.decision.request_native_capture);
            assert(!too_soon.publication);

            const auto interval = session.observe(frame(12, 11'000), context);
            assert(interval.decision.valid && interval.decision.request_native_capture);
            assert((interval.decision.trigger_mask & vh::verification_trigger_maximum_interval) != 0);
            assert(interval.publication && interval.publication->publication_index == 2);

            auto unhealthy = context;
            unhealthy.health_ready = false;
            const auto rejected = session.observe(frame(13, 12'000), unhealthy);
            assert(!rejected.decision.valid);
            assert(rejected.decision.invalid_reason == "health_not_ready");
            assert(!rejected.publication);

            const auto metrics = session.metrics();
            assert(metrics.frames_observed == 4);
            assert(metrics.invalid_observations == 1);
            assert(metrics.capture_requests == 2);
            assert(metrics.publications == 2);
            assert(metrics.processing_failures == 0);
            assert(metrics.publication_failures == 0);
            assert(metrics.output_bytes_published > 0);
            assert(metrics.package_files_checked > 0);
            assert(session.current_manifest_path().filename() == "route-verification-0002.vhrm");
            const auto manifest = vh::read_route_package_manifest(session.current_manifest_path());
            assert(vh::verify_route_package_files(session.current_manifest_path(), manifest).passed);
        }
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("publication_retry");
        const auto source = make_source_package(directory);
        {
            vh::LiveVerificationCaptureSession session(session_config(source));
            std::filesystem::create_directories(directory / "verification");
            {
                std::ofstream collision(directory / "verification/chunk-0000.vhrs", std::ios::binary);
                collision.put('x');
            }
            const auto native = frame(10, 10'000);
            const auto context = healthy_context();
            bool rejected = false;
            try {
                (void)session.observe(native, context);
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            assert(rejected);
            assert(session.metrics().capture_requests == 1);
            assert(session.metrics().publications == 0);
            assert(session.metrics().processing_failures == 1);
            assert(session.metrics().publication_failures == 1);
            std::filesystem::remove(directory / "verification/chunk-0000.vhrs");
            const auto retried = session.observe(native, context);
            assert(retried.publication && retried.publication->publication_index == 1);
            assert(session.metrics().publications == 1);
        }
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("invalid_frame");
        const auto source = make_source_package(directory);
        {
            vh::LiveVerificationCaptureSession session(session_config(source));
            auto malformed = frame(10, 10'000);
            malformed.data.pop_back();
            bool rejected = false;
            try {
                (void)session.observe(malformed, healthy_context());
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            assert(rejected);
            assert(session.metrics().processing_failures == 1);
            assert(session.metrics().capture_requests == 0);
            assert(session.metrics().publications == 0);
        }
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("descriptor_mismatch");
        const auto source = make_source_package(directory);
        bool rejected = false;
        try {
            vh::LiveVerificationCaptureSession session(session_config(source, 5));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        cleanup(directory);
    }
    return 0;
}
