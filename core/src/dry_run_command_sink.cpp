#include "visual_homing/dry_run_command_sink.hpp"

#include <ostream>
#include <stdexcept>

namespace vh {

DryRunCommandSink::DryRunCommandSink(std::ostream* output)
    : output_(output) {}

bool DryRunCommandSink::start() {
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

    commands_.push_back(command);
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

} // namespace vh
