#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"

namespace {
std::size_t cases = 0;
std::uint32_t random_state = 0x87654321;

std::uint8_t next_pixel() {
    random_state = random_state * 1664525U + 1013904223U;
    return static_cast<std::uint8_t>(random_state >> 24);
}

void check_pair(int width, int height, std::vector<std::uint8_t> pixels,
                std::vector<std::uint8_t> reference_pixels) {
    assert(!pixels.empty() && pixels.size() == reference_pixels.size());
    // Original scalar arithmetic, independently accumulated in uint64_t.
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        sum += static_cast<std::uint64_t>(std::abs(static_cast<int>(pixels[i]) - static_cast<int>(reference_pixels[i])));
    }
    const double expected = 1.0 - static_cast<double>(sum) / (static_cast<double>(pixels.size()) * 255.0);
    vh::Frame frame{.id = 23, .timestamp = vh::Timestamp(std::chrono::milliseconds(456)),
        .width = width, .height = height, .data = std::move(pixels)};
    vh::RouteSignatureEntry entry;
    entry.width = static_cast<std::uint16_t>(width);
    entry.height = static_cast<std::uint16_t>(height);
    entry.payload = std::move(reference_pixels);
    vh::RouteSignatureFile route;
    route.entries.push_back(std::move(entry));
    vh::Gray8RouteMatcher matcher(std::move(route), {.minimum_confidence = 0.5, .top_candidate_count = 1});
    const auto match = matcher.match(frame);
    assert(std::bit_cast<std::uint64_t>(match.confidence) == std::bit_cast<std::uint64_t>(expected));
    assert(match.timestamp == frame.timestamp);
    assert(match.route_index == 0 && match.progress == 1.0);
    assert(match.direction_error_rad == 0.0 && !match.direction_observation_valid);
    assert(match.valid == (expected >= 0.5));
    const auto& top = matcher.recent_top_candidates();
    assert(top.size() == 1 && top[0].route_index == 0 && top[0].progress == 1.0);
    assert(std::bit_cast<std::uint64_t>(top[0].confidence) == std::bit_cast<std::uint64_t>(expected));
    const auto zones = matcher.probe_progress_zones(frame);
    assert(zones.size() == 5 && zones.back().valid);
    assert(std::bit_cast<std::uint64_t>(zones.back().candidate.confidence) == std::bit_cast<std::uint64_t>(expected));
    ++cases;
}

void check_patterns(int width, int height) {
    const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (int pattern = 0; pattern < 3; ++pattern) {
        std::vector<std::uint8_t> current(size), reference(size);
        for (std::size_t i = 0; i < size; ++i) {
            current[i] = pattern == 0 ? next_pixel() : pattern == 1 ? 0 : static_cast<std::uint8_t>(i % 256);
            reference[i] = pattern == 0 ? next_pixel() : pattern == 1 ? 255 : current[i];
        }
        check_pair(width, height, std::move(current), std::move(reference));
    }
}
} // namespace

int main() {
    // Every byte pair independently, including signed-byte and zero boundaries.
    for (int a = 0; a < 256; ++a) {
        for (int b = 0; b < 256; ++b) {
            check_pair(1, 1, {static_cast<std::uint8_t>(a)}, {static_cast<std::uint8_t>(b)});
        }
    }
    // Every byte pair again in one vectorized-sized payload.
    std::vector<std::uint8_t> current(65536), reference(65536);
    for (std::size_t i = 0; i < current.size(); ++i) {
        current[i] = static_cast<std::uint8_t>(i / 256);
        reference[i] = static_cast<std::uint8_t>(i % 256);
    }
    check_pair(256, 256, std::move(current), std::move(reference));
    for (const int size : {1,2,3,7,8,15,16,17,31,32,33,63,64,65,127,128,129,
                          255,256,257,1023,1024,1025,15999,16000,16001,65535}) {
        check_patterns(size, 1);
    }
    check_patterns(2, 32769); // 65538 bytes: full blocks followed by two pixels.
    // A real representable Gray8 shape whose worst-case sum exceeds uint32_t.
    constexpr int width = 4097, height = 4113;
    constexpr std::size_t large_size = static_cast<std::size_t>(width) * height;
    static_assert(large_size * std::uint64_t{255} > std::numeric_limits<std::uint32_t>::max());
    check_pair(width, height, std::vector<std::uint8_t>(large_size, 0),
        std::vector<std::uint8_t>(large_size, 255));
    std::cout << "Exact raw-distance API cases: " << cases << '\n';
}
