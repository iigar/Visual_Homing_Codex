#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"

namespace {
vh::RouteSignatureEntry entry(int width, int height, std::vector<std::uint8_t> pixels) {
    vh::RouteSignatureEntry result;
    result.width = width;
    result.height = height;
    result.format = vh::PixelFormat::Gray8;
    result.payload = std::move(pixels);
    return result;
}

vh::Frame frame(const vh::RouteSignatureEntry& source) {
    return {.id = 77, .timestamp = vh::Timestamp(std::chrono::milliseconds(123)),
        .width = source.width, .height = source.height,
        .format = source.format, .data = source.payload};
}

void check(const vh::RouteMatch& match, std::size_t index, double progress, double confidence, bool valid) {
    assert(match.timestamp == vh::Timestamp(std::chrono::milliseconds(123)));
    assert(match.route_index == index);
    assert(match.progress == progress);
    assert(match.confidence == confidence);
    assert(match.valid == valid);
}

void scaled_ties() {
    const auto reference = entry(4, 4, {0,10,20,30, 40,50,60,70, 80,90,100,110, 120,130,140,150});
    const auto zoom = entry(4, 4, {50,50,60,70, 50,50,60,70, 90,90,100,110, 130,130,140,150});
    auto near = zoom;
    for (auto& pixel : near.payload) ++pixel;
    vh::RouteSignatureFile route;
    route.entries = {reference, zoom, zoom};
    const vh::Gray8RouteMatcherConfig config{.max_direction_shift_px = 2, .radians_per_pixel = 0.05,
        .enable_scale_refinement = true, .scale_refinement_radius = 2, .top_candidate_count = 3};
    vh::Gray8RouteMatcher exact(route, config);
    const auto matched = exact.match(frame(zoom));
    // Coarse exact winner survives an earlier entry with an equally exact scaled score.
    check(matched, 1, 0.5, 1.0, true);
    assert(matched.direction_error_rad == 0.0 && matched.direction_observation_valid);
    const auto& top = exact.recent_top_candidates();
    assert(top.size() == 3 && top[0].route_index == 2 && top[1].route_index == 1);
    assert(top[0].progress == 1.0 && top[1].progress == 0.5);
    assert(top[0].confidence == 1.0 && top[1].confidence == 1.0);

    route.entries = {reference, near, reference};
    vh::Gray8RouteMatcher refined(route, config);
    const auto reranked = refined.match(frame(zoom));
    // Both neighbors reach zero at a later scale; the first refined tie wins.
    check(reranked, 0, 0.0, 1.0, true);
    assert(refined.recent_top_candidates().front().route_index == 1);
    assert(refined.recent_top_candidates().front().confidence == 1.0 - 1.0 / 255.0);
}

void overlap_and_positive_scores() {
    vh::RouteSignatureFile route;
    const auto zeros = entry(3, 3, std::vector<std::uint8_t>(9, 0));
    auto query = entry(3, 3, std::vector<std::uint8_t>(9, 20));
    query.payload[4] = 0;
    route.entries = {zeros, entry(3, 3, std::vector<std::uint8_t>(9, 21)), zeros};
    vh::Gray8RouteMatcher early_scale(route, {.enable_scale_refinement = true, .scale_refinement_radius = 2});
    // Scale 0.3 sees only the central pixel. The first refined neighbor wins.
    check(early_scale.match(frame(query)), 0, 0.0, 1.0, true);
    route.entries = {entry(2, 2, {0,0,0,0})};
    vh::Gray8RouteMatcher no_zero(route, {.minimum_confidence = 1.0, .enable_scale_refinement = true});
    // Initial small scales have no overlap; finite positive scores must still be evaluated.
    check(no_zero.match(frame(entry(2, 2, {1,1,1,1}))), 0, 1.0, 1.0 - 1.0 / 255.0, false);
}

void directional_rank_and_state() {
    // Exact zero, negative and positive ranked scores, in both search directions.
    for (const int direction : {-1, 1}) {
        for (const double bias : {127.0 / 255.0, 1.0, 0.1}) {
            vh::RouteSignatureFile route;
            route.entries = {entry(3, 3, std::vector<std::uint8_t>(9, 0)),
                             entry(3, 3, std::vector<std::uint8_t>(9, 255))};
            if (direction < 0) std::reverse(route.entries.begin(), route.entries.end());
            const std::size_t anchor = direction > 0 ? 0 : 1;
            const std::size_t next = 1 - anchor;
            vh::Gray8RouteMatcher matcher(route, {.window_radius = 1, .minimum_confidence = 0.75,
                .enable_scale_refinement = true, .top_candidate_count = 2,
                .directional_search_direction = direction, .directional_search_bias = bias});
            check(matcher.match(frame(route.entries[anchor])), anchor, static_cast<double>(anchor), 1.0, true);
            const auto result = matcher.match(frame(entry(3, 3, std::vector<std::uint8_t>(9, 128))));
            check(result, next, static_cast<double>(next), 1.0 - 127.0 / 255.0, false);
            assert(result.direction_error_rad == 0.0 && !result.direction_observation_valid);
            // An invalid result must not advance the directional window past the anchor.
            // With reverse bias 1, the earlier entry has ranked score 0 as well;
            // its raw score is 1, so the existing first-tie policy rejects it.
            const bool ranked_tie = direction < 0 && bias == 1.0;
            check(matcher.match(frame(route.entries[anchor])), ranked_tie ? next : anchor,
                static_cast<double>(ranked_tie ? next : anchor), ranked_tie ? 0.0 : 1.0, !ranked_tie);
        }
    }
}

void validation_after_exact_match() {
    const auto reference = entry(3, 3, std::vector<std::uint8_t>(9, 10));
    vh::RouteSignatureFile route;
    route.entries = {reference, entry(2, 2, {10,10,10,10})};
    vh::Gray8RouteMatcher matcher(route, {.enable_scale_refinement = true});
    bool rejected = false;
    try {
        (void)matcher.match(frame(reference));
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()) == "Gray8 route matcher frame dimensions do not match route entry dimensions";
    }
    assert(rejected); // Finding zero cannot skip validation of later coarse candidates.

    route.entries = {reference, reference};
    vh::Gray8RouteMatcher initial(route, {.enable_scale_refinement = true,
        .initial_progress_window_enabled = true, .initial_progress_min = 1.0, .initial_progress_max = 1.0});
    check(initial.match(frame(reference)), 1, 1.0, 1.0, true);
    check(initial.match(frame(reference)), 0, 0.0, 1.0, true);
}
} // namespace

int main() {
    scaled_ties();
    overlap_and_positive_scores();
    directional_rank_and_state();
    validation_after_exact_match();
}
