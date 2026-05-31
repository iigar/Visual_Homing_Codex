#include "visual_homing/route_artifact_check.hpp"

#include <algorithm>
#include <cmath>
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

} // namespace

RouteSelfMatchSummary self_match_route_signature(
    const RouteSignatureFile& route,
    const RouteSelfMatchConfig& config) {
    if (route.entries.empty()) {
        throw std::invalid_argument("Route self-match requires at least one route entry");
    }

    Gray8RouteMatcher matcher(route, {
        .window_radius = 1,
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
        && summary.minimum_confidence_seen >= config.minimum_confidence
        && summary.progress_monotonic;
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
        .window_radius = 1,
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

} // namespace vh
