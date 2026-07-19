#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "visual_homing/route_descriptor_index.hpp"

namespace {

std::uint32_t positive_u32(const char* value, const char* name) {
    const auto parsed = std::stoul(value);
    if (parsed == 0 || parsed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument(std::string(name) + " must be a positive uint32");
    }
    return static_cast<std::uint32_t>(parsed);
}

std::uint16_t positive_u16(const char* value, const char* name) {
    const auto parsed = positive_u32(value, name);
    if (parsed > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument(std::string(name) + " exceeds uint16");
    }
    return static_cast<std::uint16_t>(parsed);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4 && argc != 7) {
        std::cerr
            << "usage: route_descriptor_index_builder INPUT.vhrm OUTPUT.vhrm INDEX_RELATIVE_PATH"
            << " [GRID_WIDTH GRID_HEIGHT SAMPLE_STRIDE]\n";
        return 2;
    }
    try {
        vh::RouteDescriptorIndexBuildConfig config;
        config.input_manifest_path = argv[1];
        config.output_manifest_path = argv[2];
        config.index_relative_path = argv[3];
        if (argc == 7) {
            config.grid_width = positive_u16(argv[4], "grid_width");
            config.grid_height = positive_u16(argv[5], "grid_height");
            config.sample_stride = positive_u32(argv[6], "sample_stride");
        }
        const auto result = vh::build_route_descriptor_index_package(config);
        std::cout
            << "route_descriptor_index_build_done"
            << " source_entries_scanned=" << result.source_entries_scanned
            << " descriptors_written=" << result.descriptors_written
            << " descriptor_dimensions=" << result.index_record.descriptor_dimensions
            << " index_path=" << result.index_record.relative_path.generic_string()
            << " output_manifest=" << config.output_manifest_path.string()
            << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "route_descriptor_index_build_failed error=" << error.what() << '\n';
        return 1;
    }
}
