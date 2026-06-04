#include "visual_homing/dry_run_command_sink.hpp"

#include <ostream>
#include <stdexcept>

namespace vh {

DryRunCommandSink::DryRunCommandSink(std::ostream* output, std::size_t max_command_history)
    : output_(output),
      max_command_history_(max_command_history) {
    if (max_command_history_ == 0) {
        throw std::invalid_argument("DryRunCommandSink max command history must be positive");
    }
}

bool DryRunCommandSink::start() {
    commands_.clear();
    commands_sent_ = 0;
    commands_dropped_ = 0;
    running_ = true;
    return true;
}

void DryRunCommandSink::stop() {
    running_ = false;
}

void DryRunCommandSink::send(const NavigationCommand& command) {
    if (!running_) {
        throw std::runtime_error("DryRunCommandSink send called while stopped");
    }

    ++commands_sent_;
    commands_.push_back(command);
    if (commands_.size() > max_command_history_) {
        commands_.erase(commands_.begin());
        ++commands_dropped_;
    }
    if (output_ != nullptr) {
        *output_ << "dry_run_command valid=" << (command.valid ? "true" : "false")
                 << " vx_mps=" << command.vx_mps
                 << " vy_mps=" << command.vy_mps
                 << " yaw_rate_radps=" << command.yaw_rate_radps
                 << " confidence=" << command.confidence << "\n";
    }
}

bool DryRunCommandSink::running() const {
    return running_;
}

const std::vector<NavigationCommand>& DryRunCommandSink::commands() const {
    return commands_;
}

std::uint64_t DryRunCommandSink::commands_sent() const {
    return commands_sent_;
}

std::uint64_t DryRunCommandSink::commands_dropped() const {
    return commands_dropped_;
}

} // namespace vh
