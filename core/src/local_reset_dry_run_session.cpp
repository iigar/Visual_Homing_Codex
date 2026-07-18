#include "visual_homing/local_reset_dry_run_session.hpp"

namespace vh {

const char* local_reset_dry_run_decision_name(LocalResetDryRunDecision decision) {
    switch (decision) {
    case LocalResetDryRunDecision::ObserveOnly:
        return "observe_only";
    case LocalResetDryRunDecision::Blocked:
        return "blocked";
    case LocalResetDryRunDecision::WouldResetLocalEstimator:
        return "would_reset_local_estimator";
    }
    return "unknown";
}

LocalResetDryRunSession::LocalResetDryRunSession(LocalResetDryRunSessionConfig config)
    : trigger_decoder_(config.trigger),
      safety_gate_(config.safety) {}

LocalResetDryRunResult LocalResetDryRunSession::observe(
    const LocalResetDryRunInput& input) {
    LocalResetDryRunResult result;
    result.trigger = trigger_decoder_.observe(input.rc);

    if (!result.trigger.sample_accepted) {
        result.decision = LocalResetDryRunDecision::Blocked;
        result.reason = result.trigger.reason;
        return result;
    }
    if (!result.trigger.trigger_edge) {
        result.reason = result.trigger.reason;
        return result;
    }

    InflightHomeResetSafetySnapshot snapshot;
    snapshot.now = input.now;
    snapshot.telemetry = input.telemetry;
    snapshot.rc_input_timestamp = input.rc.timestamp;
    snapshot.action = InflightHomeResetAction::LocalEstimatorReset;
    snapshot.rc_trigger_edge = true;
    snapshot.reset_reference_valid = input.reset_reference_valid;

    result.safety_evaluated = true;
    result.safety = safety_gate_.evaluate(snapshot);
    result.reason = result.safety.reason;
    result.decision = result.safety.allowed
        ? LocalResetDryRunDecision::WouldResetLocalEstimator
        : LocalResetDryRunDecision::Blocked;
    return result;
}

} // namespace vh
