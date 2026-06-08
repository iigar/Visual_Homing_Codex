#pragma once

#include <string>

#include "visual_homing/interfaces.hpp"

#ifndef VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED
#define VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED 0
#endif

#ifndef VISUAL_HOMING_BENCH_PROPS_OFF_LIVE_OUTPUT_SCOPE
#define VISUAL_HOMING_BENCH_PROPS_OFF_LIVE_OUTPUT_SCOPE 0
#endif

#ifndef VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE
#define VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE 0
#endif

namespace vh {

class LiveMavlinkCommandWriter {
public:
    virtual ~LiveMavlinkCommandWriter() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void send(const NavigationCommand& command) = 0;
    virtual bool running() const = 0;
    virtual std::string unavailable_reason() const = 0;
};

class LiveMavlinkBridge final : public MavlinkBridge {
public:
    LiveMavlinkBridge() = default;
    explicit LiveMavlinkBridge(LiveMavlinkCommandWriter& writer);

    static constexpr bool command_output_compiled_out() noexcept {
        return VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE == 0;
    }

    static constexpr bool command_output_available() noexcept {
        return VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE == 1;
    }

    static constexpr bool build_requested() noexcept {
        return VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED == 1;
    }

    static constexpr bool bench_props_off_scope() noexcept {
        return VISUAL_HOMING_BENCH_PROPS_OFF_LIVE_OUTPUT_SCOPE == 1;
    }

    bool start() override;
    void stop() override;
    void send(const NavigationCommand& command) override;

    bool running() const;
    bool available() const;
    std::string unavailable_reason() const;

private:
    LiveMavlinkCommandWriter* writer_ = nullptr;
    bool running_ = false;
};

} // namespace vh
