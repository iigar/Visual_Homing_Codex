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

std::vector<std::uint8_t> gray8_edge_payload(int width, int height, const std::vector<std::uint8_t>& data) {
    const auto expected_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (width <= 0 || height <= 0 || data.size() != expected_size) {
        throw std::runtime_error("Gray8 edge diagnostics received malformed payload");
    }

    std::vector<std::uint8_t> edges(expected_size, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                + static_cast<std::size_t>(x);
            const int center = static_cast<int>(data[index]);
            int gradient = 0;
            if (x + 1 < width) {
                gradient += std::abs(static_cast<int>(data[index + 1]) - center);
            }
            if (y + 1 < height) {
                gradient += std::abs(static_cast<int>(data[index + static_cast<std::size_t>(width)]) - center);
            }
            edges[index] = static_cast<std::uint8_t>(std::min(gradient, 255));
        }
    }
    return edges;
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

RouteMatchCandidate candidate_from_distance(const RouteSignatureFile& route,
                                            std::size_t index,
                                            double distance) {
    return RouteMatchCandidate{
        index,
        route.entries.size() > 1
            ? static_cast<double>(index) / static_cast<double>(route.entries.size() - 1)
            : 1.0,
        std::clamp(1.0 - distance, 0.0, 1.0),
    };
}

double route_progress_for_index(const RouteSignatureFile& route, std::size_t index) {
    return route.entries.size() > 1
        ? static_cast<double>(index) / static_cast<double>(route.entries.size() - 1)
        : 1.0;
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
    if (config_.initial_progress_window_enabled) {
        if (!std::isfinite(config_.initial_progress_min) || !std::isfinite(config_.initial_progress_max)
            || config_.initial_progress_min < 0.0 || config_.initial_progress_max > 1.0
            || config_.initial_progress_min > config_.initial_progress_max) {
            throw std::invalid_argument("Gray8RouteMatcher initial progress window must be within 0..1");
        }
    }
    if (config_.directional_search_direction < -1 || config_.directional_search_direction > 1) {
        throw std::invalid_argument("Gray8RouteMatcher directional search direction must be -1, 0, or 1");
    }
    if (config_.directional_search_bias < 0.0 || !std::isfinite(config_.directional_search_bias)) {
        throw std::invalid_argument("Gray8RouteMatcher directional search bias must be non-negative");
    }

    route_edge_payloads_.reserve(route_.entries.size());
    for (const auto& entry : route_.entries) {
        if (entry.format != PixelFormat::Gray8) {
            throw std::runtime_error("Gray8 route matcher only accepts Gray8 route entries");
        }
        route_edge_payloads_.push_back(gray8_edge_payload(entry.width, entry.height, entry.payload));
    }
}

RouteMatch Gray8RouteMatcher::match(const Frame& frame) {
    recent_top_candidates_.clear();
    std::size_t begin = 0;
    std::size_t end = route_.entries.size();
    if (last_index_.has_value() && config_.window_radius > 0) {
        begin = (*last_index_ > config_.window_radius) ? *last_index_ - config_.window_radius : 0;
        end = std::min(route_.entries.size(), *last_index_ + config_.window_radius + 1);
        if (config_.directional_search_direction < 0) {
            end = std::min(end, *last_index_ + 1);
        } else if (config_.directional_search_direction > 0) {
            begin = std::max(begin, *last_index_);
        }
    }

    double best_distance = std::numeric_limits<double>::infinity();
    double best_raw_distance = std::numeric_limits<double>::infinity();
    std::size_t best_index = begin;
    bool evaluated_candidate = false;
    for (std::size_t index = begin; index < end; ++index) {
        const auto progress = route_progress_for_index(route_, index);
        if (!last_index_.has_value() && config_.initial_progress_window_enabled
            && (progress < config_.initial_progress_min || progress > config_.initial_progress_max)) {
            continue;
        }
        const auto& entry = route_.entries[index];
        validate_frame_against_entry(frame, entry);
        const auto distance = normalized_mean_absolute_difference(frame.data, entry.payload);
        evaluated_candidate = true;
        double ranked_distance = distance;
        if (last_index_.has_value() && config_.directional_search_bias > 0.0
            && config_.directional_search_direction != 0) {
            const bool moves_in_expected_direction =
                (config_.directional_search_direction < 0 && index < *last_index_)
                || (config_.directional_search_direction > 0 && index > *last_index_);
            if (moves_in_expected_direction) {
                const auto step_fraction = static_cast<double>(
                    index > *last_index_ ? index - *last_index_ : *last_index_ - index)
                    / static_cast<double>(std::max<std::size_t>(route_.entries.size() - 1, 1));
                ranked_distance -= config_.directional_search_bias * step_fraction;
            }
        }
        insert_top_candidate(
            recent_top_candidates_,
            config_.top_candidate_count,
            candidate_from_distance(route_, index, distance));
        if (ranked_distance < best_distance) {
            best_distance = ranked_distance;
            best_raw_distance = distance;
            best_index = index;
        }
    }
    if (!evaluated_candidate) {
        throw std::runtime_error("Gray8 route matcher initial progress window selected no route entries");
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
                best_raw_distance = distance;
                best_index = index;
            }
        }
    }

    const auto confidence = std::clamp(1.0 - best_raw_distance, 0.0, 1.0);
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

