#include "visual_homing/route_artifact_check.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

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

std::vector<std::uint8_t> brightness_offset(std::vector<std::uint8_t> data, int delta) {
    for (auto& value : data) {
        value = static_cast<std::uint8_t>(std::clamp(static_cast<int>(value) + delta, 0, 255));
    }
    return data;
}

std::vector<std::uint8_t> deterministic_noise(std::vector<std::uint8_t> data, int delta) {
    if (delta <= 0) {
        return data;
    }

    for (std::size_t index = 0; index < data.size(); ++index) {
        const int sign = (index % 2 == 0) ? delta : -delta;
        data[index] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(data[index]) + sign, 0, 255));
    }
    return data;
}

std::vector<std::uint8_t> horizontal_shift(
    const RouteSignatureEntry& entry,
    int shift_px) {
    std::vector<std::uint8_t> shifted(entry.payload.size());
    for (int y = 0; y < entry.height; ++y) {
        for (int x = 0; x < entry.width; ++x) {
            const int source_x = std::clamp(x + shift_px, 0, static_cast<int>(entry.width) - 1);
            const auto target_index = static_cast<std::size_t>(y) * static_cast<std::size_t>(entry.width)
                + static_cast<std::size_t>(x);
            const auto source_index = static_cast<std::size_t>(y) * static_cast<std::size_t>(entry.width)
                + static_cast<std::size_t>(source_x);
            shifted[target_index] = entry.payload[source_index];
        }
    }
    return shifted;
}

Frame perturbed_frame(
    const RouteSignatureEntry& entry,
    std::vector<std::uint8_t> data) {
    auto frame = frame_from_entry(entry);
    frame.data = std::move(data);
    return frame;
}

double mean_abs_diff_bytes(
    const std::vector<std::uint8_t>& left,
    const std::vector<std::uint8_t>& right) {
    if (left.size() != right.size()) {
        throw std::runtime_error("Route distinctiveness requires equal payload sizes");
    }
    if (left.empty()) {
        throw std::runtime_error("Route distinctiveness requires non-empty payloads");
    }

    std::uint64_t sum = 0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        sum += static_cast<std::uint64_t>(std::abs(static_cast<int>(left[index]) - static_cast<int>(right[index])));
    }
    return static_cast<double>(sum) / static_cast<double>(left.size());
}

double payload_range(const std::vector<std::uint8_t>& payload) {
    if (payload.empty()) {
        throw std::runtime_error("Route distinctiveness requires non-empty payloads");
    }
    const auto [min_it, max_it] = std::minmax_element(payload.begin(), payload.end());
    return static_cast<double>(*max_it) - static_cast<double>(*min_it);
}

void append_sample_frame_id(std::vector<std::uint64_t>& frame_ids, std::uint64_t frame_id) {
    constexpr std::size_t max_sample_ids = 8;
    if (frame_ids.size() < max_sample_ids) {
        frame_ids.push_back(frame_id);
    }
}

} // namespace

