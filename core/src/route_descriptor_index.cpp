#include "visual_homing/route_descriptor_index.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#include "visual_homing/artifact_digest.hpp"

namespace vh {
namespace {

constexpr std::array<char, 4> magic = {'V', 'H', 'I', 'X'};
constexpr std::uint16_t header_size = 32;
constexpr std::uint8_t little_endian = 1;
constexpr std::uint8_t centered_block_mean_i8 = 1;
constexpr std::uint64_t maximum_entries_v1 = 250000;
constexpr std::uint32_t maximum_dimensions_v1 = 1024;
constexpr std::uint64_t maximum_descriptor_bytes_v1 = 64U * 1024U * 1024U;
constexpr std::uint64_t entry_header_size = 16;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
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

std::uint8_t read_u8(std::istream& input) {
    const auto value = input.get();
    if (value == std::char_traits<char>::eof()) {
        throw std::runtime_error("Truncated VHIX descriptor index");
    }
    return static_cast<std::uint8_t>(value);
}

std::uint16_t read_u16(std::istream& input) {
    const auto low = read_u8(input);
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(read_u8(input)) << 8U));
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

void require_stream_ok(const std::ios& stream, const std::string& action) {
    require(static_cast<bool>(stream), "Failed to " + action + " VHIX descriptor index");
}

std::uint32_t descriptor_dimensions(const RouteDescriptorIndex& index) {
    return static_cast<std::uint32_t>(index.grid_width)
        * static_cast<std::uint32_t>(index.grid_height);
}

bool path_has_prefix(const std::filesystem::path& path, const std::filesystem::path& prefix) {
    auto path_it = path.begin();
    for (auto prefix_it = prefix.begin(); prefix_it != prefix.end(); ++prefix_it, ++path_it) {
        if (path_it == path.end() || *path_it != *prefix_it) {
            return false;
        }
    }
    return true;
}

void validate_output_parent(
    const std::filesystem::path& package_directory,
    const std::filesystem::path& output_path) {
    std::filesystem::create_directories(output_path.parent_path());
    const auto package = std::filesystem::weakly_canonical(package_directory);
    const auto parent = std::filesystem::weakly_canonical(output_path.parent_path());
    require(path_has_prefix(parent, package), "VHIX output path escapes the route package directory");
    require(!std::filesystem::is_symlink(std::filesystem::symlink_status(output_path.parent_path())),
        "VHIX output directory must not be a symlink");
}

const RouteLayerRecord& find_tracking_layer(const RoutePackageManifest& manifest) {
    const RouteLayerRecord* found = nullptr;
    for (const auto& layer : manifest.layers) {
        if (layer.role == RouteLayerRole::Tracking) {
            require(found == nullptr, "Route package has more than one tracking layer");
            found = &layer;
        }
    }
    require(found != nullptr, "Route package has no tracking layer");
    return *found;
}

} // namespace

std::vector<std::int8_t> make_gray8_centered_block_mean_descriptor(
    const RouteSignatureEntry& entry,
    std::uint16_t grid_width,
    std::uint16_t grid_height) {
    require(entry.format == PixelFormat::Gray8, "VHIX descriptor requires Gray8 input");
    require(entry.width > 0 && entry.height > 0, "VHIX descriptor source dimensions must be positive");
    require(grid_width > 0 && grid_height > 0
        && grid_width <= entry.width && grid_height <= entry.height,
        "VHIX descriptor grid must fit inside the source image");
    require(entry.payload.size() == static_cast<std::size_t>(entry.width) * entry.height,
        "VHIX descriptor source payload size is invalid");

    std::uint64_t global_sum = 0;
    for (const auto value : entry.payload) {
        global_sum += value;
    }
    const auto global_mean = static_cast<double>(global_sum) / entry.payload.size();
    std::vector<std::int8_t> result;
    result.reserve(static_cast<std::size_t>(grid_width) * grid_height);
    for (std::uint16_t gy = 0; gy < grid_height; ++gy) {
        const auto y0 = static_cast<std::uint32_t>(gy) * entry.height / grid_height;
        const auto y1 = static_cast<std::uint32_t>(gy + 1U) * entry.height / grid_height;
        for (std::uint16_t gx = 0; gx < grid_width; ++gx) {
            const auto x0 = static_cast<std::uint32_t>(gx) * entry.width / grid_width;
            const auto x1 = static_cast<std::uint32_t>(gx + 1U) * entry.width / grid_width;
            std::uint64_t sum = 0;
            std::uint64_t count = 0;
            for (auto y = y0; y < y1; ++y) {
                for (auto x = x0; x < x1; ++x) {
                    sum += entry.payload[static_cast<std::size_t>(y) * entry.width + x];
                    ++count;
                }
            }
            require(count > 0, "VHIX descriptor grid contains an empty cell");
            const auto centered = static_cast<long>(std::lround(static_cast<double>(sum) / count - global_mean));
            result.push_back(static_cast<std::int8_t>(std::clamp(centered, -127L, 127L)));
        }
    }
    return result;
}

