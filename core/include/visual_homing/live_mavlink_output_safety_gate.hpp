#pragma once

#include <string>

#include "visual_homing/mavlink.hpp"
#include "visual_homing/navigation.hpp"
#include "visual_homing/route.hpp"
#include "visual_homing/time.hpp"

namespace vh {

struct LiveMavlinkOutputSafetyConfig {
    bool runtime_enabled = false;
    bool operator_confirmed = false;
    bool dry_run_quality_passed = false;
    bool audit_log_enabled = false;
    bool single_writer = true;
    double max_telemetry_age_ms = 500.0;
    double min_match_confidence = 0.75;
    double max_match_age_ms = 250.0;
    double max_abs_yaw_rate_radps = 0.35;
    double max_abs_forward_speed_mps = 0.5;
};

struct LiveMavlinkOutputSafetySnapshot {
    Timestamp now{};
    MavlinkTelemetry telemetry{};
    RouteMatch match{};
    NavigationCommand command{};
};

struct LiveMavlinkOutputSafetyResult {
    bool allowed = false;
    std::string reason;
};

class LiveMavlinkOutputSafetyGate {
public:
    explicit LiveMavlinkOutputSafetyGate(LiveMavlinkOutputSafetyConfig config);

    LiveMavlinkOutputSafetyResult evaluate(const LiveMavlinkOutputSafetySnapshot& snapshot) const;

private:
    LiveMavlinkOutputSafetyConfig config_;
};

} // namespace vh
