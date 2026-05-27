#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "visual_homing/frame.hpp"

namespace vh {

constexpr std::uint16_t route_signature_format_version = 1;

struct RouteSignatureEntry {
    std::uint64_t frame_id = 0;
    std::uint64_t timestamp_ns = 0;
    std::int16_t altitude_band_m = 0;
    float heading_hint_rad = 0.0F;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    PixelFormat format = PixelFormat::Gray8;
    std::vector<std::uint8_t> payload;
};

struct RouteSignatureFile {
    std::uint16_t version = route_signature_format_version;
    std::vector<RouteSignatureEntry> entries;
};

void write_route_signature_file(const std::filesystem::path& path, const RouteSignatureFile& route);
RouteSignatureFile read_route_signature_file(const std::filesystem::path& path);

} // namespace vh
