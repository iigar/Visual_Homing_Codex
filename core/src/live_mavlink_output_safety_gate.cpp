#include "visual_homing/live_mavlink_output_safety_gate.hpp"

#include <cmath>
#include <stdexcept>

namespace vh {
namespace {

bool finite(double value) {
    return std::isfinite(value);
}

LiveMavlinkOutputSafetyResult blocked(const std::string& reason) {
    return LiveMavlinkOutputSafetyResult{false, reason};
}

} // namespace

LiveMavlinkOutputSafetyGate::LiveMavlinkOutputSafetyGate(LiveMavlinkOutputSafetyConfig config)
    : config_(config) {
    if (!finite(config_.max_telemetry_age_ms) || config_.max_telemetry_age_ms < 0.0) {
        throw std::invalid_argument("max_telemetry_age_ms must be finite and non-negative");
    }
    if (!finite(config_.min_match_confidence) || config_.min_match_confidence < 0.0
        || config_.min_match_confidence > 1.0) {
        throw std::invalid_argument("min_match_confidence must be finite and in [0, 1]");
    }
    if (!finite(config_.max_match_age_ms) || config_.max_match_age_ms < 0.0) {
        throw std::invalid_argument("max_match_age_ms must be finite and non-negative");
    }
    if (!finite(config_.max_abs_yaw_rate_radps) || config_.max_abs_yaw_rate_radps < 0.0) {
        throw std::invalid_argument("max_abs_yaw_rate_radps must be finite and non-negative");
    }
    if (!finite(config_.max_abs_forward_speed_mps) || config_.max_abs_forward_speed_mps < 0.0) {
        throw std::invalid_argument("max_abs_forward_speed_mps must be finite and non-negative");
    }
}

LiveMavlinkOutputSafetyResult LiveMavlinkOutputSafetyGate::evaluate(
    const LiveMavlinkOutputSafetySnapshot& snapshot) const {
    if (!config_.live_output_available) {
        return blocked("live_output_unavailable");
    }
    if (!config_.runtime_enabled) {
        return blocked("runtime_live_output_disabled");
    }
    if (!config_.operator_confirmed) {
        return blocked("operator_confirmation_missing");
    }
    if (!config_.single_writer) {
        return blocked("single_writer_gate_failed");
    }
    if (!config_.audit_log_enabled) {
        return blocked("audit_log_disabled");
    }
    if (!config_.audit_log_ready) {
        return blocked("audit_log_not_ready");
    }
    if (!config_.dry_run_quality_passed) {
        return blocked("dry_run_quality_not_validated");
    }
    if (!snapshot.telemetry.heartbeat_seen) {
        return blocked("telemetry_heartbeat_missing");
    }
    if (!snapshot.telemetry.armed) {
        return blocked("vehicle_not_armed");
    }
    if (snapshot.telemetry.mode != FlightMode::Guided) {
        return blocked("vehicle_not_guided");
    }
    const auto telemetry_age_ms = milliseconds_between(snapshot.telemetry.timestamp, snapshot.now);
    if (!finite(telemetry_age_ms) || telemetry_age_ms < 0.0 || telemetry_age_ms > config_.max_telemetry_age_ms) {
        return blocked("telemetry_stale");
    }
    if (!snapshot.match.valid) {
        return blocked("route_match_invalid");
    }
    if (!finite(snapshot.match.confidence) || snapshot.match.confidence < config_.min_match_confidence) {
        return blocked("route_match_confidence_low");
    }
    if (!finite(snapshot.match.progress) || snapshot.match.progress < 0.0 || snapshot.match.progress > 1.0) {
        return blocked("route_match_progress_invalid");
    }
    const auto match_age_ms = milliseconds_between(snapshot.match.timestamp, snapshot.now);
    if (!finite(match_age_ms) || match_age_ms < 0.0 || match_age_ms > config_.max_match_age_ms) {
        return blocked("route_match_stale");
    }
    if (!snapshot.command.valid) {
        return blocked("command_invalid");
    }
    if (!finite(snapshot.command.vx_mps) || !finite(snapshot.command.vy_mps)
        || !finite(snapshot.command.yaw_rate_radps) || !finite(snapshot.command.confidence)) {
        return blocked("command_non_finite");
    }
    if (std::abs(snapshot.command.yaw_rate_radps) > config_.max_abs_yaw_rate_radps) {
        return blocked("command_yaw_rate_out_of_bounds");
    }
    if (config_.require_zero_forward_speed && snapshot.command.vx_mps != 0.0) {
        return blocked("command_forward_speed_not_zero");
    }
    if (config_.require_zero_lateral_speed && snapshot.command.vy_mps != 0.0) {
        return blocked("command_lateral_speed_not_zero");
    }
    if (std::abs(snapshot.command.vx_mps) > config_.max_abs_forward_speed_mps) {
        return blocked("command_forward_speed_out_of_bounds");
    }
    return LiveMavlinkOutputSafetyResult{true, "allowed"};
}

} // namespace vh
