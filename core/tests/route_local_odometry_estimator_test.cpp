#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

#include "visual_homing/route_local_odometry_estimator.hpp"

namespace {

vh::Timestamp at_ms(int milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(
        std::chrono::milliseconds(milliseconds)));
}

vh::RouteLocalOdometryEstimatorConfig config() {
    vh::RouteLocalOdometryEstimatorConfig value;
    value.nominal_route_length_m = 10.0;
    value.minimum_match_confidence = 0.9;
    value.maximum_match_age_ms = 100.0;
    value.maximum_altitude_age_ms = 100.0;
    value.minimum_update_interval_ms = 10.0;
    value.maximum_update_interval_ms = 500.0;
    value.maximum_horizontal_rate_mps = 10.0;
    value.maximum_vertical_rate_mps = 3.0;
    value.maximum_yaw_rate_rad_s = 1.0;
    value.maximum_direction_error_rad = 0.5;
    value.maximum_progress_direction_error = 0.02;
    value.maximum_consecutive_invalid_updates = 2;
    return value;
}

vh::RouteLocalOdometryObservation observation(
    int milliseconds,
    double progress,
    double altitude_m,
    vh::RouteTravelDirection direction = vh::RouteTravelDirection::forward,
    double direction_error_rad = 0.1) {
    vh::RouteLocalOdometryObservation value;
    value.timestamp = at_ms(milliseconds);
    value.match.timestamp = at_ms(milliseconds - 10);
    value.match.route_index = static_cast<std::size_t>(progress * 100.0);
    value.match.progress = progress;
    value.match.direction_error_rad = direction_error_rad;
    value.match.direction_observation_valid = true;
    value.match.confidence = 0.95;
    value.match.valid = true;
    value.altitude_timestamp = at_ms(milliseconds - 5);
    value.altitude_m = altitude_m;
    value.altitude_valid = true;
    value.health_ready = true;
    value.direction = direction;
    return value;
}

void initialize(vh::RouteLocalOdometryEstimator& estimator) {
    estimator.initialize_start_altitude(0.5, at_ms(0), at_ms(10));
}

} // namespace