std::vector<RouteMatchZoneCandidate> Gray8RouteMatcher::probe_progress_zones(const Frame& frame) const {
    std::vector<RouteMatchZoneCandidate> zones{
        {"start", 0.0, 0.20, {}, false},
        {"early", 0.20, 0.40, {}, false},
        {"mid", 0.40, 0.60, {}, false},
        {"late", 0.60, 0.80, {}, false},
        {"end", 0.80, 1.0, {}, false},
    };

    std::vector<double> best_distances(zones.size(), std::numeric_limits<double>::infinity());
    for (std::size_t index = 0; index < route_.entries.size(); ++index) {
        const auto& entry = route_.entries[index];
        validate_frame_against_entry(frame, entry);
        const auto progress = route_.entries.size() > 1
            ? static_cast<double>(index) / static_cast<double>(route_.entries.size() - 1)
            : 1.0;
        const auto distance = normalized_mean_absolute_difference(frame.data, entry.payload);
        for (std::size_t zone_index = 0; zone_index < zones.size(); ++zone_index) {
            const auto& zone = zones[zone_index];
            const bool in_zone = zone_index + 1 == zones.size()
                ? (progress >= zone.start_progress && progress <= zone.end_progress)
                : (progress >= zone.start_progress && progress < zone.end_progress);
            if (in_zone && distance < best_distances[zone_index]) {
                best_distances[zone_index] = distance;
                zones[zone_index].candidate = candidate_from_distance(route_, index, distance);
                zones[zone_index].valid = true;
            }
        }
    }

    return zones;
}

RouteMatchEdgeDiagnostics Gray8RouteMatcher::probe_edge_diagnostics(
    const Frame& frame,
    std::size_t top_candidate_count) const {
    RouteMatchEdgeDiagnostics diagnostics;
    diagnostics.zone_candidates = {
        {"start", 0.0, 0.20, {}, false},
        {"early", 0.20, 0.40, {}, false},
        {"mid", 0.40, 0.60, {}, false},
        {"late", 0.60, 0.80, {}, false},
        {"end", 0.80, 1.0, {}, false},
    };

    const auto current_edges = gray8_edge_payload(frame.width, frame.height, frame.data);
    std::vector<double> best_distances(
        diagnostics.zone_candidates.size(),
        std::numeric_limits<double>::infinity());
    for (std::size_t index = 0; index < route_.entries.size(); ++index) {
        const auto& entry = route_.entries[index];
        validate_frame_against_entry(frame, entry);
        const auto distance = normalized_mean_absolute_difference(current_edges, route_edge_payloads_[index]);
        const auto candidate = candidate_from_distance(route_, index, distance);
        insert_top_candidate(diagnostics.top_candidates, top_candidate_count, candidate);
        const auto progress = candidate.progress;
        for (std::size_t zone_index = 0; zone_index < diagnostics.zone_candidates.size(); ++zone_index) {
            const auto& zone = diagnostics.zone_candidates[zone_index];
            const bool in_zone = zone_index + 1 == diagnostics.zone_candidates.size()
                ? (progress >= zone.start_progress && progress <= zone.end_progress)
                : (progress >= zone.start_progress && progress < zone.end_progress);
            if (in_zone && distance < best_distances[zone_index]) {
                best_distances[zone_index] = distance;
                diagnostics.zone_candidates[zone_index].candidate = candidate;
                diagnostics.zone_candidates[zone_index].valid = true;
            }
        }
    }

    return diagnostics;
}

} // namespace vh
