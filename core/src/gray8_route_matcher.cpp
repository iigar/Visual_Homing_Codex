#include "visual_homing/gray8_route_matcher.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace vh {
namespace {

double normalized_mean_absolute_difference(const std::vector<std::uint8_t>& current, const std::vector<std::uint8_t>& reference) {
    if (current.size() != reference.size() || current.empty()) {
        throw std::runtime_error("Gray8 route matcher payload sizes must match");
    }

    std::uint64_t sum = 0;
    for (std::size_t index = 0; index < current.size(); ++index) {
        const auto delta = static_cast<int>(current[index]) - static_cast<int>(reference[index]);
        sum += static_cast<std::uint64_t>(std::abs(delta));
    }

    return static_cast<double>(sum) / (static_cast<double>(current.size()) * 255.0);
}

void validate_frame_against_entry(const Frame& frame, const RouteSignatureEntry& entry) {
    if (frame.format != PixelFormat::Gray8 || entry.format != PixelFormat::Gray8) {
        throw std::runtime_error("Gray8 route matcher only accepts Gray8 frames and route entries");
    }
    if (frame.width != entry.width || frame.height != entry.height) {
        throw std::runtime_error("Gray8 route matcher frame dimensions do not match route entry dimensions");
    }
    const auto expected_size = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    if (frame.width <= 0 || frame.height <= 0 || frame.data.size() != expected_size || entry.payload.size() != expected_size) {
        throw std::runtime_error("Gray8 route matcher received malformed payload");
    }
}

} // namespace

Gray8RouteMatcher::Gray8RouteMatcher(RouteSignatureFile route, Gray8RouteMatcherConfig config)
    : route_(std::move(route)), config_(config) {
    if (route_.entries.empty()) {
        throw std::invalid_argument("Gray8RouteMatcher requires at least one route entry");
    }
}

RouteMatch Gray8RouteMatcher::match(const Frame& frame) {
    std::size_t begin = 0;
    std::size_t end = route_.entries.size();
    if (last_index_.has_value() && config_.window_radius > 0) {
        begin = (*last_index_ > config_.window_radius) ? *last_index_ - config_.window_radius : 0;
        end = std::min(route_.entries.size(), *last_index_ + config_.window_radius + 1);
    }

    double best_distance = std::numeric_limits<double>::infinity();
    std::size_t best_index = begin;
    for (std::size_t index = begin; index < end; ++index) {
        const auto& entry = route_.entries[index];
        validate_frame_against_entry(frame, entry);
        const auto distance = normalized_mean_absolute_difference(frame.data, entry.payload);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = index;
        }
    }

    const auto confidence = std::clamp(1.0 - best_distance, 0.0, 1.0);
    const auto valid = confidence >= config_.minimum_confidence;
    if (valid) {
        last_index_ = best_index;
    }

    RouteMatch match;
    match.timestamp = frame.timestamp;
    match.route_index = best_index;
    match.progress = route_.entries.size() > 1
        ? static_cast<double>(best_index) / static_cast<double>(route_.entries.size() - 1)
        : 1.0;
    match.direction_error_rad = 0.0;
    match.confidence = confidence;
    match.valid = valid;
    return match;
}

} // namespace vh
