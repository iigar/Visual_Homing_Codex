#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "visual_homing/mavlink.hpp"

namespace vh {

// One explicitly selected producer for all decoded telemetry. Zero is unconfigured,
// never a wildcard or an instruction to trust the first packet on the wire.
struct MavlinkTelemetrySourceId {
    std::uint8_t system_id = 0;
    std::uint8_t component_id = 0;

    bool configured() const { return system_id != 0 && component_id != 0; }
    bool operator==(const MavlinkTelemetrySourceId&) const = default;
};

struct MavlinkTelemetryInspectionSummary {
    MavlinkTelemetrySourceId selected_source{};
    // Source counters cover only structurally accepted, CRC-checked known IDs.
    std::uint64_t selected_source_frames = 0;
    std::uint64_t unselected_source_frames = 0;
    std::uint64_t invalid_source_frames = 0;
    bool heartbeat_contract_passed = false;
    std::uint64_t bytes_read = 0;
    std::uint64_t frames_seen = 0;
    std::uint64_t mavlink1_frames = 0;
    std::uint64_t mavlink2_frames = 0;
    std::uint64_t malformed_frames = 0;
    // Counts below describe complete frames. Unsupported IDs are counted but
    // never decoded or CRC-validated. Rejected frames cannot refresh receipts.
    std::uint64_t checksum_errors = 0;
    std::uint64_t unsupported_message_frames = 0;
    std::uint64_t unsupported_incompatibility_frames = 0;
    std::uint64_t unsupported_signed_frames = 0;
    std::uint64_t heartbeat_messages = 0;
    std::uint64_t attitude_messages = 0;
    std::uint64_t global_position_int_messages = 0;
    std::uint64_t altitude_messages = 0;
    std::uint64_t distance_sensor_messages = 0;
    std::uint64_t optical_flow_messages = 0;
    std::uint64_t optical_flow_rad_messages = 0;
    std::uint64_t relative_altitude_samples = 0;
    double relative_altitude_min_m = 0.0;
    double relative_altitude_avg_m = 0.0;
    double relative_altitude_max_m = 0.0;
    std::map<std::uint32_t, std::uint64_t> message_id_counts;
    bool distance_sensor_seen = false;
    double distance_sensor_current_m = 0.0;
    double distance_sensor_min_m = 0.0;
    double distance_sensor_max_m = 0.0;
    double distance_sensor_current_min_m = 0.0;
    double distance_sensor_current_avg_m = 0.0;
    double distance_sensor_current_max_m = 0.0;
    std::uint8_t distance_sensor_type = 0;
    std::uint8_t distance_sensor_id = 0;
    std::uint8_t distance_sensor_orientation = 0;
    bool optical_flow_distance_seen = false;
    double optical_flow_distance_m = 0.0;
    std::uint8_t optical_flow_quality = 0;
    std::uint32_t heartbeat_custom_mode = 0;
    std::uint8_t heartbeat_type = 0;
    std::uint8_t heartbeat_autopilot = 0;
    std::uint8_t heartbeat_base_mode = 0;
    std::uint8_t heartbeat_system_status = 0;
    std::uint8_t heartbeat_mavlink_version = 0;
    MavlinkTelemetry latest{};
    // Exclusive frame-end offsets in the inspected bytes; zero means absent.
    // These locate the messages supplying latest fields, not a cumulative count.
    std::uint64_t heartbeat_end_offset = 0;
    std::uint64_t attitude_end_offset = 0;
    std::uint64_t relative_altitude_end_offset = 0;
};

struct MavlinkTelemetryValidationConfig {
    std::uint64_t minimum_heartbeat_messages = 1;
    std::uint64_t minimum_attitude_messages = 1;
    std::uint64_t minimum_global_position_int_messages = 1;
    std::uint64_t maximum_malformed_frames = 0;
    MavlinkTelemetrySourceId expected_source{};
};

struct MavlinkTelemetryValidationResult {
    bool passed = false;
    bool source_passed = false;
    bool heartbeat_contract_passed = false;
    bool heartbeat_passed = false;
    bool attitude_passed = false;
    bool global_position_int_passed = false;
    bool altitude_passed = false;
    bool malformed_passed = false;
};

// An unconfigured source performs structural inspection only: no decoded values
// or receipt offsets, and validation always fails closed.
MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_bytes(
    const std::string& bytes, MavlinkTelemetrySourceId source = {});
MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_file(
    const std::string& path, MavlinkTelemetrySourceId source = {});
MavlinkTelemetryValidationResult validate_mavlink_telemetry(
    const MavlinkTelemetryInspectionSummary& summary,
    const MavlinkTelemetryValidationConfig& config);
std::string to_string(FlightMode mode);
std::string format_mavlink_message_id_counts(const std::map<std::uint32_t, std::uint64_t>& counts);

} // namespace vh