void validate_route_descriptor_index(const RouteDescriptorIndex& index) {
    require(index.version == route_descriptor_index_format_version, "Unsupported VHIX version");
    require(index.source_width > 0 && index.source_height > 0, "VHIX source dimensions must be positive");
    require(index.grid_width > 0 && index.grid_height > 0
        && index.grid_width <= index.source_width && index.grid_height <= index.source_height,
        "VHIX grid dimensions must fit inside the source dimensions");
    const auto dimensions = descriptor_dimensions(index);
    require(dimensions > 0 && dimensions <= maximum_dimensions_v1, "VHIX descriptor dimensions exceed v1 bounds");
    require(!index.entries.empty(), "VHIX index must contain at least one descriptor");
    require(index.entries.size() <= maximum_entries_v1, "VHIX item count exceeds v1 bounds");
    const auto artifact_bytes = static_cast<std::uint64_t>(index.entries.size())
        * (entry_header_size + dimensions);
    require(artifact_bytes <= maximum_descriptor_bytes_v1, "VHIX descriptor payload exceeds v1 bounds");
    std::uint64_t previous_frame_id = 0;
    std::uint32_t previous_chunk_index = 0;
    std::uint32_t previous_entry_index = 0;
    for (std::size_t i = 0; i < index.entries.size(); ++i) {
        const auto& entry = index.entries[i];
        require(entry.descriptor.size() == dimensions, "VHIX entry descriptor size is invalid");
        if (i > 0) {
            require(entry.frame_id > previous_frame_id, "VHIX frame IDs must be strictly increasing");
            require(entry.chunk_index > previous_chunk_index
                || (entry.chunk_index == previous_chunk_index && entry.entry_index > previous_entry_index),
                "VHIX source locations must be strictly increasing");
        }
        previous_frame_id = entry.frame_id;
        previous_chunk_index = entry.chunk_index;
        previous_entry_index = entry.entry_index;
    }
}

void write_route_descriptor_index(
    const std::filesystem::path& path,
    const RouteDescriptorIndex& index) {
    validate_route_descriptor_index(index);
    require(!path.empty(), "VHIX output path must not be empty");
    auto partial = path;
    partial += ".partial";
    require(!std::filesystem::exists(path) && !std::filesystem::exists(partial),
        "VHIX output or partial path already exists");
    std::ofstream output(partial, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "Could not open VHIX partial for write");
    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    write_u16(output, index.version);
    write_u16(output, header_size);
    write_u8(output, little_endian);
    write_u8(output, centered_block_mean_i8);
    write_u16(output, 0);
    write_u16(output, index.grid_width);
    write_u16(output, index.grid_height);
    write_u32(output, descriptor_dimensions(index));
    write_u64(output, index.entries.size());
    write_u16(output, index.source_width);
    write_u16(output, index.source_height);
    for (const auto& entry : index.entries) {
        write_u64(output, entry.frame_id);
        write_u32(output, entry.chunk_index);
        write_u32(output, entry.entry_index);
        for (const auto value : entry.descriptor) {
            write_u8(output, static_cast<std::uint8_t>(value));
        }
    }
    output.flush();
    require_stream_ok(output, "write");
    output.close();
    require_stream_ok(output, "close");
    require(!std::filesystem::exists(path), "VHIX final path appeared before finalize");
    std::filesystem::rename(partial, path);
}

