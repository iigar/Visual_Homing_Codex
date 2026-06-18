#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include "visual_homing/mavlink_telemetry_inspector.hpp"

namespace {

void append_u32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<unsigned char>(value & 0xFF));
    bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
    bytes.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
}

void append_u64(std::vector<unsigned char>& bytes, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xFF));
    }
}

void append_i32(std::vector<unsigned char>& bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<unsigned char>& bytes, float value) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    append_u32(bytes, raw);
}

std::string mavlink1_frame(std::uint8_t message_id, const std::vector<unsigned char>& payload) {
    std::string frame;
    frame.push_back(static_cast<char>(0xFE));
    frame.push_back(static_cast<char>(payload.size()));
    frame.push_back(static_cast<char>(1));
    frame.push_back(static_cast<char>(1));
    frame.push_back(static_cast<char>(1));
    frame.push_back(static_cast<char>(message_id));
    for (const auto byte : payload) {
        frame.push_back(static_cast<char>(byte));
    }
    frame.push_back(static_cast<char>(0));
    frame.push_back(static_cast<char>(0));
    return frame;
}

std::string mavlink2_frame(std::uint32_t message_id, const std::vector<unsigned char>& payload) {
    std::string frame;
    frame.push_back(static_cast<char>(0xFD));
    frame.push_back(static_cast<char>(payload.size()));
    frame.push_back(static_cast<char>(0));
    frame.push_back(static_cast<char>(0));
    frame.push_back(static_cast<char>(2));
    frame.push_back(static_cast<char>(1));
    frame.push_back(static_cast<char>(1));
    frame.push_back(static_cast<char>(message_id & 0xFF));
    frame.push_back(static_cast<char>((message_id >> 8) & 0xFF));
    frame.push_back(static_cast<char>((message_id >> 16) & 0xFF));
    for (const auto byte : payload) {
        frame.push_back(static_cast<char>(byte));
    }
    frame.push_back(static_cast<char>(0));
    frame.push_back(static_cast<char>(0));
    return frame;
}

} // namespace

