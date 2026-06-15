#include "visual_homing/mavlink_telemetry_inspector.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace vh {
namespace {

constexpr unsigned char mavlink1_stx = 0xFE;
constexpr unsigned char mavlink2_stx = 0xFD;
constexpr std::uint32_t msg_heartbeat = 0;
constexpr std::uint32_t msg_attitude = 30;
constexpr std::uint32_t msg_global_position_int = 33;
constexpr std::uint32_t msg_altitude = 141;
constexpr std::uint8_t mav_mode_flag_safety_armed = 128;

std::uint32_t read_u32_le(const unsigned char* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::int32_t read_i32_le(const unsigned char* data) {
    return static_cast<std::int32_t>(read_u32_le(data));
}

float read_f32_le(const unsigned char* data) {
    const auto value = read_u32_le(data);
    float output = 0.0F;
    std::memcpy(&output, &value, sizeof(output));
    return output;
}

FlightMode ardupilot_custom_mode_to_flight_mode(std::uint32_t custom_mode) {
    switch (custom_mode) {
    case 0:
        return FlightMode::Stabilize;
    case 2:
        return FlightMode::AltHold;
    case 3:
        return FlightMode::Auto;
    case 4:
        return FlightMode::Guided;
    case 6:
        return FlightMode::Rtl;
    case 9:
        return FlightMode::Land;
    default:
        return FlightMode::Unknown;
    }
}

void inspect_payload(std::uint32_t message_id,
                     const unsigned char* payload,
                     std::size_t payload_size,
                     MavlinkTelemetryInspectionSummary& summary) {
    if (message_id == msg_heartbeat) {
        if (payload_size < 9) {
            ++summary.malformed_frames;
            return;
        }
        const auto custom_mode = read_u32_le(payload);
        const auto base_mode = payload[6];
        ++summary.heartbeat_messages;
        summary.heartbeat_custom_mode = custom_mode;
        summary.heartbeat_type = payload[4];
        summary.heartbeat_autopilot = payload[5];
        summary.heartbeat_base_mode = base_mode;
        summary.heartbeat_system_status = payload[7];
        summary.heartbeat_mavlink_version = payload[8];
        summary.latest.heartbeat_seen = true;
        summary.latest.armed = (base_mode & mav_mode_flag_safety_armed) != 0;
        summary.latest.mode = ardupilot_custom_mode_to_flight_mode(custom_mode);
        return;
    }

    if (message_id == msg_attitude) {
        if (payload_size < 28) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.attitude_messages;
        summary.latest.roll_rad = static_cast<double>(read_f32_le(payload + 4));
        summary.latest.pitch_rad = static_cast<double>(read_f32_le(payload + 8));
        summary.latest.yaw_rad = static_cast<double>(read_f32_le(payload + 12));
        return;
    }

    if (message_id == msg_global_position_int) {
        if (payload_size < 28) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.global_position_int_messages;
        ++summary.altitude_messages;
        summary.latest.relative_altitude_seen = true;
        summary.latest.relative_altitude_m = static_cast<double>(read_i32_le(payload + 16)) / 1000.0;
        return;
    }

    if (message_id == msg_altitude) {
        if (payload_size < 32) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.altitude_messages;
        summary.latest.relative_altitude_seen = true;
        summary.latest.relative_altitude_m = static_cast<double>(read_f32_le(payload + 20));
    }
}

} // namespace

std::string to_string(FlightMode mode) {
    switch (mode) {
    case FlightMode::Unknown:
        return "Unknown";
    case FlightMode::Manual:
        return "Manual";
    case FlightMode::Stabilize:
        return "Stabilize";
    case FlightMode::AltHold:
        return "AltHold";
    case FlightMode::Guided:
        return "Guided";
    case FlightMode::Auto:
        return "Auto";
    case FlightMode::Rtl:
        return "Rtl";
    case FlightMode::Land:
        return "Land";
    }

    return "Unknown";
}

MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_bytes(const std::string& bytes) {
    MavlinkTelemetryInspectionSummary summary;
    summary.bytes_read = bytes.size();

    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto stx = static_cast<unsigned char>(bytes[offset]);
        if (stx != mavlink1_stx && stx != mavlink2_stx) {
            ++offset;
            continue;
        }

        if (stx == mavlink1_stx) {
            constexpr std::size_t header_size = 6;
            constexpr std::size_t checksum_size = 2;
            if (offset + header_size > bytes.size()) {
                ++summary.malformed_frames;
                break;
            }
            const auto payload_size = static_cast<std::size_t>(static_cast<unsigned char>(bytes[offset + 1]));
            const auto frame_size = header_size + payload_size + checksum_size;
            if (offset + frame_size > bytes.size()) {
                ++summary.malformed_frames;
                break;
            }
            const auto message_id = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 5]));
            const auto* payload = reinterpret_cast<const unsigned char*>(bytes.data() + offset + header_size);
            ++summary.frames_seen;
            ++summary.mavlink1_frames;
            inspect_payload(message_id, payload, payload_size, summary);
            offset += frame_size;
            continue;
        }

        constexpr std::size_t header_size = 10;
        constexpr std::size_t checksum_size = 2;
        constexpr std::size_t signature_size = 13;
        if (offset + header_size > bytes.size()) {
            ++summary.malformed_frames;
            break;
        }
        const auto payload_size = static_cast<std::size_t>(static_cast<unsigned char>(bytes[offset + 1]));
        const auto incompat_flags = static_cast<unsigned char>(bytes[offset + 2]);
        const bool signed_frame = (incompat_flags & 0x01) != 0;
        const auto frame_size = header_size + payload_size + checksum_size + (signed_frame ? signature_size : 0);
        if (offset + frame_size > bytes.size()) {
            ++summary.malformed_frames;
            break;
        }
        const auto message_id =
            static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 7])) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 8])) << 8) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 9])) << 16);
        const auto* payload = reinterpret_cast<const unsigned char*>(bytes.data() + offset + header_size);
        ++summary.frames_seen;
        ++summary.mavlink2_frames;
        inspect_payload(message_id, payload, payload_size, summary);
        offset += frame_size;
    }

    return summary;
}

MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open MAVLink telemetry file for read: " + path);
    }

    const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return inspect_mavlink_telemetry_bytes(bytes);
}

MavlinkTelemetryValidationResult validate_mavlink_telemetry(
    const MavlinkTelemetryInspectionSummary& summary,
    const MavlinkTelemetryValidationConfig& config) {
    MavlinkTelemetryValidationResult result;
    result.heartbeat_passed = summary.heartbeat_messages >= config.minimum_heartbeat_messages;
    result.attitude_passed = summary.attitude_messages >= config.minimum_attitude_messages;
    result.global_position_int_passed =
        summary.global_position_int_messages >= config.minimum_global_position_int_messages;
    result.altitude_passed = summary.altitude_messages > 0 || config.minimum_global_position_int_messages == 0;
    result.malformed_passed = summary.malformed_frames <= config.maximum_malformed_frames;
    result.passed = result.heartbeat_passed &&
                    result.attitude_passed &&
                    result.global_position_int_passed &&
                    result.altitude_passed &&
                    result.malformed_passed;
    return result;
}

} // namespace vh
