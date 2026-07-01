#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "visual_homing/interfaces.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

struct Gray8RouteMatcherConfig {
    std::size_t window_radius = 0;
    double minimum_confidence = 0.0;
    int max_direction_shift_px = 0;
    double radians_per_pixel = 0.0;
    bool enable_scale_refinement = false;
    std::size_t scale_refinement_radius = 1;
    std::size_t top_candidate_count = 0;
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
