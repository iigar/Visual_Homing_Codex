#pragma once

#include <string>

#include "visual_homing/interfaces.hpp"
#include "visual_homing/live_mavlink_output_audit_log.hpp"
#include "visual_homing/live_mavlink_output_safety_gate.hpp"

namespace vh {

struct LiveMavlinkOutputSessionConfig {
    std::string run_id;
    LiveMavlinkOutputSafetyConfig safety_config;
};

struct LiveMavlinkOutputSessionResult {
    bool sent = false;
    LiveMavlinkOutputSafetyResult safety;
};

class LiveMavlinkOutputSession {
public:
    LiveMavlinkOutputSession(
        LiveMavlinkOutputSessionConfig config,
        LiveMavlinkOutputAuditSink& audit,
        MavlinkBridge& bridge);

    bool start();
    void stop(const std::string& reason);
    LiveMavlinkOutputSessionResult process(const LiveMavlinkOutputSafetySnapshot& snapshot);

    bool running() const;

private:
    LiveMavlinkOutputSessionConfig config_;
    LiveMavlinkOutputAuditSink& audit_;
    MavlinkBridge& bridge_;
    bool running_ = false;
};

} // namespace vh
