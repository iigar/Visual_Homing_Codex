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
    assert(summary.latest.relative_altitude_m == 42.5);
    assert(vh::to_string(summary.latest.mode) == "Guided");

    const auto malformed = vh::inspect_mavlink_telemetry_bytes(std::string(1, static_cast<char>(0xFE)));
    assert(malformed.malformed_frames == 1);
    assert(malformed.frames_seen == 0);

    return 0;
}
