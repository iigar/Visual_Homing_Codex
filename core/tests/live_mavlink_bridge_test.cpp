#include <cassert>
#include <stdexcept>

#include "visual_homing/live_mavlink_bridge.hpp"

int main() {
    vh::LiveMavlinkBridge bridge;
    assert(!bridge.available());
    assert(!bridge.running());
    assert(!bridge.unavailable_reason().empty());

    assert(!bridge.start());
    assert(!bridge.running());

    vh::NavigationCommand command;
    command.valid = true;
    command.vx_mps = 1.0;
    command.yaw_rate_radps = 0.1;
    command.confidence = 1.0;

    bool rejected = false;
    try {
        bridge.send(command);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
    assert(!bridge.running());

    bridge.stop();
    assert(!bridge.running());

    return 0;
}
