#include <cassert>
#include <stdexcept>
#include <string>

#include "visual_homing/live_mavlink_bridge.hpp"

namespace {

class FakeLiveWriter final : public vh::LiveMavlinkCommandWriter {
public:
    bool start_result = true;
    bool running_state = false;
    int starts = 0;
    int stops = 0;
    int sends = 0;
    std::string reason = "fake writer unavailable";

    bool start() override {
        ++starts;
        running_state = start_result;
        return start_result;
    }

    void stop() override {
        ++stops;
        running_state = false;
    }

    void send(const vh::NavigationCommand&) override {
        if (!running_state) {
            throw std::runtime_error("fake writer send called while stopped");
        }
        ++sends;
    }

    bool running() const override {
        return running_state;
    }

    std::string unavailable_reason() const override {
        return reason;
    }
};

vh::NavigationCommand valid_command() {
    vh::NavigationCommand command;
    command.valid = true;
    command.vx_mps = 0.0;
    command.yaw_rate_radps = 0.1;
    command.confidence = 1.0;
    return command;
}

} // namespace

int main() {
#if VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE
    static_assert(VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BLOCKED == 0);
    static_assert(!vh::LiveMavlinkBridge::command_output_compiled_out());
    static_assert(vh::LiveMavlinkBridge::command_output_available());
#else
    static_assert(VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BLOCKED == 1);
    static_assert(VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE == 0);
    static_assert(vh::LiveMavlinkBridge::command_output_compiled_out());
    static_assert(!vh::LiveMavlinkBridge::command_output_available());
#endif
#if VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED
    static_assert(vh::LiveMavlinkBridge::build_requested());
    static_assert(vh::LiveMavlinkBridge::bench_props_off_scope());
#else
    static_assert(!vh::LiveMavlinkBridge::build_requested());
    static_assert(!vh::LiveMavlinkBridge::bench_props_off_scope());
#endif

    vh::LiveMavlinkBridge bridge;
    assert(!bridge.available());
    assert(!bridge.running());
    assert(!bridge.unavailable_reason().empty());

    assert(!bridge.start());
    assert(!bridge.running());

    const auto command = valid_command();

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

    {
        FakeLiveWriter writer;
        vh::LiveMavlinkBridge writer_bridge(writer);
        assert(writer_bridge.available());
        assert(!writer_bridge.running());
        assert(writer_bridge.unavailable_reason() == "fake writer unavailable");

        bool stopped_send_rejected = false;
        try {
            writer_bridge.send(command);
        } catch (const std::runtime_error&) {
            stopped_send_rejected = true;
        }
        assert(stopped_send_rejected);
        assert(writer.sends == 0);

        assert(writer_bridge.start());
        assert(writer_bridge.running());
        assert(writer.starts == 1);
        assert(writer_bridge.start());
        assert(writer.starts == 1);

        writer_bridge.send(command);
        assert(writer.sends == 1);

        writer_bridge.stop();
        assert(!writer_bridge.running());
        assert(!writer.running());
        assert(writer.stops == 1);
        writer_bridge.stop();
        assert(writer.stops == 1);
    }

    {
        FakeLiveWriter writer;
        writer.start_result = false;
        vh::LiveMavlinkBridge writer_bridge(writer);
        assert(!writer_bridge.start());
        assert(!writer_bridge.running());
        assert(writer.starts == 1);

        bool rejected_after_failed_start = false;
        try {
            writer_bridge.send(command);
        } catch (const std::runtime_error&) {
            rejected_after_failed_start = true;
        }
        assert(rejected_after_failed_start);
        assert(writer.sends == 0);
    }

    return 0;
}
