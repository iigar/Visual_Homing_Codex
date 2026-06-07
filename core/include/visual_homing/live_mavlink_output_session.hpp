#pragma once

#include <cstdint>
#include <string>

#include "visual_homing/interfaces.hpp"
#include "visual_homing/live_mavlink_output_audit_log.hpp"
#include "visual_homing/live_mavlink_output_safety_gate.hpp"

namespace vh {

struct LiveMavlinkOutputSessionConfig {
    std::string run_id;
    LiveMavlinkOutputSafetyConfig safety_config;
    std::uint64_t max_commands = 0;
    double max_duration_ms = 0.0;
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
    Timestamp started_at_{};
    std::uint64_t commands_sent_ = 0;
    bool bridge_started_ = false;
    bool running_ = false;
};

} // namespace vh