int main() {
    std::vector<unsigned char> heartbeat;
    append_u32(heartbeat, 4);
    heartbeat.push_back(2);
    heartbeat.push_back(3);
    heartbeat.push_back(128);
    heartbeat.push_back(4);
    heartbeat.push_back(3);

    std::vector<unsigned char> attitude;
    append_u32(attitude, 1000);
    append_f32(attitude, 0.1F);
    append_f32(attitude, -0.2F);
    append_f32(attitude, 1.3F);
    append_f32(attitude, 0.0F);
    append_f32(attitude, 0.0F);
    append_f32(attitude, 0.0F);

    std::vector<unsigned char> global_position;
    append_u32(global_position, 1000);
    append_i32(global_position, 0);
    append_i32(global_position, 0);
    append_i32(global_position, 0);
    append_i32(global_position, 42500);
    append_i32(global_position, 0);
    global_position.push_back(0);
    global_position.push_back(0);
    global_position.push_back(0);
    global_position.push_back(0);

    const auto bytes =
        std::string("noise") +
        mavlink1_frame(0, heartbeat) +
        mavlink2_frame(30, attitude) +
        mavlink2_frame(33, global_position);

    const auto summary = vh::inspect_mavlink_telemetry_bytes(bytes);
    assert(summary.bytes_read == bytes.size());
    assert(summary.frames_seen == 3);
    assert(summary.mavlink1_frames == 1);
    assert(summary.mavlink2_frames == 2);
    assert(summary.malformed_frames == 0);
    assert(summary.heartbeat_messages == 1);
    assert(summary.attitude_messages == 1);
    assert(summary.global_position_int_messages == 1);
    assert(summary.altitude_messages == 1);
    assert(summary.message_id_counts.at(0) == 1);
    assert(summary.message_id_counts.at(30) == 1);
    assert(summary.message_id_counts.at(33) == 1);
    assert(vh::format_mavlink_message_id_counts(summary.message_id_counts) == "0:1,30:1,33:1");
    assert(summary.heartbeat_custom_mode == 4);
    assert(summary.heartbeat_type == 2);
    assert(summary.heartbeat_autopilot == 3);
    assert(summary.heartbeat_base_mode == 128);
    assert(summary.heartbeat_system_status == 4);
    assert(summary.heartbeat_mavlink_version == 3);
    assert(summary.latest.heartbeat_seen);
    assert(summary.latest.armed);
    assert(summary.latest.mode == vh::FlightMode::Guided);
    assert(summary.latest.roll_rad > 0.09 && summary.latest.roll_rad < 0.11);
    assert(summary.latest.pitch_rad < -0.19 && summary.latest.pitch_rad > -0.21);
    assert(summary.latest.yaw_rad > 1.29 && summary.latest.yaw_rad < 1.31);
    assert(summary.latest.relative_altitude_seen);
    assert(summary.latest.relative_altitude_m == 42.5);
    assert(vh::to_string(summary.latest.mode) == "Guided");

    const auto validation = vh::validate_mavlink_telemetry(summary, {});
    assert(validation.passed);
    assert(validation.heartbeat_passed);
    assert(validation.attitude_passed);
    assert(validation.global_position_int_passed);
    assert(validation.altitude_passed);
    assert(validation.malformed_passed);

    vh::MavlinkTelemetryValidationConfig strict_validation_config;
    strict_validation_config.minimum_heartbeat_messages = 2;
    const auto strict_validation = vh::validate_mavlink_telemetry(summary, strict_validation_config);
    assert(!strict_validation.passed);
    assert(!strict_validation.heartbeat_passed);
    assert(strict_validation.attitude_passed);
    assert(strict_validation.global_position_int_passed);
    assert(strict_validation.altitude_passed);
    assert(strict_validation.malformed_passed);

    std::vector<unsigned char> truncated_global_position;
    append_u32(truncated_global_position, 1000);
    append_i32(truncated_global_position, 0);
    append_i32(truncated_global_position, 0);
    append_i32(truncated_global_position, 0);
    append_i32(truncated_global_position, 42500);
    const auto truncated_summary =
        vh::inspect_mavlink_telemetry_bytes(mavlink2_frame(33, truncated_global_position));
    assert(truncated_summary.malformed_frames == 0);
    assert(truncated_summary.global_position_int_messages == 1);
    assert(truncated_summary.altitude_messages == 1);
    assert(truncated_summary.latest.relative_altitude_seen);
    assert(truncated_summary.latest.relative_altitude_m == 42.5);

    std::vector<unsigned char> altitude;
    append_u64(altitude, 1000);
    append_f32(altitude, 101.0F);
    append_f32(altitude, 102.0F);
    append_f32(altitude, 1.0F);
    append_f32(altitude, 3.25F);
    append_f32(altitude, 0.0F);
    append_f32(altitude, 0.0F);
    const auto altitude_only = vh::inspect_mavlink_telemetry_bytes(
        mavlink2_frame(0, heartbeat) +
        mavlink2_frame(30, attitude) +
        mavlink2_frame(141, altitude));
    assert(altitude_only.global_position_int_messages == 0);
    assert(altitude_only.altitude_messages == 1);
    assert(altitude_only.latest.relative_altitude_seen);
    assert(altitude_only.latest.relative_altitude_m > 3.24);
    assert(altitude_only.latest.relative_altitude_m < 3.26);
    vh::MavlinkTelemetryValidationConfig relaxed_validation_config;
    relaxed_validation_config.minimum_global_position_int_messages = 0;
    const auto relaxed_validation = vh::validate_mavlink_telemetry(altitude_only, relaxed_validation_config);
    assert(relaxed_validation.passed);
    assert(relaxed_validation.global_position_int_passed);
    assert(relaxed_validation.altitude_passed);

    std::vector<unsigned char> truncated_altitude;
    append_u64(truncated_altitude, 1000);
    append_f32(truncated_altitude, 101.0F);
    append_f32(truncated_altitude, 102.0F);
    append_f32(truncated_altitude, 1.0F);
    append_f32(truncated_altitude, 3.25F);
    const auto truncated_altitude_summary =
        vh::inspect_mavlink_telemetry_bytes(mavlink2_frame(141, truncated_altitude));
    assert(truncated_altitude_summary.malformed_frames == 0);
    assert(truncated_altitude_summary.altitude_messages == 1);
    assert(truncated_altitude_summary.latest.relative_altitude_m > 3.24);
    assert(truncated_altitude_summary.latest.relative_altitude_m < 3.26);

    std::vector<unsigned char> distance_sensor;
    append_u32(distance_sensor, 1000);
    distance_sensor.push_back(20);
    distance_sensor.push_back(0);
    distance_sensor.push_back(200);
    distance_sensor.push_back(0);
    distance_sensor.push_back(73);
    distance_sensor.push_back(0);
    distance_sensor.push_back(0);
    distance_sensor.push_back(3);
    distance_sensor.push_back(25);
    distance_sensor.push_back(0);
    const auto rangefinder_summary = vh::inspect_mavlink_telemetry_bytes(mavlink2_frame(132, distance_sensor));
    assert(rangefinder_summary.distance_sensor_messages == 1);
    assert(rangefinder_summary.distance_sensor_seen);
    assert(rangefinder_summary.distance_sensor_current_m > 0.72);
    assert(rangefinder_summary.distance_sensor_current_m < 0.74);
    assert(rangefinder_summary.distance_sensor_min_m > 0.19);
    assert(rangefinder_summary.distance_sensor_min_m < 0.21);
    assert(rangefinder_summary.distance_sensor_max_m > 1.99);
    assert(rangefinder_summary.distance_sensor_max_m < 2.01);
    assert(rangefinder_summary.distance_sensor_id == 3);
    assert(rangefinder_summary.distance_sensor_orientation == 25);

    std::vector<unsigned char> optical_flow_rad;
    append_u64(optical_flow_rad, 1000);
    append_u32(optical_flow_rad, 10000);
    append_f32(optical_flow_rad, 0.01F);
    append_f32(optical_flow_rad, -0.02F);
    append_f32(optical_flow_rad, 0.0F);
    append_f32(optical_flow_rad, 0.0F);
    append_f32(optical_flow_rad, 0.0F);
    optical_flow_rad.push_back(0);
    optical_flow_rad.push_back(0);
    optical_flow_rad.push_back(1);
    optical_flow_rad.push_back(220);
    append_u32(optical_flow_rad, 10000);
    append_f32(optical_flow_rad, 0.73F);
    const auto optical_flow_summary = vh::inspect_mavlink_telemetry_bytes(mavlink2_frame(106, optical_flow_rad));
    assert(optical_flow_summary.optical_flow_rad_messages == 1);
    assert(optical_flow_summary.optical_flow_distance_seen);
    assert(optical_flow_summary.optical_flow_distance_m > 0.72);
    assert(optical_flow_summary.optical_flow_distance_m < 0.74);
    assert(optical_flow_summary.optical_flow_quality == 220);

    std::vector<unsigned char> alt_hold_heartbeat;
    append_u32(alt_hold_heartbeat, 2);
    alt_hold_heartbeat.push_back(2);
    alt_hold_heartbeat.push_back(3);
    alt_hold_heartbeat.push_back(81);
    alt_hold_heartbeat.push_back(3);
    alt_hold_heartbeat.push_back(3);
    const auto alt_hold = vh::inspect_mavlink_telemetry_bytes(mavlink2_frame(0, alt_hold_heartbeat));
    assert(alt_hold.heartbeat_custom_mode == 2);
    assert(!alt_hold.latest.armed);
    assert(alt_hold.latest.mode == vh::FlightMode::AltHold);
    assert(vh::to_string(alt_hold.latest.mode) == "AltHold");

    const auto malformed = vh::inspect_mavlink_telemetry_bytes(std::string(1, static_cast<char>(0xFE)));
    assert(malformed.malformed_frames == 1);
    assert(malformed.frames_seen == 0);

    return 0;
}