int main() {
    {
        vh::RouteLocalOdometryEstimator estimator(config());
        auto no_origin = estimator.update(observation(100, 0.25, 0.5));
        assert(!no_origin.estimate.valid_for_fc);
        assert(no_origin.reason == "start_altitude_not_initialized");
        assert(no_origin.consecutive_invalid_updates == 0);

        initialize(estimator);
        const auto first = estimator.update(observation(100, 0.25, 0.5));
        assert(first.estimate.valid_for_fc);
        assert(first.reason == "valid");
        assert(first.estimate.x_m == 2.5);
        assert(first.estimate.y_m == 0.0);
        assert(first.estimate.z_m == 0.0);
        assert(std::abs(first.estimate.yaw_rad - 0.1) < 1e-12);
        assert(first.estimate.reset_counter == 0);
        assert(first.match_age_ms == 10.0);
        assert(first.altitude_age_ms == 5.0);

        const auto climbed = estimator.update(observation(200, 0.30, 0.7, vh::RouteTravelDirection::forward, 0.15));
        assert(climbed.estimate.valid_for_fc);
        assert(std::abs(climbed.estimate.x_m - 3.0) < 1e-12);
        assert(std::abs(climbed.estimate.z_m - -0.2) < 1e-12);
        assert(std::abs(climbed.horizontal_rate_mps - 5.0) < 1e-12);
        assert(std::abs(climbed.vertical_rate_mps - 2.0) < 1e-12);
        assert(std::abs(climbed.yaw_rate_rad_s - 0.5) < 1e-12);

        const auto frame = vh::encode_mavlink2_route_local_odometry(
            climbed.estimate,
            200000ULL,
            4,
            191,
            1);
        assert(frame.size() == 244);
        assert(frame.at(240) == 0);
    }

    {
        vh::RouteLocalOdometryEstimator estimator(config());
        bool rejected = false;
        try {
            estimator.initialize_start_altitude(0.5, at_ms(20), at_ms(10));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);

        rejected = false;
        try {
            estimator.initialize_start_altitude(0.5, at_ms(0), at_ms(200));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    {
        vh::RouteLocalOdometryEstimator estimator(config());
        initialize(estimator);
        const auto reverse = estimator.update(observation(
            100,
            0.8,
            0.5,
            vh::RouteTravelDirection::reverse));
        assert(!reverse.estimate.valid_for_fc);
        assert(reverse.reason == "reverse_yaw_policy_unavailable");
    }

    {
        auto reverse_config = config();
        reverse_config.reverse_yaw_policy = vh::ReverseYawPolicy::nose_toward_route_start;
        vh::RouteLocalOdometryEstimator estimator(reverse_config);
        initialize(estimator);
        const auto reverse = estimator.update(observation(
            100,
            0.8,
            0.5,
            vh::RouteTravelDirection::reverse));
        assert(reverse.estimate.valid_for_fc);
        assert(std::abs(reverse.estimate.yaw_rad - (-std::numbers::pi + 0.1)) < 1e-12);

        const auto wrong_direction = estimator.update(observation(
            200,
            0.85,
            0.5,
            vh::RouteTravelDirection::reverse));
        assert(!wrong_direction.estimate.valid_for_fc);
        assert(wrong_direction.reason == "progress_direction_violation");
    }

    {
        auto direction_config = config();
        direction_config.reverse_yaw_policy = vh::ReverseYawPolicy::nose_toward_route_start;
        vh::RouteLocalOdometryEstimator estimator(direction_config);
        initialize(estimator);
        assert(estimator.update(observation(100, 0.50, 0.5)).estimate.valid_for_fc);
        const auto changed = estimator.update(observation(
            200,
            0.45,
            0.5,
            vh::RouteTravelDirection::reverse));
        assert(!changed.estimate.valid_for_fc);
        assert(changed.reason == "direction_change_requires_reset");
        assert(changed.reset_required);
        assert(estimator.reset_required());
    }

    {
        vh::RouteLocalOdometryEstimator estimator(config());
        initialize(estimator);
        assert(estimator.update(observation(100, 0.20, 0.5)).estimate.valid_for_fc);

        auto unhealthy = observation(200, 0.25, 0.5);
        unhealthy.health_ready = false;
        const auto first_invalid = estimator.update(unhealthy);
        assert(first_invalid.reason == "health_not_ready");
        assert(first_invalid.consecutive_invalid_updates == 1);
        assert(!first_invalid.reset_required);

        unhealthy.timestamp = at_ms(300);
        unhealthy.match.timestamp = at_ms(290);
        unhealthy.altitude_timestamp = at_ms(295);
        const auto second_invalid = estimator.update(unhealthy);
        assert(second_invalid.reason == "health_not_ready");
        assert(second_invalid.consecutive_invalid_updates == 2);
        assert(second_invalid.reset_required);

        const auto blocked = estimator.update(observation(400, 0.30, 0.5));
        assert(blocked.reason == "tracking_reset_required");
        assert(blocked.reset_required);

        estimator.reset_tracking();
        assert(estimator.reset_counter() == 1);
        assert(estimator.start_altitude_initialized());
        const auto recovered = estimator.update(observation(400, 0.30, 0.5));
        assert(recovered.estimate.valid_for_fc);
        assert(recovered.estimate.reset_counter == 1);

        estimator.clear_start_altitude();
        assert(!estimator.start_altitude_initialized());
        assert(estimator.reset_counter() == 2);
        assert(estimator.update(observation(500, 0.35, 0.5)).reason == "start_altitude_not_initialized");
    }

    {
        vh::RouteLocalOdometryEstimator estimator(config());
        initialize(estimator);
        assert(estimator.update(observation(100, 0.20, 0.5)).estimate.valid_for_fc);
        const auto too_fast = estimator.update(observation(105, 0.21, 0.5));
        assert(too_fast.reason == "update_interval_too_short");
        assert(too_fast.consecutive_invalid_updates == 1);
        const auto recovered = estimator.update(observation(120, 0.21, 0.5));
        assert(recovered.estimate.valid_for_fc);
        assert(recovered.consecutive_invalid_updates == 0);
    }

    {
        vh::RouteLocalOdometryEstimator estimator(config());
        initialize(estimator);
        assert(estimator.update(observation(100, 0.50, 0.5)).estimate.valid_for_fc);
        const auto regression = estimator.update(observation(200, 0.40, 0.5));
        assert(regression.reason == "progress_direction_violation");
    }

    {
        auto rate_config = config();
        rate_config.maximum_horizontal_rate_mps = 1.0;
        vh::RouteLocalOdometryEstimator estimator(rate_config);
        initialize(estimator);
        assert(estimator.update(observation(100, 0.10, 0.5)).estimate.valid_for_fc);
        assert(estimator.update(observation(200, 0.20, 0.5)).reason == "horizontal_rate_exceeded");
    }

    {
        auto rate_config = config();
        rate_config.maximum_vertical_rate_mps = 0.5;
        vh::RouteLocalOdometryEstimator estimator(rate_config);
        initialize(estimator);
        assert(estimator.update(observation(100, 0.10, 0.5)).estimate.valid_for_fc);
        assert(estimator.update(observation(200, 0.11, 0.7)).reason == "vertical_rate_exceeded");
    }

    {
        auto rate_config = config();
        rate_config.maximum_yaw_rate_rad_s = 0.5;
        vh::RouteLocalOdometryEstimator estimator(rate_config);
        initialize(estimator);
        assert(estimator.update(observation(100, 0.10, 0.5, vh::RouteTravelDirection::forward, 0.0)).estimate.valid_for_fc);
        assert(estimator.update(observation(200, 0.11, 0.5, vh::RouteTravelDirection::forward, 0.2)).reason == "yaw_rate_exceeded");
    }

    {
        vh::RouteLocalOdometryEstimator estimator(config());
        initialize(estimator);
        auto stale_match = observation(200, 0.2, 0.5);
        stale_match.match.timestamp = at_ms(0);
        assert(estimator.update(stale_match).reason == "route_match_not_fresh");

        estimator.reset_tracking();
        auto future_altitude = observation(200, 0.2, 0.5);
        future_altitude.altitude_timestamp = at_ms(201);
        assert(estimator.update(future_altitude).reason == "altitude_from_future");

        estimator.reset_tracking();
        auto saturated = observation(200, 0.2, 0.5);
        saturated.match.direction_error_rad = 0.6;
        assert(estimator.update(saturated).reason == "direction_error_out_of_range");
    }

    {
        bool rejected = false;
        try {
            auto invalid = config();
            invalid.nominal_route_length_m = 0.0;
            vh::RouteLocalOdometryEstimator estimator(invalid);
            (void)estimator;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);

        rejected = false;
        try {
            auto invalid = config();
            invalid.reverse_direction_error_sign = 0;
            vh::RouteLocalOdometryEstimator estimator(invalid);
            (void)estimator;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);

        rejected = false;
        try {
            auto invalid = config();
            invalid.maximum_direction_error_rad = std::numeric_limits<double>::quiet_NaN();
            vh::RouteLocalOdometryEstimator estimator(invalid);
            (void)estimator;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    return 0;
}
