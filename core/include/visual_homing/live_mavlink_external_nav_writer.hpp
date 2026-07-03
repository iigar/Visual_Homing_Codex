#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "visual_homing/external_nav_estimator.hpp"
#include "visual_homing/live_mavlink_serial_writer.hpp"

namespace vh {

struct LiveMavlinkExternalNavWriterConfig {
    std::string device_path;
    int baud_rate = 115200;
    std::uint8_t source_system = 191;
    std::uint8_t source_component = 1;
};

class LiveMavlinkExternalNavWriter final {
public:
    LiveMavlinkExternalNavWriter(LiveMavlinkExternalNavWriterConfig config,
                                 LiveMavlinkByteTransport& transport);

    bool start();
    void stop();
    void send_vision_position_estimate(const ExternalNavEstimate& estimate, std::uint64_t time_usec);
    bool running() const;
    std::string unavailable_reason() const;

private:
    void validate_estimate(const ExternalNavEstimate& estimate) const;

    LiveMavlinkExternalNavWriterConfig config_;
    LiveMavlinkByteTransport* transport_ = nullptr;
    std::uint8_t sequence_ = 0;
    std::string unavailable_reason_;
};

std::vector<std::uint8_t> encode_mavlink2_vision_position_estimate(
    const ExternalNavEstimate& estimate,
    std::uint64_t time_usec,
    std::uint8_t sequence,
    std::uint8_t source_system,
    std::uint8_t source_component);

} // namespace vh
