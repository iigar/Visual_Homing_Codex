#pragma once

#include <cstdint>
#include <string>

namespace vh {

struct MavlinkTelemetryCaptureConfig {
    std::string device_path;
    int baud_rate = 57600;
    std::uint64_t duration_ms = 1000;
    std::string output_path;
};

struct MavlinkTelemetryCaptureSummary {
    bool supported = false;
    bool opened = false;
    std::uint64_t bytes_captured = 0;
    double elapsed_ms = 0.0;
    std::string output_path;
};

MavlinkTelemetryCaptureSummary capture_mavlink_telemetry_file(const MavlinkTelemetryCaptureConfig& config);

} // namespace vh
