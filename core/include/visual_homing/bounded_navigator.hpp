#pragma once

#include "visual_homing/interfaces.hpp"

namespace vh {

struct BoundedNavigatorConfig {
    double minimum_confidence = 0.70;
    double max_match_age_ms = 250.0;
    double yaw_gain = 1.0;
    double max_yaw_rate_radps = 0.35;
    double forward_speed_mps = 0.0;
};

class BoundedNavigator final : public Navigator {
public:
    explicit BoundedNavigator(BoundedNavigatorConfig config);

    NavigationCommand update(const RouteMatch& match, const HealthSnapshot& health) override;

private:
    BoundedNavigatorConfig config_;
};

} // namespace vh