RouteDescriptorIndex read_route_descriptor_index(const std::filesystem::path& path) {
    const auto file_size = std::filesystem::file_size(path);
    require(file_size >= header_size, "VHIX file is smaller than its header");
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Could not open VHIX file for read");
    std::array<char, 4> read_magic{};
    input.read(read_magic.data(), static_cast<std::streamsize>(read_magic.size()));
    require(read_magic == magic, "Invalid VHIX magic");
    RouteDescriptorIndex index;
    index.version = read_u16(input);
    require(read_u16(input) == header_size, "Unsupported VHIX header size");
    require(read_u8(input) == little_endian, "Unsupported VHIX endian policy");
    require(read_u8(input) == centered_block_mean_i8, "Unsupported VHIX descriptor encoding");
    (void)read_u16(input);
    index.grid_width = read_u16(input);
    index.grid_height = read_u16(input);
    const auto dimensions = read_u32(input);
    const auto item_count = read_u64(input);
    index.source_width = read_u16(input);
    index.source_height = read_u16(input);
    require(dimensions == descriptor_dimensions(index) && dimensions <= maximum_dimensions_v1,
        "VHIX descriptor dimensions are inconsistent");
    require(item_count <= maximum_entries_v1, "VHIX item count exceeds v1 bounds");
    require(item_count * (entry_header_size + dimensions) <= maximum_descriptor_bytes_v1,
        "VHIX descriptor payload exceeds v1 bounds");
    require(header_size + item_count * (entry_header_size + dimensions) == file_size,
        "VHIX file size or trailing-byte policy failed");
    index.entries.reserve(static_cast<std::size_t>(item_count));
    for (std::uint64_t i = 0; i < item_count; ++i) {
        RouteDescriptorIndexEntry entry;
        entry.frame_id = read_u64(input);
        entry.chunk_index = read_u32(input);
        entry.entry_index = read_u32(input);
        entry.descriptor.resize(dimensions);
        for (auto& value : entry.descriptor) {
            value = static_cast<std::int8_t>(read_u8(input));
        }
        index.entries.push_back(std::move(entry));
    }
    validate_route_descriptor_index(index);
    return index;
}

