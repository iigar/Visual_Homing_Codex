#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "visual_homing/mavlink_telemetry_inspector.hpp"

namespace vh {

struct MavlinkTelemetryStreamConfig {
    std::string device_path;
    int baud_rate = 57600;
};

struct MavlinkTelemetryStreamSnapshot {
    bool supported = false;
    bool opened = false;
    bool running = false;
    std::uint64_t bytes_captured = 0;
    MavlinkTelemetryInspectionSummary inspection{};
};

class MavlinkTelemetryStream final {
public:
    explicit MavlinkTelemetryStream(MavlinkTelemetryStreamConfig config);
    MavlinkTelemetryStream(const MavlinkTelemetryStream&) = delete;
    MavlinkTelemetryStream& operator=(const MavlinkTelemetryStream&) = delete;
    ~MavlinkTelemetryStream();

    bool start();
    void stop();
    MavlinkTelemetryStreamSnapshot snapshot() const;
    const std::string& last_error() const;

private:
    void read_loop();

    MavlinkTelemetryStreamConfig config_;
    mutable std::mutex mutex_;
    std::thread worker_;
    bool stop_requested_ = false;
    bool supported_ = false;
    bool opened_ = false;
    bool running_ = false;
    std::uint64_t bytes_captured_ = 0;
    std::string bytes_;
    std::string last_error_;
};

} // namespace vh
