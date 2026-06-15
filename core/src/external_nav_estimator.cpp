#include "visual_homing/external_nav_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace vh {
namespace {

bool finite_non_negative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool finite_positive(double value) {
    return std::isfinite(value) && value > 0.0;
}

std::string bool_text(bool value) {
    return value ? "true" : "false";
}

} // namespace

ExternalNavEstimate make_route_progress_external_nav_estimate(
    const RouteMatch& match,
    const RouteSignatureSummary& route,
    const MavlinkTelemetry& telemetry,
    Timestamp current_time,
    const ExternalNavEstimatorConfig& config) {
    if (!finite_non_negative(config.minimum_match_confidence) || config.minimum_match_confidence > 1.0) {
        throw std::invalid_argument("ExternalNavEstimatorConfig minimum_match_confidence must be in [0, 1]");
    }
    if (!finite_non_negative(config.maximum_altitude_age_ms)) {
        throw std::invalid_argument("ExternalNavEstimatorConfig maximum_altitude_age_ms must be finite and non-negative");
    }
    if (!std::isfinite(config.nominal_route_length_m) || config.nominal_route_length_m < 0.0) {
        throw std::invalid_argument("ExternalNavEstimatorConfig nominal_route_length_m must be finite and non-negative");
    }

    ExternalNavEstimate estimate;
    estimate.timestamp = current_time;
    estimate.confidence = match.confidence;
    estimate.route_progress = std::clamp(match.progress, 0.0, 1.0);
    estimate.route_index = match.route_index;
    estimate.route_entries = route.entry_count;
    estimate.route_match_valid = match.valid && match.confidence >= config.minimum_match_confidence;
    estimate.source_tag = config.source_tag;

    estimate.telemetry_fresh = telemetry.heartbeat_seen
        && milliseconds_between(telemetry.timestamp, current_time) <= config.maximum_altitude_age_ms;
    estimate.altitude_valid = estimate.telemetry_fresh
        && telemetry.relative_altitude_seen
        && finite_positive(telemetry.relative_altitude_m);
    estimate.scale_known = finite_positive(config.nominal_route_length_m) && estimate.altitude_valid;

    estimate.x_m = estimate.scale_known
        ? estimate.route_progress * config.nominal_route_length_m
        : 0.0;
    estimate.y_m = 0.0;
    estimate.z_m = estimate.altitude_valid ? -telemetry.relative_altitude_m : 0.0;
    estimate.yaw_rad = std::isfinite(telemetry.yaw_rad) ? telemetry.yaw_rad : 0.0;

    if (route.entry_count == 0) {
        estimate.reason = "route_empty";
    } else if (!estimate.route_match_valid) {
        estimate.reason = "route_match_not_valid";
    } else if (!estimate.telemetry_fresh) {
        estimate.reason = "telemetry_not_fresh";
    } else if (!estimate.altitude_valid) {
        estimate.reason = "altitude_not_valid";
    } else if (!estimate.scale_known) {
        estimate.reason = "scale_not_known";
    } else {
        estimate.valid_for_fc = true;
        estimate.reason = "valid";
    }

    return estimate;
}

std::string external_nav_estimate_log_line(const ExternalNavEstimate& estimate) {
    std::ostringstream output;
    output << "external_nav_estimate"
           << " valid_for_fc=" << bool_text(estimate.valid_for_fc)
           << " reason=" << estimate.reason
           << " source=" << estimate.source_tag
           << " x_m=" << estimate.x_m
           << " y_m=" << estimate.y_m
           << " z_m=" << estimate.z_m
           << " yaw_rad=" << estimate.yaw_rad
           << " confidence=" << estimate.confidence
           << " progress=" << estimate.route_progress
           << " route_index=" << estimate.route_index
           << " route_entries=" << estimate.route_entries
           << " route_match_valid=" << bool_text(estimate.route_match_valid)
           << " telemetry_fresh=" << bool_text(estimate.telemetry_fresh)
           << " altitude_valid=" << bool_text(estimate.altitude_valid)
           << " scale_known=" << bool_text(estimate.scale_known);
    return output.str();
}

} // namespace vh
