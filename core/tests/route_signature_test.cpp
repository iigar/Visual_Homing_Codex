#include <cassert>
#include <cmath>
#include <filesystem>
#include <vector>

#include "visual_homing/route_signature.hpp"

int main() {
    vh::RouteSignatureFile route;

    vh::RouteSignatureEntry first;
    first.frame_id = 10;
    first.timestamp_ns = 1000000000ULL;
    first.altitude_band_m = 120;
    first.heading_hint_rad = 0.25F;
    first.width = 2;
    first.height = 2;
    first.format = vh::PixelFormat::Gray8;
    first.payload = {1, 2, 3, 4};
    route.entries.push_back(first);

    vh::RouteSignatureEntry second;
    second.frame_id = 11;
    second.timestamp_ns = 1033333333ULL;
    second.altitude_band_m = 125;
    second.heading_hint_rad = -0.5F;
    second.width = 2;
    second.height = 2;
    second.format = vh::PixelFormat::Gray8;
    second.payload = {4, 3, 2, 1};
    route.entries.push_back(second);

    const auto path = std::filesystem::temp_directory_path() / "visual_homing_route_signature_test.vhrs";
    vh::write_route_signature_file(path, route);

    const auto loaded = vh::read_route_signature_file(path);
    assert(loaded.version == vh::route_signature_format_version);
    assert(loaded.entries.size() == 2);

    assert(loaded.entries[0].frame_id == first.frame_id);
    assert(loaded.entries[0].timestamp_ns == first.timestamp_ns);
    assert(loaded.entries[0].altitude_band_m == first.altitude_band_m);
    assert(std::fabs(loaded.entries[0].heading_hint_rad - first.heading_hint_rad) < 0.0001F);
    assert(loaded.entries[0].width == first.width);
    assert(loaded.entries[0].height == first.height);
    assert(loaded.entries[0].format == first.format);
    assert(loaded.entries[0].payload == first.payload);

    assert(loaded.entries[1].frame_id == second.frame_id);
    assert(loaded.entries[1].timestamp_ns == second.timestamp_ns);
    assert(loaded.entries[1].altitude_band_m == second.altitude_band_m);
    assert(std::fabs(loaded.entries[1].heading_hint_rad - second.heading_hint_rad) < 0.0001F);
    assert(loaded.entries[1].width == second.width);
    assert(loaded.entries[1].height == second.height);
    assert(loaded.entries[1].format == second.format);
    assert(loaded.entries[1].payload == second.payload);

    const auto summary = vh::inspect_route_signature_file(path);
    assert(summary.version == vh::route_signature_format_version);
    assert(summary.entry_count == 2);
    assert(summary.first_frame_id == 10);
    assert(summary.last_frame_id == 11);
    assert(summary.first_timestamp_ns == 1000000000ULL);
    assert(summary.last_timestamp_ns == 1033333333ULL);
    assert(summary.width == 2);
    assert(summary.height == 2);
    assert(summary.min_payload_bytes == 4);
    assert(summary.max_payload_bytes == 4);
    assert(summary.total_payload_bytes == 8);
    assert(summary.timestamps_monotonic);
    assert(summary.uniform_dimensions);
    assert(summary.uniform_payload_size);
    assert(summary.all_gray8);

    auto mixed_route = loaded;
    mixed_route.entries[1].timestamp_ns = 999999999ULL;
    mixed_route.entries[1].width = 3;
    mixed_route.entries[1].payload.push_back(9);
    mixed_route.entries[1].format = vh::PixelFormat::Bgr8;
    const auto mixed_summary = vh::summarize_route_signature(mixed_route);
    assert(!mixed_summary.timestamps_monotonic);
    assert(!mixed_summary.uniform_dimensions);
    assert(!mixed_summary.uniform_payload_size);
    assert(!mixed_summary.all_gray8);
    assert(mixed_summary.min_payload_bytes == 4);
    assert(mixed_summary.max_payload_bytes == 5);
    assert(mixed_summary.total_payload_bytes == 9);

    return 0;
}
