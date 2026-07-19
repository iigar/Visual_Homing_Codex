#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "visual_homing/camera_profile.hpp"

namespace vh {

constexpr std::uint16_t route_package_manifest_format_version = 1;
constexpr std::uint8_t route_gate_direction_forward = 1U;
constexpr std::uint8_t route_gate_direction_reverse = 2U;

enum class RouteLayerRole : std::uint8_t {
    Tracking = 1,
    GlobalDescriptor = 2,
    Verification = 3,
};

struct RouteCameraCompatibility {
    std::string profile_id;
    CameraSensorType sensor_type = CameraSensorType::Visible;
    PixelFormat pixel_format = PixelFormat::Gray8;
    std::uint16_t capture_width = 0;
    std::uint16_t capture_height = 0;
    double horizontal_fov_rad = 0.0;
    double vertical_fov_rad = 0.0;
    double camera_to_body_x_m = 0.0;
    double camera_to_body_y_m = 0.0;
    double camera_to_body_z_m = 0.0;
    double camera_to_body_roll_rad = 0.0;
    double camera_to_body_pitch_rad = 0.0;
    double camera_to_body_yaw_rad = 0.0;
};

struct RouteLayerRecord {
    std::string id;
    RouteLayerRole role = RouteLayerRole::Tracking;
    std::string camera_profile_id;
    PixelFormat pixel_format = PixelFormat::Gray8;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    double minimum_altitude_m = 0.0;
    double maximum_altitude_m = 0.0;
};

struct RouteChunkRecord {
    std::string id;
    std::string layer_id;
    std::filesystem::path relative_path;
    std::uint16_t artifact_format_version = 1;
    std::uint64_t first_frame_id = 0;
    std::uint64_t last_frame_id = 0;
    std::uint64_t entry_count = 0;
    std::uint64_t byte_size = 0;
    std::string sha256;
};

struct RouteSearchIndexRecord {
    std::string id;
    std::string layer_id;
    std::filesystem::path relative_path;
    std::string descriptor_type;
    std::uint32_t descriptor_dimensions = 0;
    std::uint64_t item_count = 0;
    std::uint64_t byte_size = 0;
    std::string sha256;
};

struct RouteGateKeyframeRecord {
    std::string id;
    std::string verification_layer_id;
    std::string chunk_id;
    std::string search_index_id;
    std::uint64_t frame_id = 0;
    std::uint32_t route_segment_id = 0;
    double route_progress = 0.0;
    std::uint8_t allowed_directions = route_gate_direction_forward | route_gate_direction_reverse;
    bool has_local_pose = false;
    double local_x_m = 0.0;
    double local_y_m = 0.0;
    double local_z_m = 0.0;
    double local_yaw_rad = 0.0;
    double local_position_uncertainty_m = 0.0;
    double approach_radius_m = 0.0;
    double minimum_altitude_m = 0.0;
    double maximum_altitude_m = 0.0;
    double minimum_scale_ratio = 1.0;
    double maximum_scale_ratio = 1.0;
};

struct RoutePackageManifest {
    std::uint16_t version = route_package_manifest_format_version;
    std::string route_id;
    std::string route_frame = "ROUTE_FRD";
    std::string local_frame_id;
    std::string local_frame_revision;
    std::string local_frame_convention;
    RouteCameraCompatibility camera;
    std::vector<RouteLayerRecord> layers;
    std::vector<RouteChunkRecord> chunks;
    std::vector<RouteSearchIndexRecord> search_indexes;
    std::vector<RouteGateKeyframeRecord> gates;
};

struct RoutePackageVerificationResult {
    bool passed = false;
    std::uint64_t files_checked = 0;
    std::vector<std::string> errors;
};

void validate_route_package_manifest(const RoutePackageManifest& manifest);
void write_route_package_manifest(const std::filesystem::path& path, const RoutePackageManifest& manifest);
RoutePackageManifest read_route_package_manifest(const std::filesystem::path& path);
RoutePackageVerificationResult verify_route_package_files(
    const std::filesystem::path& manifest_path,
    const RoutePackageManifest& manifest);
std::string to_string(RouteLayerRole role);

} // namespace vh
