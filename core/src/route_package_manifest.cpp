#include "visual_homing/route_package_manifest.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "visual_homing/artifact_digest.hpp"

namespace vh {
namespace {

constexpr std::array<char, 4> manifest_magic = {'V', 'H', 'R', 'M'};
constexpr std::uint8_t little_endian = 1;
constexpr std::uint32_t maximum_layers = 16;
constexpr std::uint32_t maximum_chunks = 4096;
constexpr std::uint32_t maximum_search_indexes = 64;
constexpr std::uint32_t maximum_gates = 4096;
constexpr std::size_t maximum_identifier_length = 128;
constexpr std::size_t maximum_path_length = 1024;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

void require_finite(double value, const std::string& field) {
    require(std::isfinite(value), field + " must be finite");
}

void validate_identifier(const std::string& value, const std::string& field, bool allow_empty = false) {
    if (value.empty()) {
        require(allow_empty, field + " must not be empty");
        return;
    }
    require(value.size() <= maximum_identifier_length, field + " is too long");
    for (const auto character : value) {
        const bool valid = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '_' || character == '-' || character == '.';
        require(valid, field + " contains an unsupported character");
    }
}

void validate_relative_path(const std::filesystem::path& path, const std::string& field) {
    const auto value = path.generic_string();
    require(!value.empty(), field + " must not be empty");
    require(value.size() <= maximum_path_length, field + " is too long");
    require(!path.is_absolute() && !path.has_root_name() && !path.has_root_directory(), field + " must be relative");
    require(value.find('\\') == std::string::npos && value.find(':') == std::string::npos,
            field + " must use portable relative path syntax");
    require(value.front() != '/' && value.back() != '/', field + " must not start or end with '/'");
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto separator = value.find('/', start);
        const auto component = value.substr(start, separator == std::string::npos ? std::string::npos : separator - start);
        require(!component.empty() && component != "." && component != "..", field + " contains an unsafe component");
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
}

std::uint8_t sensor_type_to_u8(CameraSensorType type) {
    switch (type) {
    case CameraSensorType::Visible:
        return 1;
    case CameraSensorType::Thermal:
        return 2;
    case CameraSensorType::Other:
        return 3;
    }
    throw std::invalid_argument("Unsupported route manifest camera sensor type");
}

CameraSensorType sensor_type_from_u8(std::uint8_t value) {
    switch (value) {
    case 1:
        return CameraSensorType::Visible;
    case 2:
        return CameraSensorType::Thermal;
    case 3:
        return CameraSensorType::Other;
    default:
        throw std::runtime_error("Unsupported route manifest camera sensor type");
    }
}

std::uint8_t pixel_format_to_u8(PixelFormat format) {
    switch (format) {
    case PixelFormat::Gray8:
        return 1;
    case PixelFormat::Bgr8:
        return 2;
    case PixelFormat::Thermal16:
        return 3;
    }
    throw std::invalid_argument("Unsupported route manifest pixel format");
}

PixelFormat pixel_format_from_u8(std::uint8_t value) {
    switch (value) {
    case 1:
        return PixelFormat::Gray8;
    case 2:
        return PixelFormat::Bgr8;
    case 3:
        return PixelFormat::Thermal16;
    default:
        throw std::runtime_error("Unsupported route manifest pixel format");
    }
}

std::uint8_t layer_role_to_u8(RouteLayerRole role) {
    return static_cast<std::uint8_t>(role);
}

RouteLayerRole layer_role_from_u8(std::uint8_t value) {
    switch (value) {
    case 1:
        return RouteLayerRole::Tracking;
    case 2:
        return RouteLayerRole::GlobalDescriptor;
    case 3:
        return RouteLayerRole::Verification;
    default:
        throw std::runtime_error("Unsupported route manifest layer role");
    }
}

void write_u8(std::ostream& output, std::uint8_t value) {
    output.put(static_cast<char>(value));
}

void write_u16(std::ostream& output, std::uint16_t value) {
    write_u8(output, static_cast<std::uint8_t>(value & 0xffU));
    write_u8(output, static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void write_u32(std::ostream& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        write_u8(output, static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void write_u64(std::ostream& output, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        write_u8(output, static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void write_f64(std::ostream& output, double value) {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u64(output, bits);
}

void write_string(std::ostream& output, const std::string& value) {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("Route manifest string exceeds wire limit");
    }
    write_u16(output, static_cast<std::uint16_t>(value.size()));
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::uint8_t read_u8(std::istream& input) {
    const auto value = input.get();
    if (value == std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected end of route package manifest");
    }
    return static_cast<std::uint8_t>(value);
}

std::uint16_t read_u16(std::istream& input) {
    std::uint16_t value = read_u8(input);
    value |= static_cast<std::uint16_t>(read_u8(input)) << 8U;
    return value;
}

std::uint32_t read_u32(std::istream& input) {
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        value |= static_cast<std::uint32_t>(read_u8(input)) << shift;
    }
    return value;
}

std::uint64_t read_u64(std::istream& input) {
    std::uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        value |= static_cast<std::uint64_t>(read_u8(input)) << shift;
    }
    return value;
}

double read_f64(std::istream& input) {
    const auto bits = read_u64(input);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string read_string(std::istream& input, std::size_t maximum_size, const std::string& field) {
    const auto size = read_u16(input);
    if (size > maximum_size) {
        throw std::runtime_error("Route manifest " + field + " exceeds safety limit");
    }
    std::string value(size, '\0');
    input.read(value.data(), static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("Unexpected end of route package manifest string");
    }
    return value;
}

std::array<std::uint8_t, 32> digest_from_hex(const std::string& value) {
    if (!is_sha256_hex(value)) {
        throw std::invalid_argument("Invalid lowercase SHA-256 digest");
    }
    std::array<std::uint8_t, 32> digest{};
    auto nibble = [](char character) -> std::uint8_t {
        if (character >= '0' && character <= '9') {
            return static_cast<std::uint8_t>(character - '0');
        }
        return static_cast<std::uint8_t>(character - 'a' + 10);
    };
    for (std::size_t i = 0; i < digest.size(); ++i) {
        digest[i] = static_cast<std::uint8_t>((nibble(value[i * 2]) << 4U) | nibble(value[i * 2 + 1]));
    }
    return digest;
}

std::string digest_to_hex(const std::array<std::uint8_t, 32>& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string value;
    value.reserve(64);
    for (const auto byte : digest) {
        value.push_back(digits[(byte >> 4U) & 0x0fU]);
        value.push_back(digits[byte & 0x0fU]);
    }
    return value;
}

void write_digest(std::ostream& output, const std::string& value) {
    const auto digest = digest_from_hex(value);
    output.write(reinterpret_cast<const char*>(digest.data()), static_cast<std::streamsize>(digest.size()));
}

std::string read_digest(std::istream& input) {
    std::array<std::uint8_t, 32> digest{};
    input.read(reinterpret_cast<char*>(digest.data()), static_cast<std::streamsize>(digest.size()));
    if (!input) {
        throw std::runtime_error("Unexpected end of route package digest");
    }
    return digest_to_hex(digest);
}

void require_stream_ok(const std::ios& stream, const std::string& action) {
    if (!stream) {
        throw std::runtime_error("Failed to " + action + " route package manifest");
    }
}

bool path_has_prefix(const std::filesystem::path& path, const std::filesystem::path& prefix) {
    auto path_iterator = path.begin();
    for (auto prefix_iterator = prefix.begin(); prefix_iterator != prefix.end(); ++prefix_iterator, ++path_iterator) {
        if (path_iterator == path.end() || *path_iterator != *prefix_iterator) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string to_string(RouteLayerRole role) {
    switch (role) {
    case RouteLayerRole::Tracking:
        return "tracking";
    case RouteLayerRole::GlobalDescriptor:
        return "global_descriptor";
    case RouteLayerRole::Verification:
        return "verification";
    }
    return "unsupported";
}

void validate_route_package_manifest(const RoutePackageManifest& manifest) {
    require(manifest.version == route_package_manifest_format_version, "Unsupported route package manifest version");
    validate_identifier(manifest.route_id, "route_id");
    require(manifest.route_frame == "ROUTE_FRD", "Route package v1 requires route_frame=ROUTE_FRD");
    validate_identifier(manifest.local_frame_id, "local_frame_id", true);
    validate_identifier(manifest.local_frame_revision, "local_frame_revision", true);
    require(manifest.local_frame_id.empty() == manifest.local_frame_revision.empty(),
            "local_frame_id and local_frame_revision must be provided together");
    require(manifest.local_frame_id.empty() == manifest.local_frame_convention.empty(),
            "local_frame_id and local_frame_convention must be provided together");
    require(manifest.local_frame_convention.empty()
                || manifest.local_frame_convention == "LOCAL_ENU"
                || manifest.local_frame_convention == "LOCAL_NED",
            "local_frame_convention must be LOCAL_ENU or LOCAL_NED");

    validate_identifier(manifest.camera.profile_id, "camera.profile_id");
    require(manifest.camera.capture_width > 0 && manifest.camera.capture_height > 0,
            "Route package camera capture dimensions must be positive");
    require_finite(manifest.camera.horizontal_fov_rad, "camera.horizontal_fov_rad");
    require_finite(manifest.camera.vertical_fov_rad, "camera.vertical_fov_rad");
    require(manifest.camera.horizontal_fov_rad > 0.0 && manifest.camera.horizontal_fov_rad <= 3.2,
            "camera.horizontal_fov_rad is outside v1 range");
    require(manifest.camera.vertical_fov_rad > 0.0 && manifest.camera.vertical_fov_rad <= 3.2,
            "camera.vertical_fov_rad is outside v1 range");
    require_finite(manifest.camera.camera_to_body_x_m, "camera.camera_to_body_x_m");
    require_finite(manifest.camera.camera_to_body_y_m, "camera.camera_to_body_y_m");
    require_finite(manifest.camera.camera_to_body_z_m, "camera.camera_to_body_z_m");
    require_finite(manifest.camera.camera_to_body_roll_rad, "camera.camera_to_body_roll_rad");
    require_finite(manifest.camera.camera_to_body_pitch_rad, "camera.camera_to_body_pitch_rad");
    require_finite(manifest.camera.camera_to_body_yaw_rad, "camera.camera_to_body_yaw_rad");
    (void)sensor_type_to_u8(manifest.camera.sensor_type);
    (void)pixel_format_to_u8(manifest.camera.pixel_format);

    require(!manifest.layers.empty() && manifest.layers.size() <= maximum_layers,
            "Route package must contain 1..16 layers");
    require(manifest.chunks.size() <= maximum_chunks, "Route package has too many chunks");
    require(manifest.search_indexes.size() <= maximum_search_indexes, "Route package has too many search indexes");
    require(manifest.gates.size() <= maximum_gates, "Route package has too many gates");

    std::unordered_map<std::string, const RouteLayerRecord*> layers;
    std::size_t tracking_layers = 0;
    for (const auto& layer : manifest.layers) {
        validate_identifier(layer.id, "layer.id");
        validate_identifier(layer.camera_profile_id, "layer.camera_profile_id");
        require(layer.camera_profile_id == manifest.camera.profile_id,
                "Route package v1 layer camera profile must match package camera profile");
        require(layers.emplace(layer.id, &layer).second, "Duplicate route layer id: " + layer.id);
        require(layer.width > 0 && layer.height > 0, "Route layer dimensions must be positive");
        require_finite(layer.minimum_altitude_m, "layer.minimum_altitude_m");
        require_finite(layer.maximum_altitude_m, "layer.maximum_altitude_m");
        require(layer.minimum_altitude_m >= 0.0 && layer.maximum_altitude_m >= layer.minimum_altitude_m,
                "Route layer altitude envelope is invalid");
        (void)pixel_format_to_u8(layer.pixel_format);
        (void)layer_role_from_u8(layer_role_to_u8(layer.role));
        if (layer.role == RouteLayerRole::Tracking) {
            ++tracking_layers;
        }
    }
    require(tracking_layers == 1, "Route package v1 requires exactly one tracking layer");

    std::unordered_map<std::string, const RouteChunkRecord*> chunks;
    std::unordered_set<std::string> artifact_paths;
    std::unordered_set<std::string> layers_with_chunks;
    for (const auto& chunk : manifest.chunks) {
        validate_identifier(chunk.id, "chunk.id");
        validate_identifier(chunk.layer_id, "chunk.layer_id");
        require(chunks.emplace(chunk.id, &chunk).second, "Duplicate route chunk id: " + chunk.id);
        require(layers.contains(chunk.layer_id), "Route chunk references unknown layer: " + chunk.layer_id);
        validate_relative_path(chunk.relative_path, "chunk.relative_path");
        require(artifact_paths.insert(chunk.relative_path.generic_string()).second,
                "Duplicate route artifact path: " + chunk.relative_path.generic_string());
        require(chunk.artifact_format_version == 1, "Route package v1 chunks must use VHRS v1");
        require(chunk.entry_count > 0, "Route chunk entry_count must be positive");
        require(chunk.last_frame_id >= chunk.first_frame_id, "Route chunk frame range is invalid");
        require(chunk.entry_count <= chunk.last_frame_id - chunk.first_frame_id + 1,
                "Route chunk entry_count exceeds frame range");
        require(chunk.byte_size > 0, "Route chunk byte_size must be positive");
        require(is_sha256_hex(chunk.sha256), "Route chunk SHA-256 must be lowercase 64-character hex");
        layers_with_chunks.insert(chunk.layer_id);
    }
    for (const auto& layer : manifest.layers) {
        if (layer.role != RouteLayerRole::GlobalDescriptor) {
            require(layers_with_chunks.contains(layer.id), "Image layer has no chunks: " + layer.id);
        }
    }

    std::unordered_map<std::string, const RouteSearchIndexRecord*> indexes;
    for (const auto& index : manifest.search_indexes) {
        validate_identifier(index.id, "search_index.id");
        validate_identifier(index.layer_id, "search_index.layer_id");
        validate_identifier(index.descriptor_type, "search_index.descriptor_type");
        require(indexes.emplace(index.id, &index).second, "Duplicate route search index id: " + index.id);
        const auto layer = layers.find(index.layer_id);
        require(layer != layers.end(), "Route search index references unknown layer: " + index.layer_id);
        require(layer->second->role == RouteLayerRole::GlobalDescriptor,
                "Route search index must reference a global-descriptor layer");
        validate_relative_path(index.relative_path, "search_index.relative_path");
        require(artifact_paths.insert(index.relative_path.generic_string()).second,
                "Duplicate route artifact path: " + index.relative_path.generic_string());
        require(index.descriptor_dimensions > 0, "Route search index descriptor_dimensions must be positive");
        require(index.item_count > 0, "Route search index item_count must be positive");
        require(index.byte_size > 0, "Route search index byte_size must be positive");
        require(is_sha256_hex(index.sha256), "Route search index SHA-256 must be lowercase 64-character hex");
    }

    std::unordered_set<std::string> gate_ids;
    for (const auto& gate : manifest.gates) {
        validate_identifier(gate.id, "gate.id");
        validate_identifier(gate.verification_layer_id, "gate.verification_layer_id");
        validate_identifier(gate.chunk_id, "gate.chunk_id");
        validate_identifier(gate.search_index_id, "gate.search_index_id");
        require(gate_ids.insert(gate.id).second, "Duplicate route gate id: " + gate.id);
        const auto layer = layers.find(gate.verification_layer_id);
        require(layer != layers.end() && layer->second->role == RouteLayerRole::Verification,
                "Route gate must reference a verification layer");
        const auto chunk = chunks.find(gate.chunk_id);
        require(chunk != chunks.end() && chunk->second->layer_id == gate.verification_layer_id,
                "Route gate chunk must belong to its verification layer");
        require(gate.frame_id >= chunk->second->first_frame_id && gate.frame_id <= chunk->second->last_frame_id,
                "Route gate frame_id is outside its chunk range");
        require(indexes.contains(gate.search_index_id), "Route gate references unknown search index");
        require_finite(gate.route_progress, "gate.route_progress");
        require(gate.route_progress >= 0.0 && gate.route_progress <= 1.0,
                "Route gate progress must be between 0 and 1");
        require(gate.allowed_directions > 0
                    && (gate.allowed_directions & ~(route_gate_direction_forward | route_gate_direction_reverse)) == 0,
                "Route gate allowed_directions is invalid");
        require_finite(gate.minimum_altitude_m, "gate.minimum_altitude_m");
        require_finite(gate.maximum_altitude_m, "gate.maximum_altitude_m");
        require(gate.minimum_altitude_m >= layer->second->minimum_altitude_m
                    && gate.maximum_altitude_m <= layer->second->maximum_altitude_m
                    && gate.maximum_altitude_m >= gate.minimum_altitude_m,
                "Route gate altitude envelope must be inside its verification layer envelope");
        require_finite(gate.minimum_scale_ratio, "gate.minimum_scale_ratio");
        require_finite(gate.maximum_scale_ratio, "gate.maximum_scale_ratio");
        require(gate.minimum_scale_ratio > 0.0 && gate.maximum_scale_ratio >= gate.minimum_scale_ratio,
                "Route gate scale envelope is invalid");
        require_finite(gate.local_x_m, "gate.local_x_m");
        require_finite(gate.local_y_m, "gate.local_y_m");
        require_finite(gate.local_z_m, "gate.local_z_m");
        require_finite(gate.local_yaw_rad, "gate.local_yaw_rad");
        require_finite(gate.local_position_uncertainty_m, "gate.local_position_uncertainty_m");
        require_finite(gate.approach_radius_m, "gate.approach_radius_m");
        if (gate.has_local_pose) {
            require(!manifest.local_frame_id.empty(), "Route gate local pose requires package local frame identity");
            require(gate.local_position_uncertainty_m >= 0.0, "Route gate local uncertainty must be non-negative");
            require(gate.approach_radius_m > gate.local_position_uncertainty_m,
                    "Route gate approach radius must exceed local position uncertainty");
        } else {
            require(gate.local_x_m == 0.0 && gate.local_y_m == 0.0 && gate.local_z_m == 0.0
                        && gate.local_yaw_rad == 0.0 && gate.local_position_uncertainty_m == 0.0
                        && gate.approach_radius_m == 0.0,
                    "Route gate without local pose must zero all local approach fields");
        }
    }
}

void write_route_package_manifest(const std::filesystem::path& path, const RoutePackageManifest& manifest) {
    validate_route_package_manifest(manifest);
    if (path.empty()) {
        throw std::invalid_argument("Route package manifest output path must not be empty");
    }
    auto partial_path = path;
    partial_path += ".partial";
    if (std::filesystem::exists(path) || std::filesystem::exists(partial_path)) {
        throw std::runtime_error("Route package manifest output or partial path already exists");
    }

    std::ofstream output(partial_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot open route package manifest partial for write: " + partial_path.string());
    }
    output.write(manifest_magic.data(), static_cast<std::streamsize>(manifest_magic.size()));
    write_u16(output, manifest.version);
    write_u8(output, little_endian);
    write_u8(output, 0);
    write_string(output, manifest.route_id);
    write_string(output, manifest.route_frame);
    write_string(output, manifest.local_frame_id);
    write_string(output, manifest.local_frame_revision);
    write_string(output, manifest.local_frame_convention);
    write_string(output, manifest.camera.profile_id);
    write_u8(output, sensor_type_to_u8(manifest.camera.sensor_type));
    write_u8(output, pixel_format_to_u8(manifest.camera.pixel_format));
    write_u16(output, manifest.camera.capture_width);
    write_u16(output, manifest.camera.capture_height);
    write_f64(output, manifest.camera.horizontal_fov_rad);
    write_f64(output, manifest.camera.vertical_fov_rad);
    write_f64(output, manifest.camera.camera_to_body_x_m);
    write_f64(output, manifest.camera.camera_to_body_y_m);
    write_f64(output, manifest.camera.camera_to_body_z_m);
    write_f64(output, manifest.camera.camera_to_body_roll_rad);
    write_f64(output, manifest.camera.camera_to_body_pitch_rad);
    write_f64(output, manifest.camera.camera_to_body_yaw_rad);
    write_u32(output, static_cast<std::uint32_t>(manifest.layers.size()));
    write_u32(output, static_cast<std::uint32_t>(manifest.chunks.size()));
    write_u32(output, static_cast<std::uint32_t>(manifest.search_indexes.size()));
    write_u32(output, static_cast<std::uint32_t>(manifest.gates.size()));

    for (const auto& layer : manifest.layers) {
        write_string(output, layer.id);
        write_u8(output, layer_role_to_u8(layer.role));
        write_string(output, layer.camera_profile_id);
        write_u8(output, pixel_format_to_u8(layer.pixel_format));
        write_u16(output, layer.width);
        write_u16(output, layer.height);
        write_f64(output, layer.minimum_altitude_m);
        write_f64(output, layer.maximum_altitude_m);
    }
    for (const auto& chunk : manifest.chunks) {
        write_string(output, chunk.id);
        write_string(output, chunk.layer_id);
        write_string(output, chunk.relative_path.generic_string());
        write_u16(output, chunk.artifact_format_version);
        write_u64(output, chunk.first_frame_id);
        write_u64(output, chunk.last_frame_id);
        write_u64(output, chunk.entry_count);
        write_u64(output, chunk.byte_size);
        write_digest(output, chunk.sha256);
    }
    for (const auto& index : manifest.search_indexes) {
        write_string(output, index.id);
        write_string(output, index.layer_id);
        write_string(output, index.relative_path.generic_string());
        write_string(output, index.descriptor_type);
        write_u32(output, index.descriptor_dimensions);
        write_u64(output, index.item_count);
        write_u64(output, index.byte_size);
        write_digest(output, index.sha256);
    }
    for (const auto& gate : manifest.gates) {
        write_string(output, gate.id);
        write_string(output, gate.verification_layer_id);
        write_string(output, gate.chunk_id);
        write_string(output, gate.search_index_id);
        write_u64(output, gate.frame_id);
        write_u32(output, gate.route_segment_id);
        write_f64(output, gate.route_progress);
        write_u8(output, gate.allowed_directions);
        write_u8(output, gate.has_local_pose ? 1U : 0U);
        write_f64(output, gate.local_x_m);
        write_f64(output, gate.local_y_m);
        write_f64(output, gate.local_z_m);
        write_f64(output, gate.local_yaw_rad);
        write_f64(output, gate.local_position_uncertainty_m);
        write_f64(output, gate.approach_radius_m);
        write_f64(output, gate.minimum_altitude_m);
        write_f64(output, gate.maximum_altitude_m);
        write_f64(output, gate.minimum_scale_ratio);
        write_f64(output, gate.maximum_scale_ratio);
    }
    output.flush();
    require_stream_ok(output, "flush");
    output.close();
    require_stream_ok(output, "close");
    std::filesystem::rename(partial_path, path);
}

RoutePackageManifest read_route_package_manifest(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open route package manifest for read: " + path.string());
    }
    std::array<char, 4> magic{};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!input || magic != manifest_magic) {
        throw std::runtime_error("Invalid route package manifest magic");
    }

    RoutePackageManifest manifest;
    manifest.version = read_u16(input);
    if (read_u8(input) != little_endian || read_u8(input) != 0) {
        throw std::runtime_error("Unsupported route package manifest byte order or flags");
    }
    manifest.route_id = read_string(input, maximum_identifier_length, "route_id");
    manifest.route_frame = read_string(input, maximum_identifier_length, "route_frame");
    manifest.local_frame_id = read_string(input, maximum_identifier_length, "local_frame_id");
    manifest.local_frame_revision = read_string(input, maximum_identifier_length, "local_frame_revision");
    manifest.local_frame_convention = read_string(input, maximum_identifier_length, "local_frame_convention");
    manifest.camera.profile_id = read_string(input, maximum_identifier_length, "camera.profile_id");
    manifest.camera.sensor_type = sensor_type_from_u8(read_u8(input));
    manifest.camera.pixel_format = pixel_format_from_u8(read_u8(input));
    manifest.camera.capture_width = read_u16(input);
    manifest.camera.capture_height = read_u16(input);
    manifest.camera.horizontal_fov_rad = read_f64(input);
    manifest.camera.vertical_fov_rad = read_f64(input);
    manifest.camera.camera_to_body_x_m = read_f64(input);
    manifest.camera.camera_to_body_y_m = read_f64(input);
    manifest.camera.camera_to_body_z_m = read_f64(input);
    manifest.camera.camera_to_body_roll_rad = read_f64(input);
    manifest.camera.camera_to_body_pitch_rad = read_f64(input);
    manifest.camera.camera_to_body_yaw_rad = read_f64(input);
    const auto layer_count = read_u32(input);
    const auto chunk_count = read_u32(input);
    const auto index_count = read_u32(input);
    const auto gate_count = read_u32(input);
    if (layer_count > maximum_layers || chunk_count > maximum_chunks
        || index_count > maximum_search_indexes || gate_count > maximum_gates) {
        throw std::runtime_error("Route package manifest record count exceeds safety limit");
    }

    manifest.layers.reserve(layer_count);
    for (std::uint32_t i = 0; i < layer_count; ++i) {
        RouteLayerRecord layer;
        layer.id = read_string(input, maximum_identifier_length, "layer.id");
        layer.role = layer_role_from_u8(read_u8(input));
        layer.camera_profile_id = read_string(input, maximum_identifier_length, "layer.camera_profile_id");
        layer.pixel_format = pixel_format_from_u8(read_u8(input));
        layer.width = read_u16(input);
        layer.height = read_u16(input);
        layer.minimum_altitude_m = read_f64(input);
        layer.maximum_altitude_m = read_f64(input);
        manifest.layers.push_back(std::move(layer));
    }
    manifest.chunks.reserve(chunk_count);
    for (std::uint32_t i = 0; i < chunk_count; ++i) {
        RouteChunkRecord chunk;
        chunk.id = read_string(input, maximum_identifier_length, "chunk.id");
        chunk.layer_id = read_string(input, maximum_identifier_length, "chunk.layer_id");
        chunk.relative_path = read_string(input, maximum_path_length, "chunk.relative_path");
        chunk.artifact_format_version = read_u16(input);
        chunk.first_frame_id = read_u64(input);
        chunk.last_frame_id = read_u64(input);
        chunk.entry_count = read_u64(input);
        chunk.byte_size = read_u64(input);
        chunk.sha256 = read_digest(input);
        manifest.chunks.push_back(std::move(chunk));
    }
    manifest.search_indexes.reserve(index_count);
    for (std::uint32_t i = 0; i < index_count; ++i) {
        RouteSearchIndexRecord index;
        index.id = read_string(input, maximum_identifier_length, "search_index.id");
        index.layer_id = read_string(input, maximum_identifier_length, "search_index.layer_id");
        index.relative_path = read_string(input, maximum_path_length, "search_index.relative_path");
        index.descriptor_type = read_string(input, maximum_identifier_length, "search_index.descriptor_type");
        index.descriptor_dimensions = read_u32(input);
        index.item_count = read_u64(input);
        index.byte_size = read_u64(input);
        index.sha256 = read_digest(input);
        manifest.search_indexes.push_back(std::move(index));
    }
    manifest.gates.reserve(gate_count);
    for (std::uint32_t i = 0; i < gate_count; ++i) {
        RouteGateKeyframeRecord gate;
        gate.id = read_string(input, maximum_identifier_length, "gate.id");
        gate.verification_layer_id = read_string(input, maximum_identifier_length, "gate.verification_layer_id");
        gate.chunk_id = read_string(input, maximum_identifier_length, "gate.chunk_id");
        gate.search_index_id = read_string(input, maximum_identifier_length, "gate.search_index_id");
        gate.frame_id = read_u64(input);
        gate.route_segment_id = read_u32(input);
        gate.route_progress = read_f64(input);
        gate.allowed_directions = read_u8(input);
        const auto local_pose_flag = read_u8(input);
        if (local_pose_flag > 1) {
            throw std::runtime_error("Invalid route gate local-pose flag");
        }
        gate.has_local_pose = local_pose_flag == 1;
        gate.local_x_m = read_f64(input);
        gate.local_y_m = read_f64(input);
        gate.local_z_m = read_f64(input);
        gate.local_yaw_rad = read_f64(input);
        gate.local_position_uncertainty_m = read_f64(input);
        gate.approach_radius_m = read_f64(input);
        gate.minimum_altitude_m = read_f64(input);
        gate.maximum_altitude_m = read_f64(input);
        gate.minimum_scale_ratio = read_f64(input);
        gate.maximum_scale_ratio = read_f64(input);
        manifest.gates.push_back(std::move(gate));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Route package manifest contains trailing bytes");
    }
    validate_route_package_manifest(manifest);
    return manifest;
}

RoutePackageVerificationResult verify_route_package_files(
    const std::filesystem::path& manifest_path,
    const RoutePackageManifest& manifest) {
    validate_route_package_manifest(manifest);
    RoutePackageVerificationResult result;
    const auto base_input = manifest_path.parent_path().empty()
        ? std::filesystem::current_path()
        : manifest_path.parent_path();
    const auto base = std::filesystem::weakly_canonical(base_input);

    auto verify = [&](const std::filesystem::path& relative_path,
                      std::uint64_t expected_size,
                      const std::string& expected_digest) {
        ++result.files_checked;
        const auto resolved = std::filesystem::weakly_canonical(base / relative_path);
        if (!path_has_prefix(resolved, base)) {
            result.errors.push_back("outside_package:" + relative_path.generic_string());
            return;
        }
        if (!std::filesystem::exists(resolved) || !std::filesystem::is_regular_file(resolved)) {
            result.errors.push_back("missing:" + relative_path.generic_string());
            return;
        }
        const auto actual_size = std::filesystem::file_size(resolved);
        if (actual_size != expected_size) {
            result.errors.push_back("size_mismatch:" + relative_path.generic_string());
            return;
        }
        if (sha256_file_hex(resolved) != expected_digest) {
            result.errors.push_back("sha256_mismatch:" + relative_path.generic_string());
        }
    };
    for (const auto& chunk : manifest.chunks) {
        verify(chunk.relative_path, chunk.byte_size, chunk.sha256);
    }
    for (const auto& index : manifest.search_indexes) {
        verify(index.relative_path, index.byte_size, index.sha256);
    }
    result.passed = result.errors.empty();
    return result;
}

} // namespace vh
