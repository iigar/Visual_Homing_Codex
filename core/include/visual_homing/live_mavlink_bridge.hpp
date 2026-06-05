#pragma once

#include <string>

#include "visual_homing/interfaces.hpp"

#ifndef VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED
#define VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED 0
#endif

#ifndef VISUAL_HOMING_BENCH_PROPS_OFF_LIVE_OUTPUT_SCOPE
#define VISUAL_HOMING_BENCH_PROPS_OFF_LIVE_OUTPUT_SCOPE 0
#endif

namespace vh {

class LiveMavlinkBridge final : public MavlinkBridge {
public:
    LiveMavlinkBridge() = default;

    static constexpr bool command_output_compiled_out() noexcept {
        return true;
    }

    static constexpr bool command_output_available() noexcept {
        return false;
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
    bool running_ = false;
};

} // namespace vh
