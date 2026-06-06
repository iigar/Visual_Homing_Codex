#include "visual_homing/live_mavlink_output_audit_log.hpp"

#include <stdexcept>
#include <utility>

namespace vh {
namespace {

const char* flight_mode_word(const FlightMode mode) {
    switch (mode) {
    case FlightMode::Unknown:
        return "Unknown";
    case FlightMode::Manual:
        return "Manual";
    case FlightMode::Stabilize:
        return "Stabilize";
    case FlightMode::AltHold:
        return "AltHold";
    case FlightMode::Guided:
        return "Guided";
    case FlightMode::Auto:
        return "Auto";
    case FlightMode::Rtl:
        return "Rtl";
    case FlightMode::Land:
        return "Land";
    }
    return "Unknown";
}

} // namespace

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
    const LiveMavlinkOutputSafetySnapshot& snapshot,
    const LiveMavlinkOutputSafetyResult& safety_result) {
    if (!ready_) {
        throw std::runtime_error("LiveMavlinkOutputAuditLog record_command called before ready");
    }

    const auto telemetry_age_ms = milliseconds_between(snapshot.telemetry.timestamp, snapshot.now);
    const auto match_age_ms = milliseconds_between(snapshot.match.timestamp, snapshot.now);
    const auto& command = snapshot.command;
    output_ << "live_output_audit event=command"
            << " allowed=" << (safety_result.allowed ? "true" : "false")
            << " decision=" << (safety_result.allowed ? "allowed" : "blocked")
            << " reason=" << safety_result.reason
            << " valid=" << (command.valid ? "true" : "false")
            << " vx_mps=" << command.vx_mps
            << " vy_mps=" << command.vy_mps
            << " yaw_rate_radps=" << command.yaw_rate_radps
            << " confidence=" << command.confidence
            << " telemetry_heartbeat_seen=" << (snapshot.telemetry.heartbeat_seen ? "true" : "false")
            << " telemetry_armed=" << (snapshot.telemetry.armed ? "true" : "false")
            << " telemetry_mode=" << flight_mode_word(snapshot.telemetry.mode)
            << " telemetry_age_ms=" << telemetry_age_ms
            << " route_match_valid=" << (snapshot.match.valid ? "true" : "false")
            << " route_match_confidence=" << snapshot.match.confidence
            << " route_match_progress=" << snapshot.match.progress
            << " route_match_age_ms=" << match_age_ms << "\n";
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
