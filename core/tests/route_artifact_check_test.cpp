#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "visual_homing/route_artifact_check.hpp"

namespace {

vh::RouteSignatureEntry entry(std::uint64_t id, std::vector<std::uint8_t> payload) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = 1000 + id;
    result.width = 2;
    result.height = 2;
    result.format = vh::PixelFormat::Gray8;
    result.payload = std::move(payload);
    return result;
}

} // namespace

int main() {
    vh::RouteSignatureFile route;
    route.entries.push_back(entry(0, {0, 10, 20, 30}));
    route.entries.push_back(entry(1, {40, 50, 60, 70}));
    route.entries.push_back(entry(2, {80, 90, 100, 110}));

    const auto summary = vh::self_match_route_signature(route);
    assert(summary.entries_checked == 3);
    assert(summary.valid_matches == 3);
    assert(summary.exact_index_matches == 3);
    assert(summary.minimum_confidence_seen > 0.999);
    assert(summary.average_confidence > 0.999);
    assert(summary.last_progress > 0.999);
    assert(summary.progress_monotonic);
    assert(summary.passed);

    const auto path = std::filesystem::temp_directory_path() / "visual_homing_route_artifact_check_test.vhrs";
    vh::write_route_signature_file(path, route);
    const auto file_summary = vh::self_match_route_signature_file(path);
    assert(file_summary.passed);
    assert(file_summary.entries_checked == 3);

    bool rejected_empty = false;
    try {
        vh::RouteSignatureFile empty;
        (void)vh::self_match_route_signature(empty);
    } catch (const std::invalid_argument&) {
        rejected_empty = true;
    }
    assert(rejected_empty);

    return 0;
}
