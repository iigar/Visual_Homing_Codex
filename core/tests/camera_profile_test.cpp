#include <cassert>
#include <stdexcept>

#include "visual_homing/camera_profile.hpp"

namespace {

bool close_to(double left, double right) {
    const auto delta = left > right ? left - right : right - left;
    return delta < 1.0e-12;
}

} // namespace

int main() {
    vh::CameraProfile profile;
    profile.id = "imx219-test";
    profile.capture_width = 320;
    profile.capture_height = 240;
    profile.target_width = 32;
    profile.target_height = 24;
    profile.horizontal_fov_rad = 1.2;
    profile.vertical_fov_rad = 0.9;

    vh::validate_camera_profile(profile);
    const auto scale = vh::derive_camera_angular_scale(profile);
    assert(close_to(scale.radians_per_capture_pixel_x, 1.2 / 320.0));
    assert(close_to(scale.radians_per_capture_pixel_y, 0.9 / 240.0));
    assert(close_to(scale.radians_per_target_pixel_x, 1.2 / 32.0));
    assert(close_to(scale.radians_per_target_pixel_y, 0.9 / 24.0));

    vh::CameraProfile thermal = profile;
    thermal.id = "thermal-test";
    thermal.sensor_type = vh::CameraSensorType::Thermal;
    thermal.pixel_format = vh::PixelFormat::Thermal16;
    thermal.capture_width = 256;
    thermal.capture_height = 192;
    thermal.target_width = 32;
    thermal.target_height = 24;
    vh::validate_camera_profile(thermal);

    bool rejected_empty_id = false;
    try {
        auto invalid = profile;
        invalid.id.clear();
        vh::validate_camera_profile(invalid);
    } catch (const std::invalid_argument&) {
        rejected_empty_id = true;
    }
    assert(rejected_empty_id);

    bool rejected_dimensions = false;
    try {
        auto invalid = profile;
        invalid.target_width = 0;
        vh::validate_camera_profile(invalid);
    } catch (const std::invalid_argument&) {
        rejected_dimensions = true;
    }
    assert(rejected_dimensions);

    bool rejected_fov = false;
    try {
        auto invalid = profile;
        invalid.horizontal_fov_rad = 0.0;
        vh::validate_camera_profile(invalid);
    } catch (const std::invalid_argument&) {
        rejected_fov = true;
    }
    assert(rejected_fov);

    bool rejected_fraction = false;
    try {
        auto invalid = profile;
        invalid.maximum_ambiguous_nearest_fraction = 1.5;
        vh::validate_camera_profile(invalid);
    } catch (const std::invalid_argument&) {
        rejected_fraction = true;
    }
    assert(rejected_fraction);

    bool rejected_threshold = false;
    try {
        auto invalid = profile;
        invalid.minimum_average_nearest_mean_abs_diff = -1.0;
        vh::validate_camera_profile(invalid);
    } catch (const std::invalid_argument&) {
        rejected_threshold = true;
    }
    assert(rejected_threshold);

    return 0;
}
