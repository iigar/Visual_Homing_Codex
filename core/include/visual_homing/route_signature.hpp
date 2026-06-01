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

struct RouteSignatureSummary {
    std::uint16_t version = route_signature_format_version;
    std::uint64_t entry_count = 0;
    std::uint64_t first_frame_id = 0;
    std::uint64_t last_frame_id = 0;
    std::uint64_t first_timestamp_ns = 0;
    std::uint64_t last_timestamp_ns = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint64_t min_payload_bytes = 0;
    std::uint64_t max_payload_bytes = 0;
    std::uint64_t total_payload_bytes = 0;
    std::int16_t min_altitude_band_m = 0;
    std::int16_t max_altitude_band_m = 0;
    float min_heading_hint_rad = 0.0F;
    float max_heading_hint_rad = 0.0F;
    bool timestamps_monotonic = true;
    bool uniform_dimensions = true;
    bool uniform_payload_size = true;
    bool uniform_altitude_band = true;
    bool uniform_heading_hint = true;
    bool all_gray8 = true;
};

void write_route_signature_file(const std::filesystem::path& path, const RouteSignatureFile& route);
RouteSignatureFile read_route_signature_file(const std::filesystem::path& path);
RouteSignatureSummary summarize_route_signature(const RouteSignatureFile& route);
RouteSignatureSummary inspect_route_signature_file(const std::filesystem::path& path);

} // namespace vh
