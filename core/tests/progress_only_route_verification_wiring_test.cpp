#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/route_descriptor_index.hpp"
#include "visual_homing/route_package_builder.hpp"

namespace {

std::filesystem::path unique_directory() {
    return std::filesystem::temp_directory_path()
        / ("visual_homing_progress_only_wiring_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

vh::RouteSignatureEntry entry(std::uint64_t id) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = id * 1'000'000U;
    result.width = 4;
    result.height = 4;
    result.format = vh::PixelFormat::Gray8;
    result.payload = {
        10, 10, 30, 30,
        10, 10, 30, 30,
        50, 50, 70, 70,
        50, 50, 70, 70,
    };
    return result;
}

vh::LiveRouteMatchingConfig valid_config(const std::filesystem::path& directory) {
    vh::RoutePackageManifest manifest;
    manifest.route_id = "progress-only-test-route";
    manifest.camera.profile_id = "ov9281-test";
    manifest.camera.sensor_type = vh::CameraSensorType::Visible;
    manifest.camera.pixel_format = vh::PixelFormat::Gray8;
    manifest.camera.capture_width = 4;
    manifest.camera.capture_height = 4;
    manifest.camera.horizontal_fov_rad = 1.0;
    manifest.camera.vertical_fov_rad = 1.0;
    manifest.layers = {{
        .id = "tracking-4x4",
        .role = vh::RouteLayerRole::Tracking,
        .camera_profile_id = "ov9281-test",
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 4,
        .height = 4,
        .minimum_altitude_m = 0.4,
        .maximum_altitude_m = 3.0,
    }};
    vh::RoutePackageBuilder builder({
        .package_directory = directory,
        .manifest_template = std::move(manifest),
        .tracking_layer_id = "tracking-4x4",
        .maximum_entries_per_chunk = 8,
        .checkpoint_interval_entries = 1,
    });
    builder.append(entry(1));
    builder.finalize();
    const vh::RouteDescriptorIndexBuildConfig index_config{
        .input_manifest_path = directory / "route.vhrm",
        .output_manifest_path = directory / "route-indexed.vhrm",
        .index_relative_path = "index/coarse-v1.vhix",
        .index_id = "coarse-index-v1",
        .descriptor_layer_id = "global-descriptor-v1",
        .grid_width = 2,
        .grid_height = 2,
        .sample_stride = 1,
    };
    (void)vh::build_route_descriptor_index_package(index_config);

    vh::LiveRouteMatchingConfig result;
    result.camera.width = 4;
    result.camera.height = 4;
    result.camera_profile_id = "ov9281-test";
    result.route_path = directory / "tracking/chunk-0000.vhrs";
    result.use_live_telemetry_stream = true;
    result.require_live_telemetry_health = true;
    result.visual_scale_diagnostics = true;
    result.visual_scale_reference_altitude_m = 0.75;
    result.publish_progress_only_route_verification = true;
    result.route_verification_producer.require_mavlink_health = true;
    auto& writer = result.route_verification_publisher.capture.writer;
    writer.source_manifest_path = index_config.output_manifest_path;
    writer.output_manifest_base_path = directory / "route-verification.vhrm";
    writer.search_index_id = index_config.index_id;
    writer.verification_layer = {
        .id = "verification-native-v1",
        .role = vh::RouteLayerRole::Verification,
        .camera_profile_id = "ov9281-test",
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 4,
        .height = 4,
        .minimum_altitude_m = 0.4,
        .maximum_altitude_m = 3.0,
    };
    writer.selector.descriptor_dimensions = 4;
    writer.selector.nominal_route_length_m = 10.0;
    return result;
}

bool rejected(vh::LiveRouteMatchingConfig config) {
    try {
        vh::validate_progress_only_route_verification_config(config);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    const auto directory = unique_directory();
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    std::filesystem::create_directories(directory);

    const auto config = valid_config(directory);
    vh::validate_progress_only_route_verification_config(config);

    auto invalid = config;
    invalid.use_live_telemetry_stream = false;
    assert(rejected(invalid));

    invalid = config;
    invalid.emit_external_nav_estimates = true;
    assert(rejected(invalid));

    invalid = config;
    invalid.route_verification_producer.local_frame_id = "forbidden";
    assert(rejected(invalid));

    invalid = config;
    invalid.route_verification_publisher.capture.writer.selector.descriptor_dimensions = 5;
    assert(rejected(invalid));

    invalid = config;
    invalid.route_path = directory / "different.vhrs";
    assert(rejected(invalid));

    vh::RouteMatch match;
    match.timestamp = vh::Timestamp(std::chrono::milliseconds(1000));
    match.direction_error_rad = 0.12;
    match.direction_observation_valid = true;
    match.confidence = 0.9;
    match.valid = true;
    vh::HealthSnapshot health;
    health.timestamp = vh::Timestamp(std::chrono::milliseconds(1010));
    health.camera_ok = true;
    health.mavlink_ok = true;
    health.navigation_ok = true;
    health.route_match_confidence = 0.9;
    const auto observation = vh::make_progress_only_live_route_verification_observation(
        match,
        0.4,
        health,
        {true, vh::Timestamp(std::chrono::milliseconds(1005)), 0.75},
        {true, vh::Timestamp(std::chrono::milliseconds(1000)), 1.05});
    assert(!observation.local_pose);
    assert(observation.yaw.valid);
    assert(observation.yaw.timestamp == match.timestamp);
    assert(observation.yaw.value == match.direction_error_rad);

    std::filesystem::remove_all(directory, cleanup_error);
    return 0;
}
