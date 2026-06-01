#pragma once

#include <cstdint>
#include <string>

#include "visual_homing/mavlink.hpp"

namespace vh {

struct MavlinkTelemetryInspectionSummary {
    std::uint64_t bytes_read = 0;
    std::uint64_t frames_seen = 0;
    std::uint64_t mavlink1_frames = 0;
    std::uint64_t mavlink2_frames = 0;
    std::uint64_t malformed_frames = 0;
    std::uint64_t heartbeat_messages = 0;
    std::uint64_t attitude_messages = 0;
    std::uint64_t global_position_int_messages = 0;
    std::uint32_t heartbeat_custom_mode = 0;
    std::uint8_t heartbeat_type = 0;
    std::uint8_t heartbeat_autopilot = 0;
    std::uint8_t heartbeat_base_mode = 0;
    std::uint8_t heartbeat_system_status = 0;
    std::uint8_t heartbeat_mavlink_version = 0;
    MavlinkTelemetry latest{};
};

MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_bytes(const std::string& bytes);
MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_file(const std::string& path);
std::string to_string(FlightMode mode);

} // namespace vh
