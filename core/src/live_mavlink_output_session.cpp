#include "visual_homing/live_mavlink_output_session.hpp"

#include <stdexcept>
#include <utility>

namespace vh {

LiveMavlinkOutputSession::LiveMavlinkOutputSession(
    LiveMavlinkOutputSessionConfig config,
    LiveMavlinkOutputAuditSink& audit,
    MavlinkBridge& bridge)
    : config_(std::move(config)),
      audit_(audit),
      bridge_(bridge) {}

bool LiveMavlinkOutputSession::start() {
    running_ = false;
    if (!audit_.start(config_.run_id) || !audit_.ready()) {
        return false;
    }
    if (!bridge_.start()) {
        audit_.stop("bridge_start_failed");
        return false;
    }
    running_ = true;
    return true;
}

void LiveMavlinkOutputSession::stop(const std::string& reason) {
    if (running_) {
        bridge_.stop();
        running_ = false;
    }
    audit_.stop(reason);
}

LiveMavlinkOutputSessionResult LiveMavlinkOutputSession::process(
    const LiveMavlinkOutputSafetySnapshot& snapshot) {
    if (!running_) {
        throw std::runtime_error("LiveMavlinkOutputSession process called while stopped");
    }

    auto safety_config = config_.safety_config;
    safety_config.audit_log_ready = audit_.ready();
    const LiveMavlinkOutputSafetyGate gate(safety_config);
    const auto safety = gate.evaluate(snapshot);
    audit_.record_command(snapshot.command, safety);

    if (!safety.allowed) {
        return LiveMavlinkOutputSessionResult{false, safety};
    }

    bridge_.send(snapshot.command);
    return LiveMavlinkOutputSessionResult{true, safety};
}

bool LiveMavlinkOutputSession::running() const {
    return running_;
}

} // namespace vh
