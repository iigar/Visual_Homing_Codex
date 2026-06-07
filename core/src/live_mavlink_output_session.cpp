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
    bridge_started_ = false;
    if (!audit_.start(config_.run_id) || !audit_.ready()) {
        return false;
    }
    started_at_ = now();
    commands_sent_ = 0;
    running_ = true;
    return true;
}

void LiveMavlinkOutputSession::stop(const std::string& reason) {
    if (bridge_started_) {
        bridge_.stop();
        bridge_started_ = false;
    }
    running_ = false;
    audit_.stop(reason);
}

LiveMavlinkOutputSessionResult LiveMavlinkOutputSession::process(
    const LiveMavlinkOutputSafetySnapshot& snapshot) {
    if (!running_) {
        throw std::runtime_error("LiveMavlinkOutputSession process called while stopped");
    }

    const auto record_or_stop = [&](const LiveMavlinkOutputSafetyResult& result) {
        try {
            audit_.record_command(snapshot, result);
        } catch (...) {
            stop("audit_record_failed");
            throw;
        }
    };

    if (config_.max_commands > 0 && commands_sent_ >= config_.max_commands) {
        const LiveMavlinkOutputSafetyResult safety{false, "max_command_count_reached"};
        record_or_stop(safety);
        stop("max_command_count_reached");
        return LiveMavlinkOutputSessionResult{false, safety};
    }

    if (config_.max_duration_ms > 0.0) {
        const auto elapsed_ms = milliseconds_between(started_at_, snapshot.now);
        if (!std::isfinite(elapsed_ms) || elapsed_ms < 0.0 || elapsed_ms > config_.max_duration_ms) {
            const LiveMavlinkOutputSafetyResult safety{false, "max_duration_reached"};
            record_or_stop(safety);
            stop("max_duration_reached");
            return LiveMavlinkOutputSessionResult{false, safety};
        }
    }

    auto safety_config = config_.safety_config;
    safety_config.audit_log_ready = audit_.ready();
    const LiveMavlinkOutputSafetyGate gate(safety_config);
    const auto safety = gate.evaluate(snapshot);

    if (!safety.allowed) {
        record_or_stop(safety);
        return LiveMavlinkOutputSessionResult{false, safety};
    }

    if (!bridge_started_) {
        if (!bridge_.start()) {
            const LiveMavlinkOutputSafetyResult failed{false, "bridge_start_failed"};
            record_or_stop(failed);
            stop("bridge_start_failed");
            return LiveMavlinkOutputSessionResult{false, failed};
        }
        bridge_started_ = true;
    }

    record_or_stop(safety);

    try {
        bridge_.send(snapshot.command);
    } catch (const std::exception&) {
        const LiveMavlinkOutputSafetyResult failed{false, "send_failed"};
        stop("send_failed");
        return LiveMavlinkOutputSessionResult{false, failed};
    }

    ++commands_sent_;
    return LiveMavlinkOutputSessionResult{true, safety};
}

bool LiveMavlinkOutputSession::running() const {
    return running_;
}

} // namespace vh
