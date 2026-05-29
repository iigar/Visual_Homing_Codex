#include "visual_homing/bounded_navigator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "visual_homing/time.hpp"

namespace vh {
namespace {

bool health_allows_navigation(const HealthSnapshot& health) {
    return health.state == HealthState::Ready
        && health.camera_ok
        && health.mavlink_ok
        && health.navigation_ok;
}

} // namespace

BoundedNavigator::BoundedNavigator(BoundedNavigatorConfig config)
    : config_(config) {
    if (config_.minimum_confidence < 0.0 || config_.minimum_confidence > 1.0) {
        throw std::invalid_argument("BoundedNavigator minimum_confidence must be in [0, 1]");
    }
    if (config_.max_match_age_ms < 0.0) {
        throw std::invalid_argument("BoundedNavigator max_match_age_ms must be non-negative");
    }
    if (config_.max_yaw_rate_radps < 0.0) {
        throw std::invalid_argument("BoundedNavigator max_yaw_rate_radps must be non-negative");
    }
    if (config_.max_yaw_accel_radps2 < 0.0) {
        throw std::invalid_argument("BoundedNavigator max_yaw_accel_radps2 must be non-negative");
    }
    if (config_.forward_speed_mps < 0.0) {
        throw std::invalid_argument("BoundedNavigator forward_speed_mps must be non-negative");
    }
}

NavigationCommand BoundedNavigator::update(const RouteMatch& match, const HealthSnapshot& health) {
    NavigationCommand command;
    command.timestamp = health.timestamp;

    const auto match_age_ms = milliseconds_between(match.timestamp, health.timestamp);
    const auto confidence = std::min(match.confidence, health.route_match_confidence);

    if (!health_allows_navigation(health)
        || !match.valid
        || confidence < config_.minimum_confidence
        || match_age_ms < 0.0
        || match_age_ms > config_.max_match_age_ms) {
        command.confidence = std::max(0.0, confidence);
        command.valid = false;
        has_last_valid_command_ = false;
        return command;
    }

    const auto raw_yaw_rate = match.direction_error_rad * config_.yaw_gain;
    const auto bounded_yaw_rate = std::clamp(raw_yaw_rate, -config_.max_yaw_rate_radps, config_.max_yaw_rate_radps);
    if (has_last_valid_command_) {
        const auto dt_s = milliseconds_between(last_command_.timestamp, health.timestamp) / 1000.0;
        const auto max_delta = std::max(0.0, dt_s) * config_.max_yaw_accel_radps2;
        command.yaw_rate_radps = std::clamp(
            bounded_yaw_rate,
            last_command_.yaw_rate_radps - max_delta,
            last_command_.yaw_rate_radps + max_delta);
    } else {
        command.yaw_rate_radps = bounded_yaw_rate;
    }
    command.vx_mps = config_.forward_speed_mps;
    command.vy_mps = 0.0;
    command.confidence = confidence;
    command.valid = true;
    last_command_ = command;
    has_last_valid_command_ = true;
    return command;
}

} // namespace vh