RouteSelfMatchSummary self_match_route_signature(
    const RouteSignatureFile& route,
    const RouteSelfMatchConfig& config) {
    if (route.entries.empty()) {
        throw std::invalid_argument("Route self-match requires at least one route entry");
    }

    Gray8RouteMatcher matcher(route, {
        .window_radius = 0,
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
        && summary.minimum_confidence_seen >= config.minimum_confidence;
    return summary;
}

RouteSelfMatchSummary self_match_route_signature_file(
    const std::filesystem::path& path,
    const RouteSelfMatchConfig& config) {
    return self_match_route_signature(read_route_signature_file(path), config);
}

RoutePerturbationCheckSummary perturbation_check_route_signature(
    const RouteSignatureFile& route,
    const RoutePerturbationCheckConfig& config) {
    if (route.entries.empty()) {
        throw std::invalid_argument("Route perturbation check requires at least one route entry");
    }
    if (config.shift_px < 0 || config.max_direction_shift_px < 0) {
        throw std::invalid_argument("Route perturbation shift settings must be non-negative");
    }
    if (config.radians_per_pixel < 0.0) {
        throw std::invalid_argument("Route perturbation radians_per_pixel must be non-negative");
    }

    Gray8RouteMatcher matcher(route, {
        .window_radius = 0,
        .minimum_confidence = config.minimum_confidence,
        .max_direction_shift_px = config.max_direction_shift_px,
        .radians_per_pixel = config.radians_per_pixel,
    });

    RoutePerturbationCheckSummary summary;
    summary.minimum_brightness_confidence = 1.0;
    summary.minimum_noise_confidence = 1.0;
    summary.minimum_shift_confidence = 1.0;

    for (const auto& entry : route.entries) {
        const auto brightness = matcher.match(perturbed_frame(
            entry,
            brightness_offset(entry.payload, config.brightness_delta)));
        const auto noisy = matcher.match(perturbed_frame(
            entry,
            deterministic_noise(entry.payload, config.noise_delta)));
        const auto shifted = matcher.match(perturbed_frame(
            entry,
            horizontal_shift(entry, config.shift_px)));

        ++summary.entries_checked;
        if (brightness.valid) {
            ++summary.brightness_valid_matches;
        }
        if (noisy.valid) {
            ++summary.noise_valid_matches;
        }
        if (shifted.valid) {
            ++summary.shift_valid_matches;
        }
        if (config.shift_px == 0 || std::fabs(shifted.direction_error_rad) > 0.0) {
            ++summary.shift_direction_matches;
        }
        summary.minimum_brightness_confidence = std::min(summary.minimum_brightness_confidence, brightness.confidence);
        summary.minimum_noise_confidence = std::min(summary.minimum_noise_confidence, noisy.confidence);
        summary.minimum_shift_confidence = std::min(summary.minimum_shift_confidence, shifted.confidence);
    }

    try {
        auto malformed = frame_from_entry(route.entries.front());
        if (!malformed.data.empty()) {
            malformed.data.pop_back();
        }
        (void)matcher.match(malformed);
    } catch (const std::runtime_error&) {
        summary.malformed_rejected = true;
    }

    summary.passed = summary.brightness_valid_matches == summary.entries_checked
        && summary.noise_valid_matches == summary.entries_checked
        && summary.shift_valid_matches == summary.entries_checked
        && summary.minimum_brightness_confidence >= config.minimum_confidence
        && summary.minimum_noise_confidence >= config.minimum_confidence
        && summary.minimum_shift_confidence >= config.minimum_confidence
        && summary.malformed_rejected;
    return summary;
}

RoutePerturbationCheckSummary perturbation_check_route_signature_file(
    const std::filesystem::path& path,
    const RoutePerturbationCheckConfig& config) {
    return perturbation_check_route_signature(read_route_signature_file(path), config);
}

RouteDistinctivenessSummary analyze_route_distinctiveness(
    const RouteSignatureFile& route,
    const RouteDistinctivenessConfig& config) {
    if (route.entries.empty()) {
        throw std::invalid_argument("Route distinctiveness analysis requires at least one route entry");
    }
    if (config.low_texture_range_threshold < 0.0
        || config.ambiguous_mean_abs_diff_threshold < 0.0
        || config.maximum_low_texture_fraction < 0.0
        || config.maximum_ambiguous_nearest_fraction < 0.0
        || config.minimum_average_nearest_mean_abs_diff < 0.0) {
        throw std::invalid_argument("Route distinctiveness thresholds must be non-negative");
    }
    if (config.maximum_low_texture_fraction > 1.0 || config.maximum_ambiguous_nearest_fraction > 1.0) {
        throw std::invalid_argument("Route distinctiveness fraction thresholds must be <= 1.0");
    }

    RouteDistinctivenessSummary summary;
    summary.entries_checked = static_cast<std::uint64_t>(route.entries.size());
    summary.minimum_payload_range = std::numeric_limits<double>::max();

    double range_sum = 0.0;
    for (const auto& entry : route.entries) {
        const auto range = payload_range(entry.payload);
        summary.minimum_payload_range = std::min(summary.minimum_payload_range, range);
        range_sum += range;
        if (range <= config.low_texture_range_threshold) {
            ++summary.low_texture_entries;
            append_sample_frame_id(summary.low_texture_frame_ids, entry.frame_id);
        }
    }
    summary.average_payload_range = range_sum / static_cast<double>(summary.entries_checked);
    summary.low_texture_fraction = static_cast<double>(summary.low_texture_entries)
        / static_cast<double>(summary.entries_checked);

    if (route.entries.size() >= 2) {
        summary.minimum_adjacent_mean_abs_diff = std::numeric_limits<double>::max();
        double adjacent_sum = 0.0;
        for (std::size_t index = 1; index < route.entries.size(); ++index) {
            const auto diff = mean_abs_diff_bytes(route.entries[index - 1].payload, route.entries[index].payload);
            summary.minimum_adjacent_mean_abs_diff = std::min(summary.minimum_adjacent_mean_abs_diff, diff);
            adjacent_sum += diff;
            ++summary.adjacent_pairs_checked;
        }
        summary.average_adjacent_mean_abs_diff = adjacent_sum / static_cast<double>(summary.adjacent_pairs_checked);

        summary.minimum_nearest_mean_abs_diff = std::numeric_limits<double>::max();
        double nearest_sum = 0.0;
        for (std::size_t index = 0; index < route.entries.size(); ++index) {
            double nearest = std::numeric_limits<double>::max();
            for (std::size_t candidate = 0; candidate < route.entries.size(); ++candidate) {
                if (candidate == index) {
                    continue;
                }
                nearest = std::min(nearest, mean_abs_diff_bytes(route.entries[index].payload, route.entries[candidate].payload));
            }

            summary.minimum_nearest_mean_abs_diff = std::min(summary.minimum_nearest_mean_abs_diff, nearest);
            nearest_sum += nearest;
            if (nearest == 0.0) {
                ++summary.exact_duplicate_entries;
                append_sample_frame_id(summary.exact_duplicate_frame_ids, route.entries[index].frame_id);
            }
            if (nearest <= config.ambiguous_mean_abs_diff_threshold) {
                ++summary.ambiguous_nearest_entries;
                append_sample_frame_id(summary.ambiguous_nearest_frame_ids, route.entries[index].frame_id);
            }
        }
        summary.average_nearest_mean_abs_diff = nearest_sum / static_cast<double>(summary.entries_checked);
    }
    summary.ambiguous_nearest_fraction = static_cast<double>(summary.ambiguous_nearest_entries)
        / static_cast<double>(summary.entries_checked);

    summary.warning = summary.low_texture_entries > 0
        || summary.exact_duplicate_entries > 0
        || summary.ambiguous_nearest_entries > 0;
    summary.quality_pass = summary.low_texture_fraction <= config.maximum_low_texture_fraction
        && summary.ambiguous_nearest_fraction <= config.maximum_ambiguous_nearest_fraction
        && summary.average_nearest_mean_abs_diff >= config.minimum_average_nearest_mean_abs_diff
        && (config.allow_exact_duplicates || summary.exact_duplicate_entries == 0);
    return summary;
}

RouteDistinctivenessSummary analyze_route_distinctiveness_file(
    const std::filesystem::path& path,
    const RouteDistinctivenessConfig& config) {
    return analyze_route_distinctiveness(read_route_signature_file(path), config);
}

} // namespace vh
