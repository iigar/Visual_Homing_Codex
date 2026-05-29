#include <cassert>
#include <chrono>
#include <stdexcept>

#include "visual_homing/bounded_navigator.hpp"

namespace {

vh::Timestamp at_ms(int milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(std::chrono::milliseconds(milliseconds)));
}

vh::HealthSnapshot ready_health(int timestamp_ms, double confidence) {
    vh::HealthSnapshot health;
    health.state = vh::HealthState::Ready;
    health.timestamp = at_ms(timestamp_ms);
    health.route_match_confidence = confidence;
    health.camera_ok = true;
    health.mavlink_ok = true;
    health.navigation_ok = true;
    return health;
}

vh::RouteMatch valid_match(int timestamp_ms, double direction_error_rad, double confidence) {
    vh::RouteMatch match;
    match.timestamp = at_ms(timestamp_ms);
    match.direction_error_rad = direction_error_rad;
    match.confidence = confidence;
    match.valid = true;
    return match;
}

} // namespace

int main() {
    vh::BoundedNavigator navigator({
        .minimum_confidence = 0.75,
        .max_match_age_ms = 200.0,
        .yaw_gain = 2.0,
        .max_yaw_rate_radps = 0.30,
        .forward_speed_mps = 4.0,
    });

    const auto bounded = navigator.update(valid_match(900, 0.25, 0.90), ready_health(1000, 0.85));
    assert(bounded.valid);
    assert(bounded.vx_mps == 4.0);
    assert(bounded.vy_mps == 0.0);
    assert(bounded.yaw_rate_radps > 0.299);
    assert(bounded.yaw_rate_radps < 0.301);
    assert(bounded.confidence > 0.849);
    assert(bounded.confidence < 0.851);

    const auto negative = navigator.update(valid_match(950, -0.05, 0.95), ready_health(1000, 0.95));
    assert(negative.valid);
    assert(negative.yaw_rate_radps < -0.099);
    assert(negative.yaw_rate_radps > -0.101);

    const auto low_confidence = navigator.update(valid_match(950, 0.10, 0.70), ready_health(1000, 0.95));
    assert(!low_confidence.valid);
    assert(low_confidence.yaw_rate_radps == 0.0);

    const auto stale = navigator.update(valid_match(700, 0.10, 0.95), ready_health(1000, 0.95));
    assert(!stale.valid);

    auto degraded = ready_health(1000, 0.95);
    degraded.state = vh::HealthState::Degraded;
    const auto health_blocked = navigator.update(valid_match(950, 0.10, 0.95), degraded);
    assert(!health_blocked.valid);

    bool rejected = false;
    try {
        (void)vh::BoundedNavigator({.minimum_confidence = 1.5});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    return 0;
}
