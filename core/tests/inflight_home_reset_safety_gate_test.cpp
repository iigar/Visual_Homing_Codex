#include <cassert>
#include <limits>
#include <stdexcept>

#include "visual_homing/inflight_home_reset_safety_gate.hpp"

namespace {

vh::InflightHomeResetSafetyConfig passing_config() {
    vh::InflightHomeResetSafetyConfig config;
    config.override_available = true;
    config.allow_inflight_local_reset = true;
    config.allow_inflight_fc_home_change = true;
    config.local_reset_operator_confirmed = true;
    config.fc_home_change_operator_confirmed = true;
    config.audit_log_ready = true;
    return config;
}

vh::InflightHomeResetSafetySnapshot passing_snapshot(
    vh::InflightHomeResetAction action,
    bool armed = true) {
    const auto now = vh::now();
    vh::InflightHomeResetSafetySnapshot snapshot;
    snapshot.now = now;
    snapshot.telemetry.timestamp = now - std::chrono::milliseconds(50);
    snapshot.telemetry.heartbeat_seen = true;
    snapshot.telemetry.armed = armed;
    snapshot.rc_input_timestamp = now - std::chrono::milliseconds(20);
    snapshot.action = action;
    snapshot.rc_trigger_edge = true;
    snapshot.reset_reference_valid = true;
    snapshot.external_nav_position_valid = true;
    snapshot.home_target_valid = true;
    return snapshot;
}

void expect_blocked(const vh::InflightHomeResetSafetyConfig& config,
                    const vh::InflightHomeResetSafetySnapshot& snapshot,
                    const std::string& reason) {
    const vh::InflightHomeResetSafetyGate gate(config);
    const auto result = gate.evaluate(snapshot);
    assert(!result.allowed);
    assert(result.reason == reason);
}

} // namespace

int main() {
    const auto local_reset = vh::InflightHomeResetAction::LocalEstimatorReset;
    const auto home_change = vh::InflightHomeResetAction::FlightControllerHomeChange;

    {
        auto config = passing_config();
        const vh::InflightHomeResetSafetyGate gate(config);
        const auto result = gate.evaluate(passing_snapshot(local_reset));
        assert(result.allowed);
        assert(result.reason == "inflight_override_allowed");
    }
    {
        auto config = passing_config();
        const vh::InflightHomeResetSafetyGate gate(config);
        const auto result = gate.evaluate(passing_snapshot(home_change));
        assert(result.allowed);
        assert(result.reason == "inflight_override_allowed");
    }
    {
        vh::InflightHomeResetSafetyConfig config;
        config.audit_log_ready = true;
        const vh::InflightHomeResetSafetyGate gate(config);
        const auto result = gate.evaluate(passing_snapshot(local_reset, false));
        assert(result.allowed);
        assert(result.reason == "disarmed_action_allowed");
    }
    {
        auto config = passing_config();
        config.override_available = false;
        expect_blocked(config, passing_snapshot(local_reset), "inflight_override_unavailable");
    }
    {
        auto config = passing_config();
        config.allow_inflight_local_reset = false;
        expect_blocked(config, passing_snapshot(local_reset), "inflight_local_reset_disabled");
    }
    {
        auto config = passing_config();
        config.local_reset_operator_confirmed = false;
        expect_blocked(
            config,
            passing_snapshot(local_reset),
            "inflight_local_reset_operator_confirmation_missing");
    }
    {
        auto config = passing_config();
        config.allow_inflight_fc_home_change = false;
        expect_blocked(config, passing_snapshot(home_change), "inflight_fc_home_change_disabled");
    }
    {
        auto config = passing_config();
        config.fc_home_change_operator_confirmed = false;
        expect_blocked(
            config,
            passing_snapshot(home_change),
            "inflight_fc_home_change_operator_confirmation_missing");
    }
    {
        auto config = passing_config();
        config.allow_inflight_local_reset = true;
        config.allow_inflight_fc_home_change = false;
        expect_blocked(config, passing_snapshot(home_change), "inflight_fc_home_change_disabled");
    }
    {
        auto config = passing_config();
        config.allow_inflight_local_reset = false;
        config.allow_inflight_fc_home_change = true;
        expect_blocked(config, passing_snapshot(local_reset), "inflight_local_reset_disabled");
    }
    {
        auto snapshot = passing_snapshot(local_reset);
        snapshot.rc_trigger_edge = false;
        expect_blocked(passing_config(), snapshot, "rc_trigger_not_edge");
    }
    {
        auto snapshot = passing_snapshot(local_reset);
        snapshot.telemetry.heartbeat_seen = false;
        expect_blocked(passing_config(), snapshot, "telemetry_heartbeat_missing");
    }
    {
        auto snapshot = passing_snapshot(local_reset);
        snapshot.telemetry.timestamp = snapshot.now - std::chrono::milliseconds(501);
        expect_blocked(passing_config(), snapshot, "telemetry_stale");
    }
    {
        auto snapshot = passing_snapshot(local_reset);
        snapshot.rc_input_timestamp = snapshot.now - std::chrono::milliseconds(251);
        expect_blocked(passing_config(), snapshot, "rc_input_stale");
    }
    {
        auto config = passing_config();
        config.audit_log_ready = false;
        expect_blocked(config, passing_snapshot(local_reset), "audit_log_not_ready");
    }
    {
        auto snapshot = passing_snapshot(local_reset);
        snapshot.reset_reference_valid = false;
        expect_blocked(passing_config(), snapshot, "reset_reference_invalid");
    }
    {
        auto snapshot = passing_snapshot(home_change);
        snapshot.external_nav_position_valid = false;
        expect_blocked(passing_config(), snapshot, "external_nav_position_invalid");
    }
    {
        auto snapshot = passing_snapshot(home_change);
        snapshot.home_target_valid = false;
        expect_blocked(passing_config(), snapshot, "home_target_invalid");
    }
    {
        bool rejected = false;
        try {
            auto config = passing_config();
            config.max_rc_input_age_ms = std::numeric_limits<double>::quiet_NaN();
            const vh::InflightHomeResetSafetyGate gate(config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    return 0;
}
