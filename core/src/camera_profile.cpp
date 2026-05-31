#include "visual_homing/camera_profile.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

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

std::string trim(std::string value) {
    auto begin = value.begin();
    while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }

    auto end = value.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    return std::string(begin, end);
}

int parse_int(const std::string& value, const std::string& key) {
    std::size_t consumed = 0;
    const auto parsed = std::stoi(value, &consumed);
    if (consumed != value.size()) {
        throw std::invalid_argument("Invalid integer for CameraProfile key: " + key);
    }
    return parsed;
}

double parse_double(const std::string& value, const std::string& key) {
    std::size_t consumed = 0;
    const auto parsed = std::stod(value, &consumed);
    if (consumed != value.size()) {
        throw std::invalid_argument("Invalid floating point value for CameraProfile key: " + key);
    }
    return parsed;
}

CameraSensorType parse_sensor_type(const std::string& value) {
    if (value == "Visible") {
        return CameraSensorType::Visible;
    }
    if (value == "Thermal") {
        return CameraSensorType::Thermal;
    }
    if (value == "Other") {
        return CameraSensorType::Other;
    }
    throw std::invalid_argument("Invalid CameraProfile sensor_type: " + value);
}

PixelFormat parse_pixel_format(const std::string& value) {
    if (value == "Gray8") {
        return PixelFormat::Gray8;
    }
    if (value == "Bgr8") {
        return PixelFormat::Bgr8;
    }
    if (value == "Thermal16") {
        return PixelFormat::Thermal16;
    }
    throw std::invalid_argument("Invalid CameraProfile pixel_format: " + value);
}

} // namespace

std::string to_string(CameraSensorType sensor_type) {
    switch (sensor_type) {
    case CameraSensorType::Visible:
        return "Visible";
    case CameraSensorType::Thermal:
        return "Thermal";
    case CameraSensorType::Other:
        return "Other";
    }

    return "Other";
}

std::string to_string(PixelFormat pixel_format) {
    switch (pixel_format) {
    case PixelFormat::Gray8:
        return "Gray8";
    case PixelFormat::Bgr8:
        return "Bgr8";
    case PixelFormat::Thermal16:
        return "Thermal16";
    }

    return "Gray8";
}

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

CameraProfile load_camera_profile_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Could not open camera profile file for read: " + path);
    }

    CameraProfile profile;
    bool saw_id = false;
    bool saw_capture_width = false;
    bool saw_capture_height = false;
    bool saw_target_width = false;
    bool saw_target_height = false;
    bool saw_horizontal_fov_rad = false;
    bool saw_vertical_fov_rad = false;

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (const auto comment = line.find('#'); comment != std::string::npos) {
            line.resize(comment);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::invalid_argument("Invalid CameraProfile line without '=' at " + path + ":" + std::to_string(line_number));
        }

        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if (key.empty() || value.empty()) {
            throw std::invalid_argument("Invalid empty CameraProfile key or value at " + path + ":" + std::to_string(line_number));
        }

        if (key == "id") {
            profile.id = value;
            saw_id = true;
        } else if (key == "sensor_type") {
            profile.sensor_type = parse_sensor_type(value);
        } else if (key == "pixel_format") {
            profile.pixel_format = parse_pixel_format(value);
        } else if (key == "capture_width") {
            profile.capture_width = parse_int(value, key);
            saw_capture_width = true;
        } else if (key == "capture_height") {
            profile.capture_height = parse_int(value, key);
            saw_capture_height = true;
        } else if (key == "target_width") {
            profile.target_width = parse_int(value, key);
            saw_target_width = true;
        } else if (key == "target_height") {
            profile.target_height = parse_int(value, key);
            saw_target_height = true;
        } else if (key == "horizontal_fov_rad") {
            profile.horizontal_fov_rad = parse_double(value, key);
            saw_horizontal_fov_rad = true;
        } else if (key == "vertical_fov_rad") {
            profile.vertical_fov_rad = parse_double(value, key);
            saw_vertical_fov_rad = true;
        } else if (key == "low_texture_range_threshold") {
            profile.low_texture_range_threshold = parse_double(value, key);
        } else if (key == "ambiguous_mean_abs_diff_threshold") {
            profile.ambiguous_mean_abs_diff_threshold = parse_double(value, key);
        } else if (key == "maximum_low_texture_fraction") {
            profile.maximum_low_texture_fraction = parse_double(value, key);
        } else if (key == "maximum_ambiguous_nearest_fraction") {
            profile.maximum_ambiguous_nearest_fraction = parse_double(value, key);
        } else if (key == "minimum_average_nearest_mean_abs_diff") {
            profile.minimum_average_nearest_mean_abs_diff = parse_double(value, key);
        } else {
            throw std::invalid_argument("Unknown CameraProfile key at " + path + ":" + std::to_string(line_number) + ": " + key);
        }
    }

    if (!saw_id || !saw_capture_width || !saw_capture_height || !saw_target_width || !saw_target_height ||
        !saw_horizontal_fov_rad || !saw_vertical_fov_rad) {
        throw std::invalid_argument("CameraProfile file is missing one or more required keys: " + path);
    }

    validate_camera_profile(profile);
    return profile;
}

} // namespace vh
