#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "visual_homing/interfaces.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

// Shared by route refinement and live visual-scale diagnostics. Order is significant
// when several scales have the same distance: the first minimum wins.
inline constexpr double gray8_scale_candidates[] = {
    0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75,
    0.80, 0.85, 0.90, 0.95, 1.0, 1.05, 1.10, 1.15, 1.20, 1.25,
    1.30, 1.35, 1.40, 1.50};

// Low-level centered nearest-neighbor score for the candidate scales above.
// Callers validate matching Gray8 dimensions and complete width*height payloads.
// Nonpositive/nonfinite scale or no overlapping samples returns infinity.
double scaled_normalized_mean_absolute_difference(
    const Frame& current, const RouteSignatureEntry& reference, double scale_ratio);

struct Gray8RouteMatcherConfig {
    std::size_t window_radius = 0;
    double minimum_confidence = 0.0;
    int max_direction_shift_px = 0;
    double radians_per_pixel = 0.0;
    bool enable_scale_refinement = false;
    std::size_t scale_refinement_radius = 1;
    std::size_t top_candidate_count = 0;
    bool initial_progress_window_enabled = false;
    double initial_progress_min = 0.0;
    double initial_progress_max = 1.0;
    int directional_search_direction = 0;
    double directional_search_bias = 0.0;
};

class Gray8RouteMatcher final : public RouteMatcher {
public:
    Gray8RouteMatcher(RouteSignatureFile route, Gray8RouteMatcherConfig config);

    RouteMatch match(const Frame& frame) override;
    const std::vector<RouteMatchCandidate>& recent_top_candidates() const noexcept;
    std::vector<RouteMatchZoneCandidate> probe_progress_zones(const Frame& frame) const;
    RouteMatchEdgeDiagnostics probe_edge_diagnostics(const Frame& frame, std::size_t top_candidate_count) const;

private:
    RouteSignatureFile route_;
    Gray8RouteMatcherConfig config_;
    std::optional<std::size_t> last_index_;
    std::vector<RouteMatchCandidate> recent_top_candidates_;
    std::vector<std::vector<std::uint8_t>> route_edge_payloads_;
};

} // namespace vh
