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
        .max_yaw_accel_radps2 = 10.0,
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

    vh::BoundedNavigator negative_navigator({
        .minimum_confidence = 0.75,
        .max_match_age_ms = 200.0,
        .yaw_gain = 2.0,
        .max_yaw_rate_radps = 0.30,
        .max_yaw_accel_radps2 = 10.0,
        .forward_speed_mps = 4.0,
    });

    const auto negative = negative_navigator.update(valid_match(950, -0.05, 0.95), ready_health(1000, 0.95));
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

    vh::BoundedNavigator slew_limited({
        .minimum_confidence = 0.75,
        .max_match_age_ms = 200.0,
        .yaw_gain = 10.0,
        .max_yaw_rate_radps = 1.0,
        .max_yaw_accel_radps2 = 0.5,
        .forward_speed_mps = 0.0,
    });

    const auto first = slew_limited.update(valid_match(990, 0.10, 0.95), ready_health(1000, 0.95));
    assert(first.valid);
    assert(first.yaw_rate_radps > 0.99);
    assert(first.yaw_rate_radps < 1.01);

    const auto second = slew_limited.update(valid_match(1090, -0.10, 0.95), ready_health(1100, 0.95));
    assert(second.valid);
    assert(second.yaw_rate_radps > 0.949);
    assert(second.yaw_rate_radps < 0.951);

    auto degraded_after_slew = ready_health(1200, 0.95);
    degraded_after_slew.state = vh::HealthState::Degraded;
    const auto reset = slew_limited.update(valid_match(1190, -0.10, 0.95), degraded_after_slew);
    assert(!reset.valid);

    const auto after_reset = slew_limited.update(valid_match(1290, -0.10, 0.95), ready_health(1300, 0.95));
    assert(after_reset.valid);
    assert(after_reset.yaw_rate_radps < -0.99);
    assert(after_reset.yaw_rate_radps > -1.01);

    bool rejected = false;
    try {
        (void)vh::BoundedNavigator({.minimum_confidence = 1.5});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    return 0;
}
