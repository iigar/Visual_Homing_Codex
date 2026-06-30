#include <cassert>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"

namespace {

vh::RouteSignatureEntry entry(std::uint64_t id, std::vector<std::uint8_t> payload) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.width = 2;
    result.height = 2;
    result.format = vh::PixelFormat::Gray8;
    result.payload = std::move(payload);
    return result;
}

vh::Frame frame(std::uint64_t id, std::vector<std::uint8_t> payload) {
    vh::Frame result;
    result.id = id;
    result.width = 2;
    result.height = 2;
    result.format = vh::PixelFormat::Gray8;
    result.data = std::move(payload);
    return result;
}

std::vector<std::uint8_t> offset(std::vector<std::uint8_t> payload, int delta) {
    for (auto& value : payload) {
        const auto adjusted = std::clamp(static_cast<int>(value) + delta, 0, 255);
        value = static_cast<std::uint8_t>(adjusted);
    }
    return payload;
}

} // namespace

int main() {
    vh::RouteSignatureFile route;
    route.entries.push_back(entry(0, {0, 0, 0, 0}));
    route.entries.push_back(entry(1, {40, 40, 40, 40}));
    route.entries.push_back(entry(2, {120, 120, 120, 120}));
    route.entries.push_back(entry(3, {240, 240, 240, 240}));

    vh::Gray8RouteMatcher matcher(route, {.window_radius = 0, .minimum_confidence = 0.5});

    const auto near_second = matcher.match(frame(100, {43, 41, 39, 40}));
    assert(near_second.valid);
    assert(near_second.route_index == 1);
    assert(near_second.progress > 0.32);
    assert(near_second.progress < 0.34);
    assert(near_second.confidence > 0.99);

    const auto near_third = matcher.match(frame(101, {116, 121, 123, 119}));
    assert(near_third.valid);
    assert(near_third.route_index == 2);
    assert(near_third.progress > 0.66);
    assert(near_third.progress < 0.67);

    vh::Gray8RouteMatcher top_k_matcher(route, {
        .window_radius = 0,
        .minimum_confidence = 0.0,
        .top_candidate_count = 3,
    });
    const auto top_k_match = top_k_matcher.match(frame(150, {42, 42, 42, 42}));
    assert(top_k_match.route_index == 1);
    const auto& top_candidates = top_k_matcher.recent_top_candidates();
    assert(top_candidates.size() == 3);
    assert(top_candidates[0].route_index == 1);
    assert(top_candidates[1].route_index == 0);
    assert(top_candidates[2].route_index == 2);
    assert(top_candidates[0].confidence > top_candidates[1].confidence);
    assert(top_candidates[1].confidence > top_candidates[2].confidence);

    vh::Gray8RouteMatcher windowed(route, {.window_radius = 1, .minimum_confidence = 0.0});
    const auto first = windowed.match(frame(200, {40, 40, 40, 40}));
    assert(first.route_index == 1);

    const auto constrained = windowed.match(frame(201, {240, 240, 240, 240}));
    assert(constrained.valid);
    assert(constrained.route_index == 2);
    assert(constrained.confidence < 0.6);

    vh::Gray8RouteMatcher windowed_top_k(route, {
        .window_radius = 1,
        .minimum_confidence = 0.0,
        .top_candidate_count = 3,
    });
    const auto window_anchor = windowed_top_k.match(frame(210, {40, 40, 40, 40}));
    assert(window_anchor.route_index == 1);
    const auto window_limited = windowed_top_k.match(frame(211, {240, 240, 240, 240}));
    assert(window_limited.route_index == 2);
    const auto& window_candidates = windowed_top_k.recent_top_candidates();
    assert(window_candidates.size() == 3);
    assert(window_candidates[0].route_index == 2);
    assert(window_candidates[1].route_index == 1);
    assert(window_candidates[2].route_index == 0);

    vh::Gray8RouteMatcher strict(route, {.window_radius = 0, .minimum_confidence = 0.9});
    const auto poor = strict.match(frame(300, {180, 180, 180, 180}));
    assert(!poor.valid);
    assert(poor.confidence < 0.9);

    vh::RouteSignatureFile textured_route;
    textured_route.entries.push_back(entry(0, {10, 80, 10, 80}));
    textured_route.entries.push_back(entry(1, {80, 10, 80, 10}));
    textured_route.entries.push_back(entry(2, {30, 30, 220, 220}));

    vh::Gray8RouteMatcher perturbation_matcher(textured_route, {.window_radius = 0, .minimum_confidence = 0.8});
    const auto brightness_offset = perturbation_matcher.match(frame(400, offset({80, 10, 80, 10}, 8)));
    assert(brightness_offset.valid);
    assert(brightness_offset.route_index == 1);
    assert(brightness_offset.confidence > 0.96);

    const auto noisy = perturbation_matcher.match(frame(401, {28, 35, 215, 225}));
    assert(noisy.valid);
    assert(noisy.route_index == 2);
    assert(noisy.confidence > 0.97);

    vh::Gray8RouteMatcher scale_refinement_matcher(textured_route, {
        .window_radius = 1,
        .minimum_confidence = 0.8,
        .enable_scale_refinement = true,
        .scale_refinement_radius = 1,
    });
    const auto scale_refined = scale_refinement_matcher.match(frame(402, {28, 35, 215, 225}));
    assert(scale_refined.valid);
    assert(scale_refined.route_index == 2);
    assert(scale_refined.confidence > 0.97);

    vh::RouteSignatureFile shift_route;
    shift_route.entries.push_back(entry(0, {
        10, 30, 90, 170,
        10, 30, 90, 170,
    }));
    shift_route.entries[0].width = 4;
    shift_route.entries[0].height = 2;

    vh::Gray8RouteMatcher shift_matcher(shift_route, {
        .window_radius = 0,
        .minimum_confidence = 0.0,
        .max_direction_shift_px = 2,
        .radians_per_pixel = 0.05,
    });

    auto shifted_left = frame(500, {
        30, 90, 170, 170,
        30, 90, 170, 170,
    });
    shifted_left.width = 4;
    shifted_left.height = 2;
    const auto left_error = shift_matcher.match(shifted_left);
    assert(left_error.direction_error_rad > 0.049);
    assert(left_error.direction_error_rad < 0.051);

    vh::Gray8RouteMatcher right_shift_matcher(shift_route, {
        .window_radius = 0,
        .minimum_confidence = 0.0,
        .max_direction_shift_px = 2,
        .radians_per_pixel = 0.05,
    });

    auto shifted_right = frame(501, {
        10, 10, 30, 90,
        10, 10, 30, 90,
    });
    shifted_right.width = 4;
    shifted_right.height = 2;
    const auto right_error = right_shift_matcher.match(shifted_right);
    assert(right_error.direction_error_rad < -0.049);
    assert(right_error.direction_error_rad > -0.051);

    vh::Gray8RouteMatcher no_shift_matcher(shift_route, {
        .window_radius = 0,
        .minimum_confidence = 0.0,
        .max_direction_shift_px = 2,
        .radians_per_pixel = 0.05,
    });
    auto aligned = frame(502, {
        10, 30, 90, 170,
        10, 30, 90, 170,
    });
    aligned.width = 4;
    aligned.height = 2;
    const auto aligned_error = no_shift_matcher.match(aligned);
    assert(aligned_error.direction_error_rad > -0.001);
    assert(aligned_error.direction_error_rad < 0.001);

    return 0;
}
