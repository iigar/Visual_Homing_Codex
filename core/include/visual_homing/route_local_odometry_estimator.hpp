#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "visual_homing/route.hpp"
#include "visual_homing/route_local_odometry.hpp"
#include "visual_homing/time.hpp"

namespace vh {

enum class RouteTravelDirection {
    forward,
    reverse,
};

// Reverse yaw is deliberately unavailable by default. Enabling
// nose_toward_route_start states that vehicle Forward points toward the route
// start and therefore has a route-local yaw of pi plus the calibrated image
// direction residual.
enum class ReverseYawPolicy {
    unavailable,
    nose_toward_route_start,
};

struct RouteLocalOdometryEstimatorConfig {
    double nominal_route_length_m = 0.0;
    double minimum_match_confidence = 0.9;
    double maximum_match_age_ms = 250.0;
    double maximum_altitude_age_ms = 500.0;
    double minimum_update_interval_ms = 10.0;
    double maximum_update_interval_ms = 500.0;
    double maximum_horizontal_rate_mps = 5.0;
    double maximum_vertical_rate_mps = 2.0;
    double maximum_yaw_rate_rad_s = 2.0;
    double maximum_direction_error_rad = 0.5;
    double maximum_progress_direction_error = 0.02;
    std::size_t maximum_consecutive_invalid_updates = 3;
    ReverseYawPolicy reverse_yaw_policy = ReverseYawPolicy::unavailable;
    std::int8_t reverse_direction_error_sign = 1;
};

struct RouteLocalOdometryObservation {
    Timestamp timestamp{};
    RouteMatch match{};
    Timestamp altitude_timestamp{};
    double altitude_m = 0.0;
    bool altitude_valid = false;
    bool health_ready = false;
    RouteTravelDirection direction = RouteTravelDirection::forward;
};

struct RouteLocalOdometryEstimatorResult {
    RouteLocalOdometryEstimate estimate{};
    RouteTravelDirection direction = RouteTravelDirection::forward;
    double route_progress = 0.0;
    double start_altitude_m = 0.0;
    double current_altitude_m = 0.0;
    double match_age_ms = 0.0;
    double altitude_age_ms = 0.0;
    double update_interval_ms = 0.0;
    double horizontal_rate_mps = 0.0;
    double vertical_rate_mps = 0.0;
    double yaw_rate_rad_s = 0.0;
    std::size_t consecutive_invalid_updates = 0;
    bool start_altitude_initialized = false;
    bool reset_required = false;
    std::string reason = "not_evaluated";
};

class RouteLocalOdometryEstimator {
public:
    explicit RouteLocalOdometryEstimator(RouteLocalOdometryEstimatorConfig config);

    // Establishes the route-start vertical origin from a finite, fresh
    // observation. It may be called only once until clear_start_altitude().
    void initialize_start_altitude(
        double altitude_m,
        Timestamp observation_timestamp,
        Timestamp current_time);

    // Starts a new estimator continuity sequence while preserving the fixed
    // route-start altitude. The MAVLink reset counter increments modulo 256.
    void reset_tracking();

    // Explicitly discards the vertical origin and starts a new continuity
    // sequence. Updates fail closed until the origin is initialized again.
    void clear_start_altitude();

    RouteLocalOdometryEstimatorResult update(const RouteLocalOdometryObservation& observation);

    bool start_altitude_initialized() const;
    bool reset_required() const;
    std::uint8_t reset_counter() const;

private:
    struct AcceptedState {
        Timestamp timestamp{};
        RouteTravelDirection direction = RouteTravelDirection::forward;
        double progress = 0.0;
        double x_m = 0.0;
        double z_m = 0.0;
        double yaw_rad = 0.0;
    };

    RouteLocalOdometryEstimatorResult reject(
        RouteLocalOdometryEstimatorResult result,
        std::string reason,
        bool count_invalid_update);

    RouteLocalOdometryEstimatorConfig config_;
    bool start_altitude_initialized_ = false;
    double start_altitude_m_ = 0.0;
    std::uint8_t reset_counter_ = 0;
    std::size_t consecutive_invalid_updates_ = 0;
    bool reset_required_ = false;
    std::optional<AcceptedState> last_accepted_;
};

} // namespace vh
