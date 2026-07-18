#pragma once

#include <string>

#include "visual_homing/mavlink.hpp"
#include "visual_homing/time.hpp"

namespace vh {

enum class InflightHomeResetAction {
    LocalEstimatorReset,
    FlightControllerHomeChange,
};

struct InflightHomeResetSafetyConfig {
    bool override_available = false;
    bool allow_inflight_local_reset = false;
    bool allow_inflight_fc_home_change = false;
    bool local_reset_operator_confirmed = false;
    bool fc_home_change_operator_confirmed = false;
    bool audit_log_ready = false;
    double max_telemetry_age_ms = 500.0;
    double max_rc_input_age_ms = 250.0;
};

struct InflightHomeResetSafetySnapshot {
    Timestamp now{};
    MavlinkTelemetry telemetry{};
    Timestamp rc_input_timestamp{};
    InflightHomeResetAction action = InflightHomeResetAction::LocalEstimatorReset;
    bool rc_trigger_edge = false;
    bool reset_reference_valid = false;
    bool external_nav_position_valid = false;
    bool home_target_valid = false;
};

struct InflightHomeResetSafetyResult {
    bool allowed = false;
    std::string reason;
};

class InflightHomeResetSafetyGate {
public:
    explicit InflightHomeResetSafetyGate(InflightHomeResetSafetyConfig config);

    InflightHomeResetSafetyResult evaluate(
        const InflightHomeResetSafetySnapshot& snapshot) const;

private:
    InflightHomeResetSafetyConfig config_;
};

} // namespace vh
