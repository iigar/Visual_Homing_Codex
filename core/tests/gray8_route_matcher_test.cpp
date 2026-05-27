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

    vh::Gray8RouteMatcher windowed(route, {.window_radius = 1, .minimum_confidence = 0.0});
    const auto first = windowed.match(frame(200, {40, 40, 40, 40}));
    assert(first.route_index == 1);

    const auto constrained = windowed.match(frame(201, {240, 240, 240, 240}));
    assert(constrained.valid);
    assert(constrained.route_index == 2);
    assert(constrained.confidence < 0.6);

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

    return 0;
}
