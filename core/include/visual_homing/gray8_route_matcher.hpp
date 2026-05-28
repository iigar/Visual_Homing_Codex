#pragma once

#include <cstddef>
#include <optional>

#include "visual_homing/interfaces.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

struct Gray8RouteMatcherConfig {
    std::size_t window_radius = 0;
    double minimum_confidence = 0.0;
    int max_direction_shift_px = 0;
    double radians_per_pixel = 0.0;
};

class Gray8RouteMatcher final : public RouteMatcher {
public:
    Gray8RouteMatcher(RouteSignatureFile route, Gray8RouteMatcherConfig config);

    RouteMatch match(const Frame& frame) override;

private:
    RouteSignatureFile route_;
    Gray8RouteMatcherConfig config_;
    std::optional<std::size_t> last_index_;
};

} // namespace vh
