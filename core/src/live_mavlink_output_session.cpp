#include "visual_homing/live_mavlink_output_session.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace vh {

LiveMavlinkOutputSession::LiveMavlinkOutputSession(
    LiveMavlinkOutputSessionConfig config,
    LiveMavlinkOutputAuditSink& audit,
    MavlinkBridge& bridge)
    : config_(std::move(config)),
      audit_(audit),
      bridge_(bridge) {
    if (!std::isfinite(config_.max_duration_ms) || config_.max_duration_ms < 0.0) {
        throw std::invalid_argument("LiveMavlinkOutputSession max_duration_ms must be finite and non-negative");
    }
}

bool LiveMavlinkOutputSession::start() {
    running_ = false;
    if (!audit_.start(config_.run_id) || !audit_.ready()) {
        return false;
    }
    if (!bridge_.start()) {
        audit_.stop("bridge_start_failed");
        return false;
    }
    started_at_ = now();
    commands_sent_ = 0;
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

    if (config_.max_commands > 0 && commands_sent_ >= config_.max_commands) {
        const LiveMavlinkOutputSafetyResult safety{false, "max_command_count_reached"};
        audit_.record_command(snapshot.command, safety);
        stop("max_command_count_reached");
        return LiveMavlinkOutputSessionResult{false, safety};
    }

    if (config_.max_duration_ms > 0.0) {
        const auto elapsed_ms = milliseconds_between(started_at_, snapshot.now);
        if (!std::isfinite(elapsed_ms) || elapsed_ms < 0.0 || elapsed_ms > config_.max_duration_ms) {
            const LiveMavlinkOutputSafetyResult safety{false, "max_duration_reached"};
            audit_.record_command(snapshot.command, safety);
            stop("max_duration_reached");
            return LiveMavlinkOutputSessionResult{false, safety};
        }
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
    ++commands_sent_;
    return LiveMavlinkOutputSessionResult{true, safety};
}

bool LiveMavlinkOutputSession::running() const {
    return running_;
}

} // namespace vh
