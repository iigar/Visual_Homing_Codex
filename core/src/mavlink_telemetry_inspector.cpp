#include "visual_homing/mavlink_telemetry_inspector.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace vh {
namespace {

constexpr unsigned char mavlink1_stx = 0xFE;
constexpr unsigned char mavlink2_stx = 0xFD;
constexpr std::uint32_t msg_heartbeat = 0;
constexpr std::uint32_t msg_attitude = 30;
constexpr std::uint32_t msg_global_position_int = 33;
constexpr std::uint32_t msg_optical_flow = 100;
constexpr std::uint32_t msg_optical_flow_rad = 106;
constexpr std::uint32_t msg_distance_sensor = 132;
constexpr std::uint32_t msg_altitude = 141;
constexpr std::uint8_t mav_mode_flag_safety_armed = 128;

struct InspectionAccumulation {
    double relative_altitude_sum_m = 0.0;
    double distance_sensor_current_sum_m = 0.0;
};

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
                     MavlinkTelemetryInspectionSummary& summary,
                     InspectionAccumulation& accumulation) {
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
        if (payload_size < 16) {
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
        if (payload_size < 20) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.global_position_int_messages;
        ++summary.altitude_messages;
        summary.latest.relative_altitude_seen = true;
        summary.latest.relative_altitude_m = static_cast<double>(read_i32_le(payload + 16)) / 1000.0;
        ++summary.relative_altitude_samples;
        accumulation.relative_altitude_sum_m += summary.latest.relative_altitude_m;
        if (summary.relative_altitude_samples == 1) {
            summary.relative_altitude_min_m = summary.latest.relative_altitude_m;
            summary.relative_altitude_max_m = summary.latest.relative_altitude_m;
        } else {
            summary.relative_altitude_min_m = std::min(summary.relative_altitude_min_m, summary.latest.relative_altitude_m);
            summary.relative_altitude_max_m = std::max(summary.relative_altitude_max_m, summary.latest.relative_altitude_m);
        }
        return;
    }

    if (message_id == msg_optical_flow) {
        if (payload_size < 26) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.optical_flow_messages;
        summary.optical_flow_distance_seen = true;
        summary.optical_flow_distance_m = static_cast<double>(read_f32_le(payload + 24));
        if (payload_size >= 34) {
            summary.optical_flow_quality = payload[33];
        }
        return;
    }

    if (message_id == msg_optical_flow_rad) {
        if (payload_size < 44) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.optical_flow_rad_messages;
        summary.optical_flow_distance_seen = true;
        summary.optical_flow_distance_m = static_cast<double>(read_f32_le(payload + 40));
        summary.optical_flow_quality = payload[35];
        return;
    }

    if (message_id == msg_distance_sensor) {
        if (payload_size < 10) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.distance_sensor_messages;
        summary.distance_sensor_seen = true;
        summary.distance_sensor_min_m =
            static_cast<double>(payload[4] | (static_cast<std::uint16_t>(payload[5]) << 8)) / 100.0;
        summary.distance_sensor_max_m =
            static_cast<double>(payload[6] | (static_cast<std::uint16_t>(payload[7]) << 8)) / 100.0;
        summary.distance_sensor_current_m =
            static_cast<double>(payload[8] | (static_cast<std::uint16_t>(payload[9]) << 8)) / 100.0;
        accumulation.distance_sensor_current_sum_m += summary.distance_sensor_current_m;
        if (summary.distance_sensor_messages == 1) {
            summary.distance_sensor_current_min_m = summary.distance_sensor_current_m;
            summary.distance_sensor_current_max_m = summary.distance_sensor_current_m;
        } else {
            summary.distance_sensor_current_min_m =
                std::min(summary.distance_sensor_current_min_m, summary.distance_sensor_current_m);
            summary.distance_sensor_current_max_m =
                std::max(summary.distance_sensor_current_max_m, summary.distance_sensor_current_m);
        }
        if (payload_size >= 14) {
            summary.distance_sensor_type = payload[10];
            summary.distance_sensor_id = payload[11];
            summary.distance_sensor_orientation = payload[12];
        }
        return;
    }

    if (message_id == msg_altitude) {
        if (payload_size < 24) {
            ++summary.malformed_frames;
            return;
        }
        ++summary.altitude_messages;
        summary.latest.relative_altitude_seen = true;
        summary.latest.relative_altitude_m = static_cast<double>(read_f32_le(payload + 20));
        ++summary.relative_altitude_samples;
        accumulation.relative_altitude_sum_m += summary.latest.relative_altitude_m;
        if (summary.relative_altitude_samples == 1) {
            summary.relative_altitude_min_m = summary.latest.relative_altitude_m;
            summary.relative_altitude_max_m = summary.latest.relative_altitude_m;
        } else {
            summary.relative_altitude_min_m = std::min(summary.relative_altitude_min_m, summary.latest.relative_altitude_m);
            summary.relative_altitude_max_m = std::max(summary.relative_altitude_max_m, summary.latest.relative_altitude_m);
        }
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

std::string format_mavlink_message_id_counts(const std::map<std::uint32_t, std::uint64_t>& counts) {
    if (counts.empty()) {
        return "none";
    }
    std::ostringstream output;
    bool first = true;
    for (const auto& [message_id, count] : counts) {
        if (!first) {
            output << ",";
        }
        first = false;
        output << message_id << ":" << count;
    }
    return output.str();
}

MavlinkTelemetryInspectionSummary inspect_mavlink_telemetry_bytes(const std::string& bytes) {
    MavlinkTelemetryInspectionSummary summary;
    summary.bytes_read = bytes.size();
    InspectionAccumulation accumulation;

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
            ++summary.message_id_counts[message_id];
            inspect_payload(message_id, payload, payload_size, summary, accumulation);
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
        ++summary.message_id_counts[message_id];
        inspect_payload(message_id, payload, payload_size, summary, accumulation);
        offset += frame_size;
    }

    if (summary.relative_altitude_samples > 0) {
        summary.relative_altitude_avg_m =
            accumulation.relative_altitude_sum_m / static_cast<double>(summary.relative_altitude_samples);
    }
    if (summary.distance_sensor_messages > 0) {
        summary.distance_sensor_current_avg_m =
            accumulation.distance_sensor_current_sum_m / static_cast<double>(summary.distance_sensor_messages);
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
