#include "visual_homing/route_artifact_check.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "visual_homing/gray8_route_matcher.hpp"

namespace vh {
namespace {

Frame frame_from_entry(const RouteSignatureEntry& entry) {
    Frame frame;
    frame.id = entry.frame_id;
    frame.width = entry.width;
    frame.height = entry.height;
    frame.format = entry.format;
    frame.data = entry.payload;
    return frame;
}

} // namespace

RouteSelfMatchSummary self_match_route_signature(
    const RouteSignatureFile& route,
    const RouteSelfMatchConfig& config) {
    if (route.entries.empty()) {
        throw std::invalid_argument("Route self-match requires at least one route entry");
    }

    Gray8RouteMatcher matcher(route, {
        .window_radius = 1,
        .minimum_confidence = config.minimum_confidence,
        .max_direction_shift_px = config.max_direction_shift_px,
        .radians_per_pixel = config.radians_per_pixel,
    });

    RouteSelfMatchSummary summary;
    summary.minimum_confidence_seen = 1.0;

    double previous_progress = -1.0;
    double confidence_sum = 0.0;
    for (std::size_t index = 0; index < route.entries.size(); ++index) {
        const auto frame = frame_from_entry(route.entries[index]);
        const auto match = matcher.match(frame);

        ++summary.entries_checked;
        if (match.valid) {
            ++summary.valid_matches;
        }
        if (match.route_index == index) {
            ++summary.exact_index_matches;
        }
        if (match.progress + 1.0e-12 < previous_progress) {
            summary.progress_monotonic = false;
        }
        previous_progress = match.progress;
        summary.last_progress = match.progress;
        summary.minimum_confidence_seen = std::min(summary.minimum_confidence_seen, match.confidence);
        confidence_sum += match.confidence;
    }

    summary.average_confidence = confidence_sum / static_cast<double>(summary.entries_checked);
    summary.passed = summary.valid_matches == summary.entries_checked
        && summary.minimum_confidence_seen >= config.minimum_confidence
        && summary.progress_monotonic;
    return summary;
}

RouteSelfMatchSummary self_match_route_signature_file(
    const std::filesystem::path& path,
    const RouteSelfMatchConfig& config) {
    return self_match_route_signature(read_route_signature_file(path), config);
}

} // namespace vh
