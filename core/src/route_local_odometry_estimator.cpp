#include "visual_homing/route_local_odometry_estimator.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

#include "visual_homing/coordinate_frames.hpp"

namespace vh {
namespace {

bool finite_non_negative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool finite_positive(double value) {
    return std::isfinite(value) && value > 0.0;
}

void validate_config(const RouteLocalOdometryEstimatorConfig& config) {
    if (!finite_positive(config.nominal_route_length_m)) {
        throw std::invalid_argument("Route-local estimator nominal_route_length_m must be finite and positive");
    }
    if (!finite_non_negative(config.minimum_match_confidence)
        || config.minimum_match_confidence > 1.0) {
        throw std::invalid_argument("Route-local estimator minimum_match_confidence must be in [0, 1]");
    }
    if (!finite_non_negative(config.maximum_match_age_ms)
        || !finite_non_negative(config.maximum_altitude_age_ms)) {
        throw std::invalid_argument("Route-local estimator freshness limits must be finite and non-negative");
    }
    if (!finite_non_negative(config.minimum_update_interval_ms)
        || !finite_positive(config.maximum_update_interval_ms)
        || config.minimum_update_interval_ms > config.maximum_update_interval_ms) {
        throw std::invalid_argument("Route-local estimator update interval limits are invalid");
    }
    if (!finite_positive(config.maximum_horizontal_rate_mps)
        || !finite_positive(config.maximum_vertical_rate_mps)
        || !finite_positive(config.maximum_yaw_rate_rad_s)) {
        throw std::invalid_argument("Route-local estimator rate limits must be finite and positive");
    }
    if (!finite_positive(config.maximum_direction_error_rad)
        || config.maximum_direction_error_rad > std::numbers::pi) {
        throw std::invalid_argument("Route-local estimator maximum_direction_error_rad must be in (0, pi]");
    }
    if (!finite_non_negative(config.maximum_progress_direction_error)
        || config.maximum_progress_direction_error > 1.0) {
        throw std::invalid_argument("Route-local estimator progress direction tolerance must be in [0, 1]");
    }
    if (config.maximum_consecutive_invalid_updates == 0) {
        throw std::invalid_argument("Route-local estimator invalid update limit must be positive");
    }
    if (config.reverse_direction_error_sign != -1
        && config.reverse_direction_error_sign != 1) {
        throw std::invalid_argument("Route-local estimator reverse direction error sign must be -1 or 1");
    }
}

} // namespace

RouteLocalOdometryEstimator::RouteLocalOdometryEstimator(
    RouteLocalOdometryEstimatorConfig config)
    : config_(std::move(config)) {
    validate_config(config_);
}

void RouteLocalOdometryEstimator::initialize_start_altitude(
    double altitude_m,
    Timestamp observation_timestamp,
    Timestamp current_time) {
    if (start_altitude_initialized_) {
        throw std::logic_error("Route-local estimator start altitude is already initialized");
    }
    if (!std::isfinite(altitude_m)) {
        throw std::invalid_argument("Route-local estimator start altitude must be finite");
    }
    const auto age_ms = milliseconds_between(observation_timestamp, current_time);
    if (age_ms < 0.0) {
        throw std::invalid_argument("Route-local estimator start altitude timestamp is in the future");
    }
    if (age_ms > config_.maximum_altitude_age_ms) {
        throw std::invalid_argument("Route-local estimator start altitude is stale");
    }

    start_altitude_m_ = altitude_m;
    start_altitude_initialized_ = true;
    consecutive_invalid_updates_ = 0;
    reset_required_ = false;
    last_accepted_.reset();
}

void RouteLocalOdometryEstimator::reset_tracking() {
    reset_counter_ = static_cast<std::uint8_t>(reset_counter_ + 1U);
    consecutive_invalid_updates_ = 0;
    reset_required_ = false;
    last_accepted_.reset();
}

void RouteLocalOdometryEstimator::clear_start_altitude() {
    reset_tracking();
    start_altitude_initialized_ = false;
    start_altitude_m_ = 0.0;
}

RouteLocalOdometryEstimatorResult RouteLocalOdometryEstimator::reject(
    RouteLocalOdometryEstimatorResult result,
    std::string reason,
    bool count_invalid_update) {
    result.estimate.valid_for_fc = false;
    result.estimate.reset_counter = reset_counter_;
    result.reason = std::move(reason);

    if (count_invalid_update) {
        ++consecutive_invalid_updates_;
        if (consecutive_invalid_updates_ >= config_.maximum_consecutive_invalid_updates) {
            reset_required_ = true;
        }
    }

    result.consecutive_invalid_updates = consecutive_invalid_updates_;
    result.reset_required = reset_required_;
    return result;
}

RouteLocalOdometryEstimatorResult RouteLocalOdometryEstimator::update(
    const RouteLocalOdometryObservation& observation) {
    RouteLocalOdometryEstimatorResult result;
    result.direction = observation.direction;
    result.route_progress = observation.match.progress;
    result.start_altitude_m = start_altitude_m_;
    result.current_altitude_m = observation.altitude_m;
    result.start_altitude_initialized = start_altitude_initialized_;
    result.reset_required = reset_required_;
    result.consecutive_invalid_updates = consecutive_invalid_updates_;
    result.estimate.reset_counter = reset_counter_;

    if (!start_altitude_initialized_) {
        return reject(std::move(result), "start_altitude_not_initialized", false);
    }
    if (reset_required_) {
        return reject(std::move(result), "tracking_reset_required", false);
    }
    if (!observation.health_ready) {
        return reject(std::move(result), "health_not_ready", true);
    }
    if (!observation.match.valid
        || !std::isfinite(observation.match.progress)
        || observation.match.progress < 0.0
        || observation.match.progress > 1.0
        || !std::isfinite(observation.match.confidence)
        || observation.match.confidence < config_.minimum_match_confidence) {
        return reject(std::move(result), "route_match_not_valid", true);
    }

    result.match_age_ms = milliseconds_between(observation.match.timestamp, observation.timestamp);
    if (result.match_age_ms < 0.0) {
        return reject(std::move(result), "route_match_from_future", true);
    }
    if (result.match_age_ms > config_.maximum_match_age_ms) {
        return reject(std::move(result), "route_match_not_fresh", true);
    }
    if (!observation.altitude_valid || !std::isfinite(observation.altitude_m)) {
        return reject(std::move(result), "altitude_not_valid", true);
    }

    result.altitude_age_ms = milliseconds_between(observation.altitude_timestamp, observation.timestamp);
    if (result.altitude_age_ms < 0.0) {
        return reject(std::move(result), "altitude_from_future", true);
    }
    if (result.altitude_age_ms > config_.maximum_altitude_age_ms) {
        return reject(std::move(result), "altitude_not_fresh", true);
    }
    if (!observation.match.direction_observation_valid
        || !std::isfinite(observation.match.direction_error_rad)) {
        return reject(std::move(result), "direction_observation_not_valid", true);
    }
    if (std::abs(observation.match.direction_error_rad) > config_.maximum_direction_error_rad) {
        return reject(std::move(result), "direction_error_out_of_range", true);
    }
    if (observation.direction == RouteTravelDirection::reverse
        && config_.reverse_yaw_policy == ReverseYawPolicy::unavailable) {
        return reject(std::move(result), "reverse_yaw_policy_unavailable", true);
    }

    result.estimate.x_m = observation.match.progress * config_.nominal_route_length_m;
    result.estimate.y_m = 0.0;
    result.estimate.z_m = start_altitude_m_ - observation.altitude_m;
    const auto base_yaw_rad = observation.direction == RouteTravelDirection::forward
        ? 0.0
        : std::numbers::pi;
    const auto direction_sign = observation.direction == RouteTravelDirection::forward
        ? 1.0
        : static_cast<double>(config_.reverse_direction_error_sign);
    result.estimate.yaw_rad = wrap_angle_pi(
        base_yaw_rad + direction_sign * observation.match.direction_error_rad);

    if (last_accepted_.has_value()) {
        if (observation.direction != last_accepted_->direction) {
            reset_required_ = true;
            return reject(std::move(result), "direction_change_requires_reset", true);
        }
        result.update_interval_ms = milliseconds_between(
            last_accepted_->timestamp,
            observation.timestamp);
        if (result.update_interval_ms <= 0.0) {
            return reject(std::move(result), "timestamp_not_monotonic", true);
        }
        if (result.update_interval_ms < config_.minimum_update_interval_ms) {
            return reject(std::move(result), "update_interval_too_short", true);
        }
        if (result.update_interval_ms > config_.maximum_update_interval_ms) {
            return reject(std::move(result), "update_interval_too_long", true);
        }

        const auto tolerance = config_.maximum_progress_direction_error;
        const auto direction_wrong = observation.direction == RouteTravelDirection::forward
            ? observation.match.progress + tolerance < last_accepted_->progress
            : observation.match.progress > last_accepted_->progress + tolerance;
        if (direction_wrong) {
            return reject(std::move(result), "progress_direction_violation", true);
        }

        const auto interval_s = result.update_interval_ms / 1000.0;
        result.horizontal_rate_mps = std::abs(result.estimate.x_m - last_accepted_->x_m) / interval_s;
        result.vertical_rate_mps = std::abs(result.estimate.z_m - last_accepted_->z_m) / interval_s;
        result.yaw_rate_rad_s = std::abs(wrap_angle_pi(
            result.estimate.yaw_rad - last_accepted_->yaw_rad)) / interval_s;
        if (result.horizontal_rate_mps > config_.maximum_horizontal_rate_mps) {
            return reject(std::move(result), "horizontal_rate_exceeded", true);
        }
        if (result.vertical_rate_mps > config_.maximum_vertical_rate_mps) {
            return reject(std::move(result), "vertical_rate_exceeded", true);
        }
        if (result.yaw_rate_rad_s > config_.maximum_yaw_rate_rad_s) {
            return reject(std::move(result), "yaw_rate_exceeded", true);
        }
    }

    result.estimate.valid_for_fc = true;
    result.reason = "valid";
    consecutive_invalid_updates_ = 0;
    result.consecutive_invalid_updates = 0;
    result.reset_required = false;
    last_accepted_ = AcceptedState{
        observation.timestamp,
        observation.direction,
        observation.match.progress,
        result.estimate.x_m,
        result.estimate.z_m,
        result.estimate.yaw_rad,
    };
    return result;
}

bool RouteLocalOdometryEstimator::start_altitude_initialized() const {
    return start_altitude_initialized_;
}

bool RouteLocalOdometryEstimator::reset_required() const {
    return reset_required_;
}

std::uint8_t RouteLocalOdometryEstimator::reset_counter() const {
    return reset_counter_;
}

} // namespace vh
