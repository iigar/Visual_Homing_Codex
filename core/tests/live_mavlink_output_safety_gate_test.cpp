#include <cassert>
#include <limits>
#include <stdexcept>

#include "visual_homing/live_mavlink_output_safety_gate.hpp"

namespace {

vh::LiveMavlinkOutputSafetyConfig passing_config() {
    vh::LiveMavlinkOutputSafetyConfig config;
    config.runtime_enabled = true;
    config.operator_confirmed = true;
    config.dry_run_quality_passed = true;
    config.audit_log_enabled = true;
    config.single_writer = true;
    config.max_telemetry_age_ms = 500.0;
    config.min_match_confidence = 0.75;
    config.max_match_age_ms = 250.0;
    config.max_abs_yaw_rate_radps = 0.35;
    config.max_abs_forward_speed_mps = 0.5;
    config.require_zero_forward_speed = true;
    return config;
}

vh::LiveMavlinkOutputSafetySnapshot passing_snapshot() {
    const auto now = vh::now();
    vh::LiveMavlinkOutputSafetySnapshot snapshot;
    snapshot.now = now;
    snapshot.telemetry.timestamp = now - std::chrono::milliseconds(50);
    snapshot.telemetry.heartbeat_seen = true;
    snapshot.telemetry.armed = true;
    snapshot.telemetry.mode = vh::FlightMode::Guided;
    snapshot.match.timestamp = now - std::chrono::milliseconds(20);
    snapshot.match.valid = true;
    snapshot.match.progress = 0.5;
    snapshot.match.confidence = 0.9;
    snapshot.command.valid = true;
    snapshot.command.vx_mps = 0.0;
    snapshot.command.yaw_rate_radps = 0.1;
    snapshot.command.confidence = 0.9;
    return snapshot;
}

void expect_blocked(const vh::LiveMavlinkOutputSafetyConfig& config,
                    const vh::LiveMavlinkOutputSafetySnapshot& snapshot,
                    const std::string& reason) {
    const vh::LiveMavlinkOutputSafetyGate gate(config);
    const auto result = gate.evaluate(snapshot);
    assert(!result.allowed);
    assert(result.reason == reason);
}

} // namespace

int main() {
    {
        const vh::LiveMavlinkOutputSafetyGate gate(passing_config());
        const auto result = gate.evaluate(passing_snapshot());
        assert(result.allowed);
        assert(result.reason == "allowed");
    }

    {
        auto config = passing_config();
        config.runtime_enabled = false;
        expect_blocked(config, passing_snapshot(), "runtime_live_output_disabled");
    }
    {
        auto config = passing_config();
        config.operator_confirmed = false;
        expect_blocked(config, passing_snapshot(), "operator_confirmation_missing");
    }
    {
        auto config = passing_config();
        config.single_writer = false;
        expect_blocked(config, passing_snapshot(), "single_writer_gate_failed");
    }
    {
        auto config = passing_config();
        config.audit_log_enabled = false;
        expect_blocked(config, passing_snapshot(), "audit_log_disabled");
    }
    {
        auto config = passing_config();
        config.dry_run_quality_passed = false;
        expect_blocked(config, passing_snapshot(), "dry_run_quality_not_validated");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.telemetry.heartbeat_seen = false;
        expect_blocked(passing_config(), snapshot, "telemetry_heartbeat_missing");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.telemetry.armed = false;
        expect_blocked(passing_config(), snapshot, "vehicle_not_armed");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.telemetry.mode = vh::FlightMode::AltHold;
        expect_blocked(passing_config(), snapshot, "vehicle_not_guided");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.telemetry.timestamp = snapshot.now - std::chrono::milliseconds(600);
        expect_blocked(passing_config(), snapshot, "telemetry_stale");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.match.valid = false;
        expect_blocked(passing_config(), snapshot, "route_match_invalid");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.match.confidence = 0.5;
        expect_blocked(passing_config(), snapshot, "route_match_confidence_low");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.match.progress = 1.5;
        expect_blocked(passing_config(), snapshot, "route_match_progress_invalid");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.match.timestamp = snapshot.now - std::chrono::milliseconds(300);
        expect_blocked(passing_config(), snapshot, "route_match_stale");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.command.valid = false;
        expect_blocked(passing_config(), snapshot, "command_invalid");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.command.yaw_rate_radps = std::numeric_limits<double>::quiet_NaN();
        expect_blocked(passing_config(), snapshot, "command_non_finite");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.command.yaw_rate_radps = 0.5;
        expect_blocked(passing_config(), snapshot, "command_yaw_rate_out_of_bounds");
    }
    {
        auto snapshot = passing_snapshot();
        snapshot.command.vx_mps = 0.2;
        expect_blocked(passing_config(), snapshot, "command_forward_speed_not_zero");
    }
    {
        auto config = passing_config();
        config.require_zero_forward_speed = false;
        auto snapshot = passing_snapshot();
        snapshot.command.vx_mps = 0.2;
        const vh::LiveMavlinkOutputSafetyGate gate(config);
        const auto result = gate.evaluate(snapshot);
        assert(result.allowed);
        assert(result.reason == "allowed");
    }
    {
        auto config = passing_config();
        config.require_zero_forward_speed = false;
        auto snapshot = passing_snapshot();
        snapshot.command.vx_mps = 0.7;
        expect_blocked(config, snapshot, "command_forward_speed_out_of_bounds");
    }
    {
        bool rejected = false;
        try {
            auto config = passing_config();
            config.max_telemetry_age_ms = -1.0;
            const vh::LiveMavlinkOutputSafetyGate gate(config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    return 0;
}
