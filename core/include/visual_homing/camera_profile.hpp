#pragma once

#include <string>

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

CameraAngularScale derive_camera_angular_scale(const CameraProfile& profile);
void validate_camera_profile(const CameraProfile& profile);

} // namespace vh
