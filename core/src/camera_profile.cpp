#include "visual_homing/camera_profile.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vh {
namespace {

void reject_non_positive(double value, const char* name) {
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(name);
    }
}

void reject_negative(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(name);
    }
}

void reject_fraction(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
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

std::string json_escape(const std::string& value) {
    std::ostringstream output;
    for (const char character : value) {
        switch (character) {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                output << "\\u"
                       << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(character))
                       << std::dec << std::setfill(' ');
            } else {
                output << character;
            }
            break;
        }
    }
    return output.str();
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

CameraGroundFootprint derive_camera_ground_footprint(const CameraProfile& profile, double altitude_m) {
    validate_camera_profile(profile);
    reject_non_positive(altitude_m, "Camera ground footprint altitude_m must be positive");

    const auto ground_width_m = 2.0 * altitude_m * std::tan(profile.horizontal_fov_rad / 2.0);
    const auto ground_height_m = 2.0 * altitude_m * std::tan(profile.vertical_fov_rad / 2.0);
    if (!std::isfinite(ground_width_m) || !std::isfinite(ground_height_m) ||
        ground_width_m <= 0.0 || ground_height_m <= 0.0) {
        throw std::invalid_argument("Camera ground footprint FOV produced an invalid footprint");
    }

    return {
        .altitude_m = altitude_m,
        .ground_width_m = ground_width_m,
        .ground_height_m = ground_height_m,
        .meters_per_capture_pixel_x = ground_width_m / static_cast<double>(profile.capture_width),
        .meters_per_capture_pixel_y = ground_height_m / static_cast<double>(profile.capture_height),
        .meters_per_target_pixel_x = ground_width_m / static_cast<double>(profile.target_width),
        .meters_per_target_pixel_y = ground_height_m / static_cast<double>(profile.target_height),
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

std::vector<CameraProfileRecord> list_camera_profile_directory(const std::string& directory_path) {
    namespace fs = std::filesystem;

    if (!fs::exists(directory_path)) {
        throw std::runtime_error("Camera profile directory does not exist: " + directory_path);
    }
    if (!fs::is_directory(directory_path)) {
        throw std::runtime_error("Camera profile path is not a directory: " + directory_path);
    }

    std::vector<CameraProfileRecord> records;
    for (const auto& entry : fs::directory_iterator(directory_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".profile") {
            continue;
        }

        const auto path = entry.path().string();
        records.push_back(CameraProfileRecord{
            .path = path,
            .profile = load_camera_profile_file(path),
        });
    }

    std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
        if (left.profile.id == right.profile.id) {
            return left.path < right.path;
        }
        return left.profile.id < right.profile.id;
    });

    return records;
}

CameraProfileRecord load_camera_profile_by_id(const std::string& directory_path, const std::string& profile_id) {
    if (trim(profile_id).empty()) {
        throw std::invalid_argument("Camera profile id must not be empty");
    }

    const auto records = list_camera_profile_directory(directory_path);
    for (const auto& record : records) {
        if (record.profile.id == profile_id) {
            return record;
        }
    }

    throw std::runtime_error("Camera profile id not found: " + profile_id);
}

std::string load_active_camera_profile_id(const std::string& active_profile_path) {
    std::ifstream input(active_profile_path);
    if (!input) {
        throw std::runtime_error("Could not open active camera profile file for read: " + active_profile_path);
    }

    std::string id;
    std::getline(input, id);
    id = trim(id);
    if (id.empty()) {
        throw std::invalid_argument("Active camera profile id must not be empty: " + active_profile_path);
    }

    return id;
}

CameraProfileRecord load_active_camera_profile(const std::string& directory_path, const std::string& active_profile_path) {
    return load_camera_profile_by_id(directory_path, load_active_camera_profile_id(active_profile_path));
}

CameraProfileRecord set_active_camera_profile(const std::string& directory_path,
                                              const std::string& active_profile_path,
                                              const std::string& profile_id) {
    namespace fs = std::filesystem;

    const auto record = load_camera_profile_by_id(directory_path, profile_id);
    const auto parent = fs::path(active_profile_path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }

    std::ofstream output(active_profile_path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Could not open active camera profile file for write: " + active_profile_path);
    }
    output << record.profile.id << "\n";
    if (!output) {
        throw std::runtime_error("Could not write active camera profile file: " + active_profile_path);
    }

    return record;
}

std::string camera_profile_record_json(const CameraProfileRecord& record, bool active) {
    const auto scale = derive_camera_angular_scale(record.profile);

    std::ostringstream output;
    output << "{"
           << "\"id\":\"" << json_escape(record.profile.id) << "\","
           << "\"path\":\"" << json_escape(record.path) << "\","
           << "\"active\":" << (active ? "true" : "false") << ","
           << "\"sensor_type\":\"" << to_string(record.profile.sensor_type) << "\","
           << "\"pixel_format\":\"" << to_string(record.profile.pixel_format) << "\","
           << "\"capture_width\":" << record.profile.capture_width << ","
           << "\"capture_height\":" << record.profile.capture_height << ","
           << "\"target_width\":" << record.profile.target_width << ","
           << "\"target_height\":" << record.profile.target_height << ","
           << "\"horizontal_fov_rad\":" << record.profile.horizontal_fov_rad << ","
           << "\"vertical_fov_rad\":" << record.profile.vertical_fov_rad << ","
           << "\"radians_per_capture_pixel_x\":" << scale.radians_per_capture_pixel_x << ","
           << "\"radians_per_capture_pixel_y\":" << scale.radians_per_capture_pixel_y << ","
           << "\"radians_per_target_pixel_x\":" << scale.radians_per_target_pixel_x << ","
           << "\"radians_per_target_pixel_y\":" << scale.radians_per_target_pixel_y << ","
           << "\"low_texture_range_threshold\":" << record.profile.low_texture_range_threshold << ","
           << "\"ambiguous_mean_abs_diff_threshold\":" << record.profile.ambiguous_mean_abs_diff_threshold << ","
           << "\"maximum_low_texture_fraction\":" << record.profile.maximum_low_texture_fraction << ","
           << "\"maximum_ambiguous_nearest_fraction\":" << record.profile.maximum_ambiguous_nearest_fraction << ","
           << "\"minimum_average_nearest_mean_abs_diff\":" << record.profile.minimum_average_nearest_mean_abs_diff
           << "}";
    return output.str();
}

std::string camera_profile_registry_json(const std::vector<CameraProfileRecord>& records, const std::string& active_profile_id) {
    std::ostringstream output;
    output << "{"
           << "\"active_profile_id\":";
    if (active_profile_id.empty()) {
        output << "null";
    } else {
        output << "\"" << json_escape(active_profile_id) << "\"";
    }
    output << ",\"profiles\":[";
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (index != 0) {
            output << ",";
        }
        output << camera_profile_record_json(records[index], records[index].profile.id == active_profile_id);
    }
    output << "]}";
    return output.str();
}

std::string active_camera_profile_json(const CameraProfileRecord& record) {
    std::ostringstream output;
    output << "{"
           << "\"active_profile_id\":\"" << json_escape(record.profile.id) << "\","
           << "\"profile\":" << camera_profile_record_json(record, true)
           << "}";
    return output.str();
}

} // namespace vh
