#pragma once

#include <iosfwd>
#include <vector>

#include "visual_homing/interfaces.hpp"

namespace vh {

class DryRunCommandSink final : public MavlinkBridge {
public:
    explicit DryRunCommandSink(std::ostream* output = nullptr);

    bool start() override;
    void stop() override;
    void send(const NavigationCommand& command) override;

    bool running() const;
    const std::vector<NavigationCommand>& commands() const;

private:
    std::ostream* output_ = nullptr;
    std::vector<NavigationCommand> commands_;
    bool running_ = false;
};

} // namespace vh
