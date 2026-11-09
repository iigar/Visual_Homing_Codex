#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"

namespace {
vh::RouteSignatureEntry entry(std::uint8_t value) {
    vh::RouteSignatureEntry result;
    result.width = result.height = 3;
    result.format = vh::PixelFormat::Gray8;
    result.payload.assign(9, value);
    return result;
}

vh::Frame frame(const vh::RouteSignatureEntry& source, int sequence = 1) {
    return {.id = static_cast<std::uint64_t>(sequence),
        .timestamp = vh::Timestamp(std::chrono::milliseconds(sequence * 17)),
        .width = source.width, .height = source.height,
        .format = source.format, .data = source.payload};
}

void equal(const vh::RouteMatch& a, const vh::RouteMatch& b) {
    assert(a.timestamp == b.timestamp && a.route_index == b.route_index && a.valid == b.valid);
    assert(a.direction_observation_valid == b.direction_observation_valid);
    assert(std::bit_cast<std::uint64_t>(a.progress) == std::bit_cast<std::uint64_t>(b.progress));
    assert(std::bit_cast<std::uint64_t>(a.confidence) == std::bit_cast<std::uint64_t>(b.confidence));
    assert(std::bit_cast<std::uint64_t>(a.direction_error_rad) == std::bit_cast<std::uint64_t>(b.direction_error_rad));
}

void check(const vh::RouteMatch& match, const vh::Frame& query, std::size_t index,
           std::size_t entries, double confidence = 1.0, bool valid = true) {
    assert(match.timestamp == query.timestamp && match.route_index == index);
    assert(match.progress == (entries > 1 ? static_cast<double>(index) / (entries - 1) : 1.0));
    assert(match.confidence == confidence && match.valid == valid);
}

void exact_ties_and_top_k() {
    vh::RouteSignatureFile route;
    route.entries = {entry(10), entry(30), entry(10), entry(250), entry(11)};
    const auto query = frame(route.entries[0]);
    for (const bool refinement : {false, true}) {
        vh::Gray8RouteMatcher matcher(route, {.enable_scale_refinement = refinement});
        check(matcher.match(query), query, 0, 5);
        assert(matcher.recent_top_candidates().empty());
        const auto later = frame(route.entries[3], 2);
        check(matcher.match(later), later, 3, 5); // Zero bound is per call.
        vh::Gray8RouteMatcher top(route, {.enable_scale_refinement = refinement, .top_candidate_count = 5});
        check(top.match(query), query, 0, 5);
        const auto& candidates = top.recent_top_candidates();
        assert(candidates.size() == 5);
        const std::vector<std::size_t> indices{2, 0, 4, 1, 3};
        for (std::size_t i = 0; i < indices.size(); ++i) {
            assert(candidates[i].route_index == indices[i]);
            assert(candidates[i].progress == static_cast<double>(indices[i]) / 4);
        }
        assert(candidates[0].confidence == 1.0 && candidates[1].confidence == 1.0);
        assert(candidates[2].confidence == 1.0 - 1.0 / 255.0);
    }
    auto texture = entry(0);
    texture.payload = {0,40,220, 0,40,220, 0,40,220};
    route.entries = {texture, entry(20), texture};
    vh::Gray8RouteMatcher direction(route, {.max_direction_shift_px = 1, .radians_per_pixel = 0.05});
    const auto matched = direction.match(frame(texture));
    assert(matched.route_index == 0 && matched.direction_error_rad == 0.0 && matched.direction_observation_valid);
}

void window_and_invalid_state() {
    vh::RouteSignatureFile route;
    for (int value = 0; value <= 60; value += 10) route.entries.push_back(entry(value));
    vh::Gray8RouteMatcher matcher(route, {.window_radius = 1, .minimum_confidence = 0.99});
    auto query = frame(entry(30));
    check(matcher.match(query), query, 3, 7);
    query = frame(entry(60), 2);
    check(matcher.match(query), query, 4, 7, 1.0 - 20.0 / 255.0, false);
    query = frame(entry(20), 3);
    check(matcher.match(query), query, 2, 7); // Rejected index 4 did not advance the window.
    query = frame(entry(30), 4);
    check(matcher.match(query), query, 3, 7);

    route.entries = {entry(10), entry(10), entry(10)};
    vh::Gray8RouteMatcher initial(route, {.initial_progress_window_enabled = true,
        .initial_progress_min = 0.5, .initial_progress_max = 1.0});
    query = frame(entry(10));
    check(initial.match(query), query, 1, 3);
    check(initial.match(query), query, 0, 3); // Initial window no longer applies.
    vh::Gray8RouteMatcher rejected_zero(route, {.minimum_confidence = 1.01,
        .initial_progress_window_enabled = true, .initial_progress_min = 1.0, .initial_progress_max = 1.0});
    check(rejected_zero.match(query), query, 2, 3, 1.0, false);
    check(rejected_zero.match(query), query, 2, 3, 1.0, false); // Invalid zero does not set last_index.
}

void biased_scores_are_not_bounded_by_zero() {
    vh::RouteSignatureFile route;
    route.entries = {entry(0), entry(1), entry(2)};
    for (const bool refinement : {false, true}) {
        vh::Gray8RouteMatcher matcher(route, {.window_radius = 2, .enable_scale_refinement = refinement,
            .directional_search_direction = 1, .directional_search_bias = 0.1});
        const auto anchor = frame(entry(0));
        check(matcher.match(anchor), anchor, 0, 3);
        check(matcher.match(anchor), anchor, 2, 3, 1.0 - 2.0 / 255.0);
        // Later ranked scores beat an earlier raw zero and an intermediate negative score.
    }
    for (const int direction : {-1, 0, 1}) {
        vh::Gray8RouteMatcher neutral(route, {.window_radius = 2,
            .directional_search_direction = direction, .directional_search_bias = 0.0});
        const auto anchor = frame(entry(1));
        check(neutral.match(anchor), anchor, 1, 3);
        const auto next = frame(entry(direction < 0 ? 0 : 2), 2);
        check(neutral.match(next), next, direction < 0 ? 0 : 2, 3);
    }
}

void validation_and_exceptions() {
    vh::RouteSignatureFile route;
    auto different_shape = entry(10);
    different_shape.width = 1;
    different_shape.height = 9; // Valid route entry, incompatible with this query.
    route.entries = {entry(10), entry(10), different_shape};
    for (const std::size_t top_count : {0U, 3U}) {
        vh::Gray8RouteMatcher matcher(route, {.enable_scale_refinement = true, .top_candidate_count = top_count});
        for (int repeat = 0; repeat < 2; ++repeat) {
            bool rejected = false;
            try { (void)matcher.match(frame(entry(10))); }
            catch (const std::runtime_error& error) {
                rejected = std::string(error.what()) == "Gray8 route matcher frame dimensions do not match route entry dimensions";
            }
            assert(rejected); // Cannot break the structural traversal after finding zero.
            assert(matcher.recent_top_candidates().size() == (top_count ? 2U : 0U));
        }
    }
    route.entries = {entry(10), entry(30)};
    vh::Gray8RouteMatcher matcher(route, {.top_candidate_count = 2});
    (void)matcher.match(frame(entry(10)));
    auto malformed = frame(entry(10));
    malformed.data.pop_back();
    bool rejected = false;
    try { (void)matcher.match(malformed); }
    catch (const std::runtime_error& error) {
        rejected = std::string(error.what()) == "Gray8 route matcher received malformed payload";
    }
    assert(rejected && matcher.recent_top_candidates().empty());
    vh::Gray8RouteMatcher empty_window(route, {.initial_progress_window_enabled = true,
        .initial_progress_min = 0.25, .initial_progress_max = 0.75});
    rejected = false;
    try { (void)empty_window.match(frame(entry(10))); }
    catch (const std::runtime_error& error) {
        rejected = std::string(error.what()) == "Gray8 route matcher initial progress window selected no route entries";
    }
    assert(rejected);
}

std::uint32_t state = 0x7193abcd;
std::uint8_t pixel() {
    state = state * 1664525U + 1013904223U;
    return static_cast<std::uint8_t>(state >> 24);
}

void compare_full_scoring_path() {
    std::size_t comparisons = 0;
    // Asking for every top candidate forces complete coarse scoring; collecting
    // diagnostics must not alter the selected match or its subsequent state.
    for (int scenario = 0; scenario < 144; ++scenario) {
        vh::RouteSignatureFile route;
        const std::size_t count = 2 + scenario % 11;
        for (std::size_t i = 0; i < count; ++i) {
            auto item = entry(0);
            for (auto& value : item.payload) value = pixel();
            route.entries.push_back(std::move(item));
        }
        if (scenario % 2 == 0) route.entries.back() = route.entries.front();
        vh::Gray8RouteMatcherConfig config{.window_radius = static_cast<std::size_t>(scenario % 4),
            .minimum_confidence = scenario % 3 == 0 ? 1.01 : scenario % 3 == 1 ? 0.99 : 0.0,
            .max_direction_shift_px = 1, .radians_per_pixel = 0.05,
            .enable_scale_refinement = scenario % 2 == 0,
            .initial_progress_window_enabled = scenario % 5 == 0,
            .initial_progress_min = 0.0, .initial_progress_max = 0.0,
            .directional_search_direction = scenario % 3 - 1,
            .directional_search_bias = scenario % 4 == 0 ? 0.1 : 0.0};
        vh::Gray8RouteMatcher no_top(route, config);
        config.top_candidate_count = count;
        vh::Gray8RouteMatcher full_scoring(route, config);
        for (int q = 0; q < 24; ++q) {
            auto query = frame(route.entries[pixel() % count], q + 1);
            if (q % 3 == 0) for (auto& value : query.data) value = pixel();
            equal(no_top.match(query), full_scoring.match(query));
            assert(no_top.recent_top_candidates().empty());
            ++comparisons;
        }
        const auto query = frame(route.entries.front());
        const auto a = no_top.probe_progress_zones(query), b = full_scoring.probe_progress_zones(query);
        assert(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            assert(a[i].valid == b[i].valid && a[i].candidate.route_index == b[i].candidate.route_index);
            assert(a[i].candidate.confidence == b[i].candidate.confidence);
        }
    }
    assert(comparisons == 3456);
    std::cout << "Coarse-zero full-scoring comparisons: " << comparisons << '\n';
}
} // namespace

int main() {
    exact_ties_and_top_k();
    window_and_invalid_state();
    biased_scores_are_not_bounded_by_zero();
    validation_and_exceptions();
    compare_full_scoring_path();
}
