#pragma once

#include <iosfwd>
#include <vector>

#include "visual_homing/interfaces.hpp"

namespace vh {

class DryRunMavlinkBridge final : public MavlinkBridge, public MavlinkTelemetrySource {
public:
    DryRunMavlinkBridge(std::vector<MavlinkTelemetry> telemetry_script, std::ostream* output = nullptr);

    bool start() override;
    void stop() override;
    void send(const NavigationCommand& command) override;
    std::optional<MavlinkTelemetry> poll_telemetry() override;

    bool running() const;
    const std::vector<NavigationCommand>& commands() const;

private:
    std::vector<MavlinkTelemetry> telemetry_script_;
    std::ostream* output_ = nullptr;
    std::vector<NavigationCommand> commands_;
    std::size_t next_telemetry_index_ = 0;
    bool running_ = false;
};

const char* flight_mode_name(FlightMode mode);

} // namespace vh
