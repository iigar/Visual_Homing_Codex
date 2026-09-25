#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"

namespace {
// Frozen scalar oracle from 1345ab9. Keep the per-pixel traversal independent
// of the production coordinate-hoisting implementation.
double scalar_distance(const vh::Frame& current, const vh::RouteSignatureEntry& reference, double scale) {
    if (scale <= 0.0 || !std::isfinite(scale)) return std::numeric_limits<double>::infinity();
    const double cx = (static_cast<double>(current.width) - 1.0) * 0.5;
    const double cy = (static_cast<double>(current.height) - 1.0) * 0.5;
    std::uint64_t sum = 0;
    std::size_t count = 0;
    for (int y = 0; y < current.height; ++y) {
        for (int x = 0; x < current.width; ++x) {
            const auto rx = static_cast<int>(std::lround(cx + (static_cast<double>(x) - cx) / scale));
            const auto ry = static_cast<int>(std::lround(cy + (static_cast<double>(y) - cy) / scale));
            if (rx < 0 || rx >= current.width || ry < 0 || ry >= current.height) continue;
            const auto i = static_cast<std::size_t>(y) * static_cast<std::size_t>(current.width) + static_cast<std::size_t>(x);
            const auto j = static_cast<std::size_t>(ry) * static_cast<std::size_t>(current.width) + static_cast<std::size_t>(rx);
            sum += static_cast<std::uint64_t>(std::abs(static_cast<int>(current.data[i]) - static_cast<int>(reference.payload[j])));
            ++count;
        }
    }
    if (count == 0) return std::numeric_limits<double>::infinity();
    return static_cast<double>(sum) / (static_cast<double>(count) * 255.0);
}

std::uint32_t random_state = 0x10293847;
std::uint8_t next_pixel() {
    // Defined unsigned arithmetic, independent of standard-library distributions.
    random_state = random_state * 1664525U + 1013904223U;
    return static_cast<std::uint8_t>(random_state >> 24);
}

std::size_t comparisons = 0;
void compare(const vh::Frame& current, const vh::RouteSignatureEntry& reference, double scale) {
    const auto expected = scalar_distance(current, reference, scale);
    const auto actual = vh::scaled_normalized_mean_absolute_difference(current, reference, scale);
    if (std::bit_cast<std::uint64_t>(expected) != std::bit_cast<std::uint64_t>(actual)) {
        std::cerr << "Scale distance differs: " << current.width << 'x' << current.height
                  << " scale=" << scale << " expected=" << expected << " actual=" << actual << '\n';
        assert(false);
    }
    ++comparisons;
}

void compare_image(int width, int height, int pattern) {
    vh::Frame current;
    current.width = width;
    current.height = height;
    current.data.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    vh::RouteSignatureEntry reference;
    reference.width = width;
    reference.height = height;
    reference.payload.resize(current.data.size());
    for (std::size_t i = 0; i < current.data.size(); ++i) {
        current.data[i] = pattern == 0 ? next_pixel() : pattern == 1 ? 0 : static_cast<std::uint8_t>((i % 3) * 127);
        reference.payload[i] = pattern == 0 ? next_pixel() : pattern == 1 ? 255 : current.data[i];
    }
    for (const double scale : vh::gray8_scale_candidates) compare(current, reference, scale);
    // Adjacent representable scales exercise rounding boundaries without changing
    // the production grid. All operations must retain the original expression.
    for (const double scale : {0.3, 0.5, 1.0, 1.5}) {
        compare(current, reference, std::nextafter(scale, 0.0));
        compare(current, reference, std::nextafter(scale, 2.0));
    }
}
} // namespace

int main() {
    for (const int width : {1,2,3,4,15,31,63,64,65,127,128,129,159,160,161,255,256,257,320}) {
        for (const int height : {1,2,3,7,10,100}) {
            for (int pattern = 0; pattern < 3; ++pattern) compare_image(width, height, pattern);
        }
    }
    compare_image(640, 480, 0);
    compare_image(1280, 720, 0);
    vh::Frame empty;
    vh::RouteSignatureEntry empty_reference;
    for (const auto& dimensions : {std::pair{0,0}, std::pair{0,3}, std::pair{3,0}, std::pair{-1,3}, std::pair{3,-1}}) {
        empty.width = dimensions.first;
        empty.height = dimensions.second;
        compare(empty, empty_reference, 1.0);
    }
    for (const double scale : {0.0, -0.0, -1.0, std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        compare(empty, empty_reference, scale);
    }
    std::cout << "Exact scalar-oracle comparisons: " << comparisons << '\n';
}
