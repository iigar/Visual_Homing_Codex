#pragma once

#include <string>

#include "visual_homing/interfaces.hpp"

namespace vh {

class LiveMavlinkBridge final : public MavlinkBridge {
public:
    LiveMavlinkBridge() = default;

    static constexpr bool command_output_compiled_out() noexcept {
        return true;
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
