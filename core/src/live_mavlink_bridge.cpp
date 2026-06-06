#include "visual_homing/live_mavlink_bridge.hpp"

#include <stdexcept>

namespace vh {
namespace {

constexpr const char* blocked_reason =
    "Live MAVLink command output is intentionally blocked in this build; bench props-off writer is not implemented";

} // namespace

LiveMavlinkBridge::LiveMavlinkBridge(LiveMavlinkCommandWriter& writer)
    : writer_(&writer) {}

bool LiveMavlinkBridge::start() {
    if (running_) {
        return true;
    }
    if (writer_ == nullptr) {
        running_ = false;
        return false;
    }
    if (!writer_->start()) {
        running_ = false;
        return false;
    }
    running_ = true;
    return true;
}

void LiveMavlinkBridge::stop() {
    if (running_ && writer_ != nullptr) {
        writer_->stop();
    }
    running_ = false;
}

void LiveMavlinkBridge::send(const NavigationCommand& command) {
    if (!running_ || writer_ == nullptr) {
        throw std::runtime_error(blocked_reason);
    }
    writer_->send(command);
}

bool LiveMavlinkBridge::running() const {
    return running_;
}

bool LiveMavlinkBridge::available() const {
    return writer_ != nullptr;
}

std::string LiveMavlinkBridge::unavailable_reason() const {
    if (writer_ != nullptr) {
        return writer_->unavailable_reason();
    }
    return blocked_reason;
}

} // namespace vh
