#include "visual_homing/live_mavlink_bridge.hpp"

#include <stdexcept>

namespace vh {
namespace {

constexpr const char* blocked_reason =
    "Live MAVLink command output is intentionally blocked in this build; bench props-off writer is not implemented";

} // namespace

bool LiveMavlinkBridge::start() {
    running_ = false;
    return false;
}

void LiveMavlinkBridge::stop() {
    running_ = false;
}

void LiveMavlinkBridge::send(const NavigationCommand&) {
    throw std::runtime_error(blocked_reason);
}

bool LiveMavlinkBridge::running() const {
    return running_;
}

bool LiveMavlinkBridge::available() const {
    return false;
}

std::string LiveMavlinkBridge::unavailable_reason() const {
    return blocked_reason;
}

} // namespace vh
