#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

#include "visual_homing/interfaces.hpp"

namespace vh {

class DryRunCommandSink final : public MavlinkBridge {
public:
    explicit DryRunCommandSink(std::ostream* output = nullptr, std::size_t max_command_history = 10000);

    bool start() override;
    void stop() override;
    void send(const NavigationCommand& command) override;

    bool running() const;
    const std::vector<NavigationCommand>& commands() const;
    std::uint64_t commands_sent() const;
    std::uint64_t commands_dropped() const;

private:
    std::ostream* output_ = nullptr;
    std::size_t max_command_history_ = 0;
    std::uint64_t commands_sent_ = 0;
    std::uint64_t commands_dropped_ = 0;
    std::vector<NavigationCommand> commands_;
    bool running_ = false;
};

} // namespace vh
