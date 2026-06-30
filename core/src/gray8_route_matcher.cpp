#include "visual_homing/gray8_route_matcher.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

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

double shifted_normalized_mean_absolute_difference(
    const Frame& current,
    const RouteSignatureEntry& reference,
    int shift_px) {
    std::uint64_t sum = 0;
    std::size_t count = 0;

    for (int y = 0; y < current.height; ++y) {
        for (int x = 0; x < current.width; ++x) {
            const int reference_x = x + shift_px;
            if (reference_x < 0 || reference_x >= current.width) {
                continue;
            }

            const auto current_index = static_cast<std::size_t>(y) * static_cast<std::size_t>(current.width)
                + static_cast<std::size_t>(x);
            const auto reference_index = static_cast<std::size_t>(y) * static_cast<std::size_t>(current.width)
                + static_cast<std::size_t>(reference_x);
            const auto delta = static_cast<int>(current.data[current_index]) - static_cast<int>(reference.payload[reference_index]);
            sum += static_cast<std::uint64_t>(std::abs(delta));
            ++count;
        }
    }

    if (count == 0) {
        return std::numeric_limits<double>::infinity();
    }

    return static_cast<double>(sum) / (static_cast<double>(count) * 255.0);
}

double scaled_normalized_mean_absolute_difference(
    const Frame& current,
    const RouteSignatureEntry& reference,
    double scale_ratio) {
    if (scale_ratio <= 0.0 || !std::isfinite(scale_ratio)) {
        return std::numeric_limits<double>::infinity();
    }

    const double center_x = (static_cast<double>(current.width) - 1.0) * 0.5;
    const double center_y = (static_cast<double>(current.height) - 1.0) * 0.5;
    std::uint64_t sum = 0;
    std::size_t count = 0;
    for (int y = 0; y < current.height; ++y) {
        for (int x = 0; x < current.width; ++x) {
            const auto reference_x = static_cast<int>(
                std::lround(center_x + (static_cast<double>(x) - center_x) / scale_ratio));
            const auto reference_y = static_cast<int>(
                std::lround(center_y + (static_cast<double>(y) - center_y) / scale_ratio));
            if (reference_x < 0 || reference_x >= current.width || reference_y < 0 || reference_y >= current.height) {
                continue;
            }

            const auto current_index = static_cast<std::size_t>(y) * static_cast<std::size_t>(current.width)
                + static_cast<std::size_t>(x);
            const auto reference_index = static_cast<std::size_t>(reference_y) * static_cast<std::size_t>(current.width)
                + static_cast<std::size_t>(reference_x);
            const auto delta = static_cast<int>(current.data[current_index]) - static_cast<int>(reference.payload[reference_index]);
            sum += static_cast<std::uint64_t>(std::abs(delta));
            ++count;
        }
    }

    if (count == 0) {
        return std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(sum) / (static_cast<double>(count) * 255.0);
}

double best_scaled_normalized_mean_absolute_difference(
    const Frame& current,
    const RouteSignatureEntry& reference) {
    const double candidates[] = {
        0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75,
        0.80, 0.85, 0.90, 0.95, 1.0, 1.05, 1.10, 1.15, 1.20, 1.25,
        1.30, 1.35, 1.40, 1.50};
    double best_distance = std::numeric_limits<double>::infinity();
    for (const auto scale : candidates) {
        best_distance = std::min(best_distance, scaled_normalized_mean_absolute_difference(current, reference, scale));
    }
    return best_distance;
}

double estimate_direction_error_rad(
    const Frame& current,
    const RouteSignatureEntry& reference,
    int max_shift_px,
    double radians_per_pixel) {
    if (max_shift_px <= 0 || radians_per_pixel == 0.0) {
        return 0.0;
    }

    double best_distance = std::numeric_limits<double>::infinity();
    int best_shift = 0;
    for (int shift = -max_shift_px; shift <= max_shift_px; ++shift) {
        const auto distance = shifted_normalized_mean_absolute_difference(current, reference, shift);
        if (distance < best_distance) {
            best_distance = distance;
            best_shift = shift;
        }
    }

    return static_cast<double>(best_shift) * radians_per_pixel;
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

void insert_top_candidate(std::vector<RouteMatchCandidate>& candidates,
                          std::size_t max_candidates,
                          RouteMatchCandidate candidate) {
    if (max_candidates == 0) {
        return;
    }
    const auto position = std::lower_bound(
        candidates.begin(),
        candidates.end(),
        candidate,
        [](const RouteMatchCandidate& lhs, const RouteMatchCandidate& rhs) {
            return lhs.confidence > rhs.confidence;
        });
    candidates.insert(position, candidate);
    if (candidates.size() > max_candidates) {
        candidates.pop_back();
    }
}

} // namespace

Gray8RouteMatcher::Gray8RouteMatcher(RouteSignatureFile route, Gray8RouteMatcherConfig config)
    : route_(std::move(route)), config_(config) {
    if (route_.entries.empty()) {
        throw std::invalid_argument("Gray8RouteMatcher requires at least one route entry");
    }
    if (config_.max_direction_shift_px < 0) {
        throw std::invalid_argument("Gray8RouteMatcher max_direction_shift_px must be non-negative");
    }
    if (config_.radians_per_pixel < 0.0) {
        throw std::invalid_argument("Gray8RouteMatcher radians_per_pixel must be non-negative");
    }
}

RouteMatch Gray8RouteMatcher::match(const Frame& frame) {
    recent_top_candidates_.clear();
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
        insert_top_candidate(
            recent_top_candidates_,
            config_.top_candidate_count,
            RouteMatchCandidate{
                index,
                route_.entries.size() > 1
                    ? static_cast<double>(index) / static_cast<double>(route_.entries.size() - 1)
                    : 1.0,
                std::clamp(1.0 - distance, 0.0, 1.0),
            });
        if (distance < best_distance) {
            best_distance = distance;
            best_index = index;
        }
    }

    if (config_.enable_scale_refinement) {
        const auto refine_begin = best_index > config_.scale_refinement_radius
            ? std::max(begin, best_index - config_.scale_refinement_radius)
            : begin;
        const auto refine_end = std::min(end, best_index + config_.scale_refinement_radius + 1);
        for (std::size_t index = refine_begin; index < refine_end; ++index) {
            const auto& entry = route_.entries[index];
            const auto distance = best_scaled_normalized_mean_absolute_difference(frame, entry);
            if (distance < best_distance) {
                best_distance = distance;
                best_index = index;
            }
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
    match.direction_error_rad = estimate_direction_error_rad(
        frame,
        route_.entries[best_index],
        config_.max_direction_shift_px,
        config_.radians_per_pixel);
    match.confidence = confidence;
    match.valid = valid;
    return match;
}

const std::vector<RouteMatchCandidate>& Gray8RouteMatcher::recent_top_candidates() const noexcept {
    return recent_top_candidates_;
}

} // namespace vh
