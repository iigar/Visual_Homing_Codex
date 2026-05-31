#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

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

struct RouteDistinctivenessConfig {
    double low_texture_range_threshold = 4.0;
    double ambiguous_mean_abs_diff_threshold = 2.0;
    double maximum_low_texture_fraction = 0.05;
    double maximum_ambiguous_nearest_fraction = 0.10;
    double minimum_average_nearest_mean_abs_diff = 5.0;
    bool allow_exact_duplicates = false;
    std::uint64_t edge_trim_entries = 0;
};

struct RouteDistinctivenessSummary {
    std::uint64_t entries_checked = 0;
    std::uint64_t entries_ignored_at_start = 0;
    std::uint64_t entries_ignored_at_end = 0;
    std::uint64_t adjacent_pairs_checked = 0;
    std::uint64_t low_texture_entries = 0;
    std::uint64_t exact_duplicate_entries = 0;
    std::uint64_t ambiguous_nearest_entries = 0;
    double low_texture_fraction = 0.0;
    double ambiguous_nearest_fraction = 0.0;
    double minimum_payload_range = 0.0;
    double average_payload_range = 0.0;
    double minimum_adjacent_mean_abs_diff = 0.0;
    double average_adjacent_mean_abs_diff = 0.0;
    double minimum_nearest_mean_abs_diff = 0.0;
    double average_nearest_mean_abs_diff = 0.0;
    std::vector<std::uint64_t> low_texture_frame_ids;
    std::vector<std::uint64_t> exact_duplicate_frame_ids;
    std::vector<std::uint64_t> ambiguous_nearest_frame_ids;
    bool warning = false;
    bool quality_pass = false;
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
RouteDistinctivenessSummary analyze_route_distinctiveness(
    const RouteSignatureFile& route,
    const RouteDistinctivenessConfig& config = {});
RouteDistinctivenessSummary analyze_route_distinctiveness_file(
    const std::filesystem::path& path,
    const RouteDistinctivenessConfig& config = {});

} // namespace vh
