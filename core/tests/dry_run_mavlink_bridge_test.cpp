#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/dry_run_mavlink_bridge.hpp"

int main() {
    std::ostringstream output;

    vh::MavlinkTelemetry first;
    first.heartbeat_seen = true;
    first.armed = true;
    first.mode = vh::FlightMode::Guided;
    first.roll_rad = 0.01;
    first.pitch_rad = -0.02;
    first.yaw_rad = 1.2;
    first.relative_altitude_m = 75.0;

    vh::MavlinkTelemetry second;
    second.heartbeat_seen = true;
    second.mode = vh::FlightMode::Rtl;
    second.relative_altitude_m = 74.5;

    vh::DryRunMavlinkBridge bridge({first, second}, &output);

    bool rejected = false;
    try {
        vh::NavigationCommand command;
        bridge.send(command);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    assert(bridge.start());
    assert(bridge.running());

    const auto telemetry_a = bridge.poll_telemetry();
    assert(telemetry_a.has_value());
    assert(telemetry_a->heartbeat_seen);
    assert(telemetry_a->armed);
    assert(telemetry_a->mode == vh::FlightMode::Guided);
    assert(telemetry_a->relative_altitude_m == 75.0);

    vh::NavigationCommand command;
    command.valid = true;
    command.yaw_rate_radps = 0.15;
    command.confidence = 0.91;
    bridge.send(command);
    assert(bridge.commands().size() == 1);

    const auto telemetry_b = bridge.poll_telemetry();
    assert(telemetry_b.has_value());
    assert(telemetry_b->mode == vh::FlightMode::Rtl);
    assert(!bridge.poll_telemetry().has_value());

    bridge.stop();
    assert(!bridge.running());

    const auto text = output.str();
    assert(text.find("mavlink_dry_run_telemetry heartbeat=true") != std::string::npos);
    assert(text.find("mode=guided") != std::string::npos);
    assert(text.find("mode=rtl") != std::string::npos);
    assert(text.find("mavlink_dry_run_command valid=true") != std::string::npos);

    vh::DryRunMavlinkBridge bounded({}, nullptr, 2);
    assert(bounded.start());
    vh::NavigationCommand first_command;
    first_command.yaw_rate_radps = 0.1;
    vh::NavigationCommand second_command;
    second_command.yaw_rate_radps = 0.2;
    vh::NavigationCommand third_command;
    third_command.yaw_rate_radps = 0.3;
    bounded.send(first_command);
    bounded.send(second_command);
    bounded.send(third_command);
    assert(bounded.commands_sent() == 3);
    assert(bounded.commands_dropped() == 1);
    assert(bounded.commands().size() == 2);
    assert(bounded.commands()[0].yaw_rate_radps == 0.2);
    assert(bounded.commands()[1].yaw_rate_radps == 0.3);

    rejected = false;
    try {
        (void)vh::DryRunMavlinkBridge({}, nullptr, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    return 0;
}
