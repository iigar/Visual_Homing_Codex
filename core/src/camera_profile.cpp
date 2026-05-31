#include "visual_homing/camera_profile.hpp"

#include <stdexcept>

namespace vh {
namespace {

void reject_non_positive(double value, const char* name) {
    if (value <= 0.0) {
        throw std::invalid_argument(name);
    }
}

void reject_negative(double value, const char* name) {
    if (value < 0.0) {
        throw std::invalid_argument(name);
    }
}

void reject_fraction(double value, const char* name) {
    if (value < 0.0 || value > 1.0) {
        throw std::invalid_argument(name);
    }
}

} // namespace

void validate_camera_profile(const CameraProfile& profile) {
    if (profile.id.empty()) {
        throw std::invalid_argument("CameraProfile id must not be empty");
    }
    if (profile.capture_width <= 0 || profile.capture_height <= 0) {
        throw std::invalid_argument("CameraProfile capture dimensions must be positive");
    }
    if (profile.target_width <= 0 || profile.target_height <= 0) {
        throw std::invalid_argument("CameraProfile target dimensions must be positive");
    }

    reject_non_positive(profile.horizontal_fov_rad, "CameraProfile horizontal_fov_rad must be positive");
    reject_non_positive(profile.vertical_fov_rad, "CameraProfile vertical_fov_rad must be positive");
    reject_negative(profile.low_texture_range_threshold, "CameraProfile low_texture_range_threshold must be non-negative");
    reject_negative(profile.ambiguous_mean_abs_diff_threshold, "CameraProfile ambiguous_mean_abs_diff_threshold must be non-negative");
    reject_fraction(profile.maximum_low_texture_fraction, "CameraProfile maximum_low_texture_fraction must be between 0 and 1");
    reject_fraction(profile.maximum_ambiguous_nearest_fraction, "CameraProfile maximum_ambiguous_nearest_fraction must be between 0 and 1");
    reject_negative(profile.minimum_average_nearest_mean_abs_diff, "CameraProfile minimum_average_nearest_mean_abs_diff must be non-negative");
}

CameraAngularScale derive_camera_angular_scale(const CameraProfile& profile) {
    validate_camera_profile(profile);

    return {
        .radians_per_capture_pixel_x = profile.horizontal_fov_rad / static_cast<double>(profile.capture_width),
        .radians_per_capture_pixel_y = profile.vertical_fov_rad / static_cast<double>(profile.capture_height),
        .radians_per_target_pixel_x = profile.horizontal_fov_rad / static_cast<double>(profile.target_width),
        .radians_per_target_pixel_y = profile.vertical_fov_rad / static_cast<double>(profile.target_height),
    };
}

} // namespace vh
