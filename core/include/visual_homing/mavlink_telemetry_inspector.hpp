#pragma once

#include <cstdint>
#include <map>
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
    std::uint64_t altitude_messages = 0;
    std::map<std::uint32_t, std::uint64_t> message_id_counts;
    std::uint32_t heartbeat_custom_mode = 0;
    std::uint8_t heartbeat_type = 0;
    std::uint8_t heartbeat_autopilot = 0;
    std::uint8_t heartbeat_base_mode = 0;
    std::uint8_t heartbeat_system_status = 0;
    std::uint8_t heartbeat_mavlink_version = 0;
    MavlinkTelemetry latest{};
};

struct MavlinkTelemetryValidationConfig {
    std::uint64_t minimum_heartbeat_messages = 1;
    std::uint64_t minimum_attitude_messages = 1;
    std::uint64_t minimum_global_position_int_messages = 1;
    std::uint64_t maximum_malformed_frames = 0;
};

struct MavlinkTelemetryValidationResult {
    bool passed = false;
    bool heartbeat_passed = false;
    bool attitude_passed = false;
    bool global_position_int_passed = false;
    bool altitude_passed = false;
    bool malformed_passed = false;
};

MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_bytes(const std::string& bytes);
MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_file(const std::string& path);
MavlinkTelemetryValidationResult validate_mavlink_telemetry(
    const MavlinkTelemetryInspectionSummary& summary,
    const MavlinkTelemetryValidationConfig& config);
std::string to_string(FlightMode mode);
std::string format_mavlink_message_id_counts(const std::map<std::uint32_t, std::uint64_t>& counts);

} // namespace vh
