#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>

#include "visual_homing/local_reset_dry_run_session.hpp"

namespace {

vh::Timestamp at_milliseconds(std::int64_t milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(
        std::chrono::milliseconds(milliseconds)));
}

vh::LocalResetDryRunSessionConfig base_config() {
    vh::LocalResetDryRunSessionConfig config;
    config.trigger.debounce_ms = 0.0;
    config.trigger.cooldown_ms = 0.0;
    config.safety.audit_log_ready = true;
    return config;
}

vh::LocalResetDryRunInput input_at(std::int64_t rc_ms,
                                   std::uint16_t pwm,
                                   std::int64_t now_ms) {
    vh::LocalResetDryRunInput input;
    input.rc.timestamp = at_milliseconds(rc_ms);
    input.rc.pwm = pwm;
    input.now = at_milliseconds(now_ms);
    input.telemetry.timestamp = at_milliseconds(now_ms - 10);
    input.telemetry.heartbeat_seen = true;
    input.telemetry.armed = false;
    input.reset_reference_valid = true;
    return input;
}

vh::LocalResetDryRunResult trigger(vh::LocalResetDryRunSession& session,
                                   vh::LocalResetDryRunInput high) {
    auto low = high;
    low.rc.timestamp = high.rc.timestamp - std::chrono::milliseconds(1);
    low.rc.pwm = 999;
    const auto low_result = session.observe(low);
    assert(low_result.decision == vh::LocalResetDryRunDecision::ObserveOnly);
    assert(!low_result.safety_evaluated);
    assert(low_result.trigger.low_armed);
    return session.observe(high);
}

void expect_safety_block(const vh::LocalResetDryRunResult& result,
                         const std::string& reason) {
    assert(result.trigger.trigger_edge);
    assert(result.safety_evaluated);
    assert(!result.safety.allowed);
    assert(result.decision == vh::LocalResetDryRunDecision::Blocked);
    assert(result.reason == reason);
}

} // namespace

int main() {
    {
        vh::LocalResetDryRunSession session(base_config());
        const auto low = session.observe(input_at(1000, 999, 1010));
        assert(low.decision == vh::LocalResetDryRunDecision::ObserveOnly);
        assert(!low.safety_evaluated);
        assert(low.reason == "low_position_armed");

        const auto allowed = session.observe(input_at(1001, 2000, 1011));
        assert(allowed.trigger.trigger_edge);
        assert(allowed.safety_evaluated);
        assert(allowed.safety.allowed);
        assert(allowed.decision
               == vh::LocalResetDryRunDecision::WouldResetLocalEstimator);
        assert(allowed.reason == "disarmed_action_allowed");

        const auto held = session.observe(input_at(1200, 2000, 1210));
        assert(held.decision == vh::LocalResetDryRunDecision::ObserveOnly);
        assert(!held.safety_evaluated);
        assert(held.reason == "steady_high");
    }
    {
        vh::LocalResetDryRunSession session(base_config());
        auto input = input_at(1001, 2000, 1011);
        input.telemetry.heartbeat_seen = false;
        expect_safety_block(trigger(session, input), "telemetry_heartbeat_missing");
    }
    {
        vh::LocalResetDryRunSession session(base_config());
        auto input = input_at(1001, 2000, 2000);
        input.telemetry.timestamp = at_milliseconds(1499);
        expect_safety_block(trigger(session, input), "telemetry_stale");
    }
    {
        vh::LocalResetDryRunSession session(base_config());
        auto input = input_at(1001, 2000, 1252);
        input.telemetry.timestamp = at_milliseconds(1250);
        expect_safety_block(trigger(session, input), "rc_input_stale");
    }
    {
        auto config = base_config();
        config.safety.audit_log_ready = false;
        vh::LocalResetDryRunSession session(config);
        expect_safety_block(
            trigger(session, input_at(1001, 2000, 1011)),
            "audit_log_not_ready");
    }
    {
        vh::LocalResetDryRunSession session(base_config());
        auto input = input_at(1001, 2000, 1011);
        input.reset_reference_valid = false;
        expect_safety_block(trigger(session, input), "reset_reference_invalid");
    }
    {
        vh::LocalResetDryRunSession session(base_config());
        auto input = input_at(1001, 2000, 1011);
        input.telemetry.armed = true;
        expect_safety_block(trigger(session, input), "inflight_override_unavailable");
    }
    {
        auto config = base_config();
        config.safety.override_available = true;
        config.safety.allow_inflight_local_reset = true;
        config.safety.local_reset_operator_confirmed = true;
        vh::LocalResetDryRunSession session(config);
        auto input = input_at(1001, 2000, 1011);
        input.telemetry.armed = true;
        const auto allowed = trigger(session, input);
        assert(allowed.safety.allowed);
        assert(allowed.decision
               == vh::LocalResetDryRunDecision::WouldResetLocalEstimator);
        assert(allowed.reason == "inflight_override_allowed");
    }
    {
        auto config = base_config();
        config.safety.override_available = true;
        config.safety.allow_inflight_fc_home_change = true;
        config.safety.fc_home_change_operator_confirmed = true;
        vh::LocalResetDryRunSession session(config);
        auto input = input_at(1001, 2000, 1011);
        input.telemetry.armed = true;
        expect_safety_block(trigger(session, input), "inflight_local_reset_disabled");
    }
    {
        vh::LocalResetDryRunSession session(base_config());
        const auto invalid = session.observe(input_at(1000, 700, 1010));
        assert(!invalid.trigger.sample_accepted);
        assert(!invalid.safety_evaluated);
        assert(invalid.decision == vh::LocalResetDryRunDecision::Blocked);
        assert(invalid.reason == "pwm_out_of_range");
    }
    {
        vh::LocalResetDryRunSession session(base_config());
        const auto low = session.observe(input_at(1000, 999, 1010));
        assert(low.trigger.low_armed);
        const auto rollback = session.observe(input_at(999, 999, 1010));
        assert(!rollback.trigger.sample_accepted);
        assert(!rollback.safety_evaluated);
        assert(rollback.decision == vh::LocalResetDryRunDecision::Blocked);
        assert(rollback.reason == "timestamp_moved_backwards");
    }

    assert(std::string(vh::local_reset_dry_run_decision_name(
               vh::LocalResetDryRunDecision::ObserveOnly))
           == "observe_only");
    assert(std::string(vh::local_reset_dry_run_decision_name(
               vh::LocalResetDryRunDecision::Blocked))
           == "blocked");
    assert(std::string(vh::local_reset_dry_run_decision_name(
               vh::LocalResetDryRunDecision::WouldResetLocalEstimator))
           == "would_reset_local_estimator");
    return 0;
}
