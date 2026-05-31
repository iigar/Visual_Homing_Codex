#include <cassert>
#include <filesystem>
#include <fstream>
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
    assert(vh::to_string(profile.sensor_type) == "Visible");
    assert(vh::to_string(profile.pixel_format) == "Gray8");
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
    assert(vh::to_string(thermal.sensor_type) == "Thermal");
    assert(vh::to_string(thermal.pixel_format) == "Thermal16");

    const auto profile_path = std::filesystem::temp_directory_path() / "visual_homing_camera_profile_test.profile";
    {
        std::ofstream output(profile_path);
        output << "# Visual Homing camera profile\n";
        output << "id = file-imx219\n";
        output << "sensor_type = Visible\n";
        output << "pixel_format = Gray8\n";
        output << "capture_width = 320\n";
        output << "capture_height = 240\n";
        output << "target_width = 32\n";
        output << "target_height = 24\n";
        output << "horizontal_fov_rad = 1.1\n";
        output << "vertical_fov_rad = 0.8\n";
        output << "maximum_ambiguous_nearest_fraction = 0.2\n";
    }

    const auto loaded = vh::load_camera_profile_file(profile_path.string());
    assert(loaded.id == "file-imx219");
    assert(loaded.sensor_type == vh::CameraSensorType::Visible);
    assert(loaded.pixel_format == vh::PixelFormat::Gray8);
    assert(loaded.capture_width == 320);
    assert(loaded.capture_height == 240);
    assert(loaded.target_width == 32);
    assert(loaded.target_height == 24);
    assert(close_to(loaded.horizontal_fov_rad, 1.1));
    assert(close_to(loaded.vertical_fov_rad, 0.8));
    assert(close_to(loaded.maximum_ambiguous_nearest_fraction, 0.2));

    const auto invalid_profile_path = std::filesystem::temp_directory_path() / "visual_homing_camera_profile_invalid.profile";
    {
        std::ofstream output(invalid_profile_path);
        output << "id = bad-profile\n";
        output << "capture_width = 320\n";
        output << "capture_height = 240\n";
        output << "target_width = 32\n";
        output << "target_height = 24\n";
        output << "horizontal_fov_rad = 1.1\n";
        output << "vertical_fov_rad = 0.8\n";
        output << "unexpected_key = nope\n";
    }

    bool rejected_unknown_key = false;
    try {
        (void)vh::load_camera_profile_file(invalid_profile_path.string());
    } catch (const std::invalid_argument&) {
        rejected_unknown_key = true;
    }
    assert(rejected_unknown_key);

    const auto registry_dir = std::filesystem::temp_directory_path() / "visual_homing_camera_profile_registry";
    std::filesystem::create_directories(registry_dir);
    const auto registry_profile_a = registry_dir / "b.profile";
    const auto registry_profile_b = registry_dir / "a.profile";
    const auto ignored_file = registry_dir / "ignored.txt";
    {
        std::ofstream output(registry_profile_a);
        output << "id = z-profile\n";
        output << "capture_width = 320\n";
        output << "capture_height = 240\n";
        output << "target_width = 32\n";
        output << "target_height = 24\n";
        output << "horizontal_fov_rad = 1.1\n";
        output << "vertical_fov_rad = 0.8\n";
    }
    {
        std::ofstream output(registry_profile_b);
        output << "id = a-profile\n";
        output << "capture_width = 160\n";
        output << "capture_height = 120\n";
        output << "target_width = 16\n";
        output << "target_height = 12\n";
        output << "horizontal_fov_rad = 0.9\n";
        output << "vertical_fov_rad = 0.6\n";
    }
    {
        std::ofstream output(ignored_file);
        output << "id = ignored\n";
    }

    const auto records = vh::list_camera_profile_directory(registry_dir.string());
    assert(records.size() == 2);
    assert(records[0].profile.id == "a-profile");
    assert(records[1].profile.id == "z-profile");

    const auto selected = vh::load_camera_profile_by_id(registry_dir.string(), "z-profile");
    assert(selected.profile.id == "z-profile");
    assert(selected.profile.target_width == 32);

    const auto active_path = registry_dir / "active_camera_profile.txt";
    const auto active_set = vh::set_active_camera_profile(registry_dir.string(), active_path.string(), "a-profile");
    assert(active_set.profile.id == "a-profile");
    assert(vh::load_active_camera_profile_id(active_path.string()) == "a-profile");
    const auto active_loaded = vh::load_active_camera_profile(registry_dir.string(), active_path.string());
    assert(active_loaded.profile.id == "a-profile");

    bool rejected_missing_profile = false;
    try {
        (void)vh::set_active_camera_profile(registry_dir.string(), active_path.string(), "missing-profile");
    } catch (const std::runtime_error&) {
        rejected_missing_profile = true;
    }
    assert(rejected_missing_profile);

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
