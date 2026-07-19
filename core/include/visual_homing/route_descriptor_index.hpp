#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "visual_homing/route_package_manifest.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

constexpr std::uint16_t route_descriptor_index_format_version = 1;
inline constexpr const char* gray8_centered_block_mean_i8_v1 =
    "gray8-centered-block-mean-i8-v1";

struct RouteDescriptorIndexEntry {
    std::uint64_t frame_id = 0;
    std::uint32_t chunk_index = 0;
    std::uint32_t entry_index = 0;
    std::vector<std::int8_t> descriptor;
};

struct RouteDescriptorIndex {
    std::uint16_t version = route_descriptor_index_format_version;
    std::uint16_t source_width = 0;
    std::uint16_t source_height = 0;
    std::uint16_t grid_width = 0;
    std::uint16_t grid_height = 0;
    std::vector<RouteDescriptorIndexEntry> entries;
};

struct RouteDescriptorIndexBuildConfig {
    std::filesystem::path input_manifest_path;
    std::filesystem::path output_manifest_path;
    std::filesystem::path index_relative_path = "index/coarse-v1.vhix";
    std::string index_id = "coarse-index-v1";
    std::string descriptor_layer_id = "global-descriptor-v1";
    std::uint16_t grid_width = 16;
    std::uint16_t grid_height = 10;
    std::uint32_t sample_stride = 1;
    std::uint64_t maximum_chunk_bytes = 64U * 1024U * 1024U;
    std::uint64_t maximum_index_entries = 250000;
};

struct RouteDescriptorIndexBuildResult {
    RoutePackageManifest manifest;
    RouteSearchIndexRecord index_record;
    std::uint64_t source_entries_scanned = 0;
    std::uint64_t descriptors_written = 0;
};

std::vector<std::int8_t> make_gray8_centered_block_mean_descriptor(
    const RouteSignatureEntry& entry,
    std::uint16_t grid_width,
    std::uint16_t grid_height);

void validate_route_descriptor_index(const RouteDescriptorIndex& index);
void write_route_descriptor_index(
    const std::filesystem::path& path,
    const RouteDescriptorIndex& index);
RouteDescriptorIndex read_route_descriptor_index(const std::filesystem::path& path);

RouteDescriptorIndexBuildResult build_route_descriptor_index_package(
    const RouteDescriptorIndexBuildConfig& config);

} // namespace vh
