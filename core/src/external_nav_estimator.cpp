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
    if (!std::isfinite(config.bench_diagnostic_altitude_m) || config.bench_diagnostic_altitude_m < 0.0) {
        throw std::invalid_argument(
            "ExternalNavEstimatorConfig bench_diagnostic_altitude_m must be finite and non-negative");
    }
    if (!std::isfinite(config.route_alignment.origin_ned_m.x)
        || !std::isfinite(config.route_alignment.origin_ned_m.y)
        || !std::isfinite(config.route_alignment.origin_ned_m.z)
        || !std::isfinite(config.route_alignment.heading_ned_rad)) {
        throw std::invalid_argument("ExternalNavEstimatorConfig route alignment must be finite");
    }

    ExternalNavEstimate estimate;
    estimate.timestamp = current_time;
    estimate.confidence = match.confidence;
    estimate.route_progress = std::clamp(match.progress, 0.0, 1.0);
    estimate.route_index = match.route_index;
    estimate.route_entries = route.entry_count;
    estimate.route_match_valid = match.valid && match.confidence >= config.minimum_match_confidence;
    estimate.source_tag = config.source_tag;
    estimate.relative_altitude_seen = telemetry.relative_altitude_seen;
    estimate.relative_altitude_m = telemetry.relative_altitude_m;
    estimate.frame_alignment_known = config.route_frame_alignment_known;
    estimate.route_origin_ned_m = config.route_alignment.origin_ned_m;
    estimate.route_heading_ned_rad = config.route_alignment.heading_ned_rad;
    estimate.altitude_origin_aligned = config.altitude_origin_aligned;

    estimate.telemetry_fresh = telemetry.heartbeat_seen
        && milliseconds_between(telemetry.timestamp, current_time) <= config.maximum_altitude_age_ms;
    estimate.altitude_valid = estimate.telemetry_fresh
        && telemetry.relative_altitude_seen
        && finite_positive(telemetry.relative_altitude_m);
    estimate.bench_diagnostic_altitude_used = estimate.telemetry_fresh
        && !estimate.altitude_valid
        && finite_positive(config.bench_diagnostic_altitude_m);
    const auto altitude_for_scale_m = estimate.altitude_valid
        ? telemetry.relative_altitude_m
        : (estimate.bench_diagnostic_altitude_used ? config.bench_diagnostic_altitude_m : 0.0);
    estimate.scale_known = finite_positive(config.nominal_route_length_m)
        && (estimate.altitude_valid || estimate.bench_diagnostic_altitude_used);

    const Vector3d route_position_m{
        estimate.scale_known ? estimate.route_progress * config.nominal_route_length_m : 0.0,
        0.0,
        estimate.scale_known ? -altitude_for_scale_m : 0.0,
    };
    auto output_position_m = route_position_m;
    if (estimate.frame_alignment_known) {
        output_position_m = route_frd_to_local_ned(route_position_m, config.route_alignment);
        estimate.pose_frame = LocalCoordinateFrame::local_ned;
    }
    estimate.x_m = output_position_m.x;
    estimate.y_m = output_position_m.y;
    estimate.z_m = output_position_m.z;
    estimate.yaw_rad = std::isfinite(telemetry.yaw_rad) ? telemetry.yaw_rad : 0.0;
    // FC telemetry yaw is useful for diagnostics, but it is not an independent
    // ExternalNav yaw observation and must never be fed back as yaw authority.
    estimate.yaw_source_independent = false;

    if (route.entry_count == 0) {
        estimate.reason = "route_empty";
    } else if (!estimate.route_match_valid) {
        estimate.reason = "route_match_not_valid";
    } else if (!estimate.telemetry_fresh) {
        estimate.reason = "telemetry_not_fresh";
    } else if (estimate.bench_diagnostic_altitude_used) {
        estimate.reason = "bench_diagnostic_altitude_not_fc_ready";
    } else if (!estimate.altitude_valid) {
        estimate.reason = "altitude_not_valid";
    } else if (!estimate.scale_known) {
        estimate.reason = "scale_not_known";
    } else if (!estimate.frame_alignment_known) {
        estimate.reason = "frame_alignment_not_known";
    } else if (!estimate.altitude_origin_aligned) {
        estimate.reason = "altitude_origin_not_aligned";
    } else if (!estimate.yaw_source_independent) {
        estimate.reason = "yaw_source_not_independent";
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
           << " yaw_source_independent=" << bool_text(estimate.yaw_source_independent)
           << " pose_frame=" << coordinate_frame_name(estimate.pose_frame)
           << " frame_alignment_known=" << bool_text(estimate.frame_alignment_known)
           << " route_origin_ned_m=" << estimate.route_origin_ned_m.x
           << "/" << estimate.route_origin_ned_m.y
           << "/" << estimate.route_origin_ned_m.z
           << " route_heading_ned_rad=" << estimate.route_heading_ned_rad
           << " altitude_origin_aligned=" << bool_text(estimate.altitude_origin_aligned)
           << " confidence=" << estimate.confidence
           << " progress=" << estimate.route_progress
           << " route_index=" << estimate.route_index
           << " route_entries=" << estimate.route_entries
           << " relative_altitude_seen=" << bool_text(estimate.relative_altitude_seen)
           << " relative_altitude_m=" << estimate.relative_altitude_m
           << " route_match_valid=" << bool_text(estimate.route_match_valid)
           << " telemetry_fresh=" << bool_text(estimate.telemetry_fresh)
           << " altitude_valid=" << bool_text(estimate.altitude_valid)
           << " bench_diagnostic_altitude_used=" << bool_text(estimate.bench_diagnostic_altitude_used)
           << " scale_known=" << bool_text(estimate.scale_known)
           << " visual_scale_valid=" << bool_text(estimate.visual_scale_valid)
           << " visual_scale_ratio=" << estimate.visual_scale_ratio
           << " visual_altitude_m=" << estimate.visual_altitude_m
           << " visual_scale_confidence=" << estimate.visual_scale_confidence;
    return output.str();
}

} // namespace vh
