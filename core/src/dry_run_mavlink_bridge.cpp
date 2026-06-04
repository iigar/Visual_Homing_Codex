#include "visual_homing/dry_run_mavlink_bridge.hpp"

#include <ostream>
#include <stdexcept>

namespace vh {

const char* flight_mode_name(FlightMode mode) {
    switch (mode) {
    case FlightMode::Unknown:
        return "unknown";
    case FlightMode::Manual:
        return "manual";
    case FlightMode::Stabilize:
        return "stabilize";
    case FlightMode::AltHold:
        return "alt_hold";
    case FlightMode::Guided:
        return "guided";
    case FlightMode::Auto:
        return "auto";
    case FlightMode::Rtl:
        return "rtl";
    case FlightMode::Land:
        return "land";
    }
    return "unknown";
}

DryRunMavlinkBridge::DryRunMavlinkBridge(
    std::vector<MavlinkTelemetry> telemetry_script,
    std::ostream* output,
    std::size_t max_command_history)
    : telemetry_script_(std::move(telemetry_script)),
      output_(output),
      max_command_history_(max_command_history) {
    if (max_command_history_ == 0) {
        throw std::invalid_argument("DryRunMavlinkBridge max command history must be positive");
    }
}

bool DryRunMavlinkBridge::start() {
    next_telemetry_index_ = 0;
    commands_.clear();
    commands_sent_ = 0;
    commands_dropped_ = 0;
    running_ = true;
    return true;
}

void DryRunMavlinkBridge::stop() {
    running_ = false;
}

void DryRunMavlinkBridge::send(const NavigationCommand& command) {
    if (!running_) {
        throw std::runtime_error("DryRunMavlinkBridge send called while stopped");
    }

    ++commands_sent_;
    commands_.push_back(command);
    if (commands_.size() > max_command_history_) {
        commands_.erase(commands_.begin());
        ++commands_dropped_;
    }
    if (output_ != nullptr) {
        *output_ << "mavlink_dry_run_command valid=" << (command.valid ? "true" : "false")
                 << " vx_mps=" << command.vx_mps
                 << " vy_mps=" << command.vy_mps
                 << " yaw_rate_radps=" << command.yaw_rate_radps
                 << " confidence=" << command.confidence << "\n";
    }
}

std::optional<MavlinkTelemetry> DryRunMavlinkBridge::poll_telemetry() {
    if (!running_ || next_telemetry_index_ >= telemetry_script_.size()) {
        return std::nullopt;
    }

    const auto telemetry = telemetry_script_[next_telemetry_index_++];
    if (output_ != nullptr) {
        *output_ << "mavlink_dry_run_telemetry heartbeat=" << (telemetry.heartbeat_seen ? "true" : "false")
                 << " armed=" << (telemetry.armed ? "true" : "false")
                 << " mode=" << flight_mode_name(telemetry.mode)
                 << " roll_rad=" << telemetry.roll_rad
                 << " pitch_rad=" << telemetry.pitch_rad
                 << " yaw_rad=" << telemetry.yaw_rad
                 << " relative_altitude_m=" << telemetry.relative_altitude_m << "\n";
    }
    return telemetry;
}

bool DryRunMavlinkBridge::running() const {
    return running_;
}

const std::vector<NavigationCommand>& DryRunMavlinkBridge::commands() const {
    return commands_;
}

std::uint64_t DryRunMavlinkBridge::commands_sent() const {
    return commands_sent_;
}

std::uint64_t DryRunMavlinkBridge::commands_dropped() const {
    return commands_dropped_;
}

} // namespace vh
