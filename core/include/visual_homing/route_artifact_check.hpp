#pragma once

#include <cstdint>
#include <filesystem>

#include "visual_homing/route_signature.hpp"

namespace vh {

struct RouteSelfMatchConfig {
    double minimum_confidence = 0.99;
    int max_direction_shift_px = 0;
    double radians_per_pixel = 0.0;
};

struct RouteSelfMatchSummary {
    std::uint64_t entries_checked = 0;
    std::uint64_t valid_matches = 0;
    std::uint64_t exact_index_matches = 0;
    double minimum_confidence_seen = 0.0;
    double average_confidence = 0.0;
    double last_progress = 0.0;
    bool progress_monotonic = true;
    bool passed = false;
};

struct RoutePerturbationCheckConfig {
    double minimum_confidence = 0.90;
    int brightness_delta = 8;
    int noise_delta = 3;
    int shift_px = 1;
    int max_direction_shift_px = 2;
    double radians_per_pixel = 0.05;
};

struct RoutePerturbationCheckSummary {
    std::uint64_t entries_checked = 0;
    std::uint64_t brightness_valid_matches = 0;
    std::uint64_t noise_valid_matches = 0;
    std::uint64_t shift_valid_matches = 0;
    std::uint64_t shift_direction_matches = 0;
    double minimum_brightness_confidence = 0.0;
    double minimum_noise_confidence = 0.0;
    double minimum_shift_confidence = 0.0;
    bool malformed_rejected = false;
    bool passed = false;
};

RouteSelfMatchSummary self_match_route_signature(
    const RouteSignatureFile& route,
    const RouteSelfMatchConfig& config = {});
RouteSelfMatchSummary self_match_route_signature_file(
    const std::filesystem::path& path,
    const RouteSelfMatchConfig& config = {});
RoutePerturbationCheckSummary perturbation_check_route_signature(
    const RouteSignatureFile& route,
    const RoutePerturbationCheckConfig& config = {});
RoutePerturbationCheckSummary perturbation_check_route_signature_file(
    const std::filesystem::path& path,
    const RoutePerturbationCheckConfig& config = {});

} // namespace vh
