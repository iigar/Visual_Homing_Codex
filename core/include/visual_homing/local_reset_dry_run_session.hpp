#pragma once

#include <string>

#include "visual_homing/inflight_home_reset_safety_gate.hpp"
#include "visual_homing/rc_switch_trigger_decoder.hpp"

namespace vh {

enum class LocalResetDryRunDecision {
    ObserveOnly,
    Blocked,
    WouldResetLocalEstimator,
};

const char* local_reset_dry_run_decision_name(LocalResetDryRunDecision decision);

struct LocalResetDryRunSessionConfig {
    RcSwitchTriggerConfig trigger{};
    InflightHomeResetSafetyConfig safety{};
};

struct LocalResetDryRunInput {
    RcSwitchObservation rc{};
    Timestamp now{};
    MavlinkTelemetry telemetry{};
    bool reset_reference_valid = false;
};

struct LocalResetDryRunResult {
    RcSwitchTriggerResult trigger{};
    bool safety_evaluated = false;
    InflightHomeResetSafetyResult safety{};
    LocalResetDryRunDecision decision = LocalResetDryRunDecision::ObserveOnly;
    std::string reason;
};

class LocalResetDryRunSession {
public:
    explicit LocalResetDryRunSession(LocalResetDryRunSessionConfig config = {});

    LocalResetDryRunResult observe(const LocalResetDryRunInput& input);

private:
    RcSwitchTriggerDecoder trigger_decoder_;
    InflightHomeResetSafetyGate safety_gate_;
};

} // namespace vh