RouteDescriptorIndexBuildResult build_route_descriptor_index_package(
    const RouteDescriptorIndexBuildConfig& config) {
    require(!config.input_manifest_path.empty() && !config.output_manifest_path.empty(),
        "VHIX builder manifest paths must not be empty");
    require(config.input_manifest_path != config.output_manifest_path,
        "VHIX builder must not overwrite the input manifest");
    require(config.sample_stride > 0, "VHIX builder sample stride must be positive");
    require(config.maximum_chunk_bytes > 0, "VHIX builder chunk byte bound must be positive");
    require(config.maximum_index_entries > 0 && config.maximum_index_entries <= maximum_entries_v1,
        "VHIX builder item bound is invalid");
    const auto package_directory = config.input_manifest_path.parent_path();
    require(std::filesystem::weakly_canonical(package_directory)
            == std::filesystem::weakly_canonical(config.output_manifest_path.parent_path()),
        "VHIX derived manifest must remain in the input package directory");
    require(!std::filesystem::exists(config.output_manifest_path),
        "VHIX derived manifest already exists");
    auto output_manifest_partial = config.output_manifest_path;
    output_manifest_partial += ".partial";
    require(!std::filesystem::exists(output_manifest_partial),
        "VHIX derived manifest partial already exists");

    auto manifest = read_route_package_manifest(config.input_manifest_path);
    const auto verification = verify_route_package_files(config.input_manifest_path, manifest);
    require(verification.passed, "VHIX input package failed artifact verification");
    require(manifest.search_indexes.empty(), "VHIX builder v1 requires a package without existing indexes");
    const auto tracking = find_tracking_layer(manifest);
    require(tracking.pixel_format == PixelFormat::Gray8, "VHIX builder v1 requires a Gray8 tracking layer");
    require(config.grid_width > 0 && config.grid_height > 0
        && config.grid_width <= tracking.width && config.grid_height <= tracking.height,
        "VHIX builder grid must fit inside the tracking layer");
    require(static_cast<std::uint32_t>(config.grid_width) * config.grid_height <= maximum_dimensions_v1,
        "VHIX builder descriptor dimensions exceed v1 bounds");

    RouteLayerRecord descriptor_layer{
        .id = config.descriptor_layer_id,
        .role = RouteLayerRole::GlobalDescriptor,
        .camera_profile_id = tracking.camera_profile_id,
        .pixel_format = PixelFormat::Gray8,
        .width = config.grid_width,
        .height = config.grid_height,
        .minimum_altitude_m = tracking.minimum_altitude_m,
        .maximum_altitude_m = tracking.maximum_altitude_m,
    };
    manifest.layers.push_back(descriptor_layer);
    RouteSearchIndexRecord provisional{
        .id = config.index_id,
        .layer_id = config.descriptor_layer_id,
        .relative_path = config.index_relative_path,
        .descriptor_type = gray8_centered_block_mean_i8_v1,
        .descriptor_dimensions = static_cast<std::uint32_t>(config.grid_width) * config.grid_height,
        .item_count = 1,
        .byte_size = 1,
        .sha256 = std::string(64, '0'),
    };
    manifest.search_indexes.push_back(provisional);
    validate_route_package_manifest(manifest);
    manifest.search_indexes.clear();

    const auto index_path = package_directory / config.index_relative_path;
    validate_output_parent(package_directory, index_path);
    RouteDescriptorIndex index{
        .source_width = tracking.width,
        .source_height = tracking.height,
        .grid_width = config.grid_width,
        .grid_height = config.grid_height,
        .entries = {},
    };
    std::uint64_t global_ordinal = 0;
    std::uint64_t source_scanned = 0;
    for (std::uint32_t chunk_index = 0; chunk_index < manifest.chunks.size(); ++chunk_index) {
        const auto& chunk = manifest.chunks[chunk_index];
        if (chunk.layer_id != tracking.id) {
            continue;
        }
        require(chunk.byte_size <= config.maximum_chunk_bytes,
            "VHIX source chunk exceeds configured byte bound");
        const auto route = read_route_signature_file(package_directory / chunk.relative_path);
        require(route.entries.size() == chunk.entry_count, "VHIX source chunk entry count disagrees with manifest");
        for (std::uint32_t entry_index = 0; entry_index < route.entries.size(); ++entry_index) {
            const auto& source = route.entries[entry_index];
            ++source_scanned;
            if (global_ordinal % config.sample_stride == 0) {
                require(index.entries.size() < config.maximum_index_entries,
                    "VHIX builder item count exceeds configured bound");
                index.entries.push_back({
                    .frame_id = source.frame_id,
                    .chunk_index = chunk_index,
                    .entry_index = entry_index,
                    .descriptor = make_gray8_centered_block_mean_descriptor(
                        source, config.grid_width, config.grid_height),
                });
            }
            ++global_ordinal;
        }
    }
    require(!index.entries.empty(), "VHIX builder produced no descriptors");
    write_route_descriptor_index(index_path, index);
    const auto loaded = read_route_descriptor_index(index_path);
    require(loaded.entries.size() == index.entries.size(), "VHIX finalized index failed read-back verification");

    RouteSearchIndexRecord record{
        .id = config.index_id,
        .layer_id = config.descriptor_layer_id,
        .relative_path = config.index_relative_path,
        .descriptor_type = gray8_centered_block_mean_i8_v1,
        .descriptor_dimensions = descriptor_dimensions(index),
        .item_count = index.entries.size(),
        .byte_size = std::filesystem::file_size(index_path),
        .sha256 = sha256_file_hex(index_path),
    };
    manifest.search_indexes.push_back(record);
    validate_route_package_manifest(manifest);
    write_route_package_manifest(config.output_manifest_path, manifest);
    const auto output_verification = verify_route_package_files(config.output_manifest_path, manifest);
    require(output_verification.passed, "VHIX derived package failed artifact verification");
    return {
        .manifest = std::move(manifest),
        .index_record = record,
        .source_entries_scanned = source_scanned,
        .descriptors_written = index.entries.size(),
    };
}

} // namespace vh
