#include "visual_homing/live_mavlink_output_audit_log.hpp"

#include <stdexcept>
#include <utility>

namespace vh {

LiveMavlinkOutputAuditLog::LiveMavlinkOutputAuditLog(LiveMavlinkOutputAuditLogConfig config)
    : config_(std::move(config)) {}

bool LiveMavlinkOutputAuditLog::start(const std::string& run_id) {
    ready_ = false;
    if (output_.is_open()) {
        output_.close();
    }
    if (config_.path.empty()) {
        return false;
    }

    const auto parent = config_.path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const auto mode = std::ios::out | (config_.append ? std::ios::app : std::ios::trunc);
    output_.open(config_.path, mode);
    if (!output_.is_open() || !output_.good()) {
        return false;
    }

    output_ << "live_output_audit event=start run_id=" << run_id << "\n";
    output_.flush();
    ready_ = output_.good();
    if (!ready_) {
        output_.close();
    }
    return ready_;
}

void LiveMavlinkOutputAuditLog::stop(const std::string& reason) {
    if (!ready_) {
        return;
    }
    output_ << "live_output_audit event=stop reason=" << reason << "\n";
    output_.flush();
    output_.close();
    ready_ = false;
}

void LiveMavlinkOutputAuditLog::record_command(
    const NavigationCommand& command,
    const LiveMavlinkOutputSafetyResult& safety_result) {
    if (!ready_) {
        throw std::runtime_error("LiveMavlinkOutputAuditLog record_command called before ready");
    }

    output_ << "live_output_audit event=command"
            << " allowed=" << (safety_result.allowed ? "true" : "false")
            << " reason=" << safety_result.reason
            << " valid=" << (command.valid ? "true" : "false")
            << " vx_mps=" << command.vx_mps
            << " vy_mps=" << command.vy_mps
            << " yaw_rate_radps=" << command.yaw_rate_radps
            << " confidence=" << command.confidence << "\n";
    output_.flush();
    if (!output_.good()) {
        ready_ = false;
        throw std::runtime_error("LiveMavlinkOutputAuditLog write failed");
    }
}

bool LiveMavlinkOutputAuditLog::ready() const {
    return ready_;
}

const std::filesystem::path& LiveMavlinkOutputAuditLog::path() const {
    return config_.path;
}

} // namespace vh
