#include "visual_homing/inflight_home_reset_safety_gate.hpp"

#include <cmath>
#include <stdexcept>

namespace vh {
namespace {

bool finite(double value) {
    return std::isfinite(value);
}

InflightHomeResetSafetyResult blocked(const std::string& reason) {
    return InflightHomeResetSafetyResult{false, reason};
}

} // namespace

InflightHomeResetSafetyGate::InflightHomeResetSafetyGate(
    InflightHomeResetSafetyConfig config)
    : config_(config) {
    if (!finite(config_.max_telemetry_age_ms) || config_.max_telemetry_age_ms < 0.0) {
        throw std::invalid_argument("max_telemetry_age_ms must be finite and non-negative");
    }
    if (!finite(config_.max_rc_input_age_ms) || config_.max_rc_input_age_ms < 0.0) {
        throw std::invalid_argument("max_rc_input_age_ms must be finite and non-negative");
    }
}

InflightHomeResetSafetyResult InflightHomeResetSafetyGate::evaluate(
    const InflightHomeResetSafetySnapshot& snapshot) const {
    if (!snapshot.rc_trigger_edge) {
        return blocked("rc_trigger_not_edge");
    }
    if (!snapshot.telemetry.heartbeat_seen) {
        return blocked("telemetry_heartbeat_missing");
    }
    const auto telemetry_age_ms = milliseconds_between(snapshot.telemetry.timestamp, snapshot.now);
    if (!finite(telemetry_age_ms) || telemetry_age_ms < 0.0
        || telemetry_age_ms > config_.max_telemetry_age_ms) {
        return blocked("telemetry_stale");
    }
    const auto rc_input_age_ms = milliseconds_between(snapshot.rc_input_timestamp, snapshot.now);
    if (!finite(rc_input_age_ms) || rc_input_age_ms < 0.0
        || rc_input_age_ms > config_.max_rc_input_age_ms) {
        return blocked("rc_input_stale");
    }
    if (!config_.audit_log_ready) {
        return blocked("audit_log_not_ready");
    }

    if (snapshot.telemetry.armed) {
        if (!config_.override_available) {
            return blocked("inflight_override_unavailable");
        }
        if (snapshot.action == InflightHomeResetAction::LocalEstimatorReset) {
            if (!config_.allow_inflight_local_reset) {
                return blocked("inflight_local_reset_disabled");
            }
            if (!config_.local_reset_operator_confirmed) {
                return blocked("inflight_local_reset_operator_confirmation_missing");
            }
        } else {
            if (!config_.allow_inflight_fc_home_change) {
                return blocked("inflight_fc_home_change_disabled");
            }
            if (!config_.fc_home_change_operator_confirmed) {
                return blocked("inflight_fc_home_change_operator_confirmation_missing");
            }
        }
    }

    if (snapshot.action == InflightHomeResetAction::LocalEstimatorReset) {
        if (!snapshot.reset_reference_valid) {
            return blocked("reset_reference_invalid");
        }
    } else {
        if (!snapshot.external_nav_position_valid) {
            return blocked("external_nav_position_invalid");
        }
        if (!snapshot.home_target_valid) {
            return blocked("home_target_invalid");
        }
    }

    return InflightHomeResetSafetyResult{
        true,
        snapshot.telemetry.armed ? "inflight_override_allowed" : "disarmed_action_allowed",
    };
}

} // namespace vh
