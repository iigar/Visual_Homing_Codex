#pragma once

#include <string>
#include <vector>

#include "visual_homing/frame.hpp"

namespace vh {

enum class CameraSensorType {
    Visible,
    Thermal,
    Other
};

struct CameraProfile {
    std::string id = "generic-gray8";
    CameraSensorType sensor_type = CameraSensorType::Visible;
    PixelFormat pixel_format = PixelFormat::Gray8;
    int capture_width = 320;
    int capture_height = 240;
    int target_width = 32;
    int target_height = 24;
    double horizontal_fov_rad = 1.0;
    double vertical_fov_rad = 0.75;
    double low_texture_range_threshold = 4.0;
    double ambiguous_mean_abs_diff_threshold = 2.0;
    double maximum_low_texture_fraction = 0.05;
    double maximum_ambiguous_nearest_fraction = 0.10;
    double minimum_average_nearest_mean_abs_diff = 5.0;
};

struct CameraAngularScale {
    double radians_per_capture_pixel_x = 0.0;
    double radians_per_capture_pixel_y = 0.0;
    double radians_per_target_pixel_x = 0.0;
    double radians_per_target_pixel_y = 0.0;
};

struct CameraProfileRecord {
    std::string path;
    CameraProfile profile;
};

CameraAngularScale derive_camera_angular_scale(const CameraProfile& profile);
void validate_camera_profile(const CameraProfile& profile);
CameraProfile load_camera_profile_file(const std::string& path);
std::vector<CameraProfileRecord> list_camera_profile_directory(const std::string& directory_path);
CameraProfileRecord load_camera_profile_by_id(const std::string& directory_path, const std::string& profile_id);
std::string load_active_camera_profile_id(const std::string& active_profile_path);
CameraProfileRecord load_active_camera_profile(const std::string& directory_path, const std::string& active_profile_path);
CameraProfileRecord set_active_camera_profile(const std::string& directory_path, const std::string& active_profile_path, const std::string& profile_id);
std::string camera_profile_record_json(const CameraProfileRecord& record, bool active);
std::string camera_profile_registry_json(const std::vector<CameraProfileRecord>& records, const std::string& active_profile_id);
std::string active_camera_profile_json(const CameraProfileRecord& record);
std::string to_string(CameraSensorType sensor_type);
std::string to_string(PixelFormat pixel_format);

} // namespace vh
