#include <cassert>
#include <iostream>
#include <string>

#include "visual_homing/mavlink_telemetry_inspector.hpp"
#include "mavlink_telemetry_golden.hpp"
#include "mavlink_test_packets.hpp"

namespace {
using Summary = vh::MavlinkTelemetryInspectionSummary;

std::uint64_t decoded(const Summary& s) {
    return s.heartbeat_messages + s.attitude_messages + s.altitude_messages +
           s.optical_flow_messages + s.optical_flow_rad_messages + s.distance_sensor_messages;
}

void values(const Summary& s, unsigned id, bool zero) {
    assert(s.malformed_frames == 0 && s.checksum_errors == 0 && decoded(s) == 1);
    switch (id) {
    case 0:
        assert(s.heartbeat_messages == 1 && s.latest.heartbeat_seen);
        assert(s.heartbeat_custom_mode == (zero ? 0 : 4));
        assert(s.latest.armed == !zero);
        assert(s.latest.mode == vh::FlightMode::Unknown);
        assert(s.heartbeat_type == (zero ? 0 : 2) && s.heartbeat_autopilot == (zero ? 0 : 3));
        break;
    case 30:
        assert(s.attitude_messages == 1);
        assert(s.latest.roll_rad == (zero ? 0 : 0.25));
        assert(s.latest.pitch_rad == (zero ? 0 : -0.5));
        assert(s.latest.yaw_rad == (zero ? 0 : 1.5));
        break;
    case 33:
    case 141:
        assert(s.latest.relative_altitude_seen && s.relative_altitude_samples == 1);
        assert(s.global_position_int_messages == (id == 33 ? 1U : 0U));
        assert(s.latest.relative_altitude_m == (zero ? 0 : (id == 33 ? 42.5 : 3.25)));
        assert(s.relative_altitude_min_m == s.latest.relative_altitude_m);
        assert(s.relative_altitude_avg_m == s.latest.relative_altitude_m);
        assert(s.relative_altitude_max_m == s.latest.relative_altitude_m);
        break;
    case 100:
    case 106:
        assert(s.optical_flow_distance_seen);
        assert(s.optical_flow_distance_m == (zero ? 0 : (id == 100 ? 0.75 : 1.25)));
        assert(s.optical_flow_quality == (zero ? 0 : (id == 100 ? 220 : 219)));
        break;
    case 132:
        assert(s.distance_sensor_seen && s.distance_sensor_messages == 1);
        assert(s.distance_sensor_current_m == (zero ? 0 : 0.73));
        assert(s.distance_sensor_min_m == (zero ? 0 : 0.20));
        assert(s.distance_sensor_max_m == (zero ? 0 : 4.0));
        assert(s.distance_sensor_id == (zero ? 0 : 3));
        assert(s.distance_sensor_orientation == (zero ? 0 : 25));
        break;
    default: assert(false);
    }
}

std::string with_payload(const std::string& original, const std::string& payload, unsigned id) {
    const auto header = static_cast<unsigned char>(original[0]) == 0xFD ? 10U : 6U;
    auto frame = original.substr(0, header);
    frame[1] = static_cast<char>(payload.size());
    frame += payload;
    mavlink_test::finish_crc(frame, mavlink_test::crc_extra(id));
    return frame;
}
} // namespace

int main() {
    unsigned corruptions = 0;
    unsigned lengths = 0;
    unsigned prefixes = 0;
    for (const auto& p : mavlink_golden::packets) {
        const auto wire = mavlink_test::from_hex(p.hex);
        const auto header_size = p.v2 ? 10U : 6U;
        const auto payload_size = static_cast<unsigned char>(wire[1]);
        const auto checksum_offset = header_size + payload_size;
        // The test CRC implementation is checked against independently generated wire.
        auto recreated = wire.substr(0, checksum_offset);
        mavlink_test::finish_crc(recreated, mavlink_test::crc_extra(p.id));
        assert(recreated == wire.substr(0, checksum_offset + 2));
        const auto s = vh::inspect_mavlink_telemetry_bytes(wire, {42, 17});
        assert(s.frames_seen == 1 && s.message_id_counts.at(p.id) == 1);
        if (p.signed_frame) {
            assert(s.unsupported_signed_frames == 1 && s.malformed_frames == 1 && decoded(s) == 0);
            assert(s.heartbeat_end_offset == 0 && s.attitude_end_offset == 0 && s.relative_altitude_end_offset == 0);
        } else {
            values(s, p.id, p.zero);
            assert(s.heartbeat_end_offset == (p.id == 0 ? wire.size() : 0));
            assert(s.attitude_end_offset == (p.id == 30 ? wire.size() : 0));
            assert(s.relative_altitude_end_offset == (p.id == 33 || p.id == 141 ? wire.size() : 0));

            // Every single-bit error in sequence/source, compatible flags, payload
            // and checksum must be rejected before any semantic fields are updated.
            for (std::size_t index = 2; index < wire.size(); ++index) {
                if ((p.v2 && (index == 2 || (index >= 7 && index <= 9))) || (!p.v2 && index == 5)) continue;
                for (unsigned bit = 0; bit < 8; ++bit) {
                    auto damaged = wire;
                    damaged[index] ^= static_cast<char>(1U << bit);
                    const auto bad = vh::inspect_mavlink_telemetry_bytes(damaged, {42, 17});
                    assert(bad.checksum_errors == 1 && bad.malformed_frames == 1 && decoded(bad) == 0);
                    assert(bad.heartbeat_end_offset == 0 && bad.attitude_end_offset == 0 && bad.relative_altitude_end_offset == 0);
                    ++corruptions;
                }
            }
            if (!p.zero) {
                // All byte-sized lengths. v2 may have future extensions; v1 is fixed.
                for (unsigned length = 0; length <= 255; ++length) {
                    const auto resized = with_payload(wire, std::string(length, '\0'), p.id);
                    const auto result = vh::inspect_mavlink_telemetry_bytes(resized, {42, 17});
                    const bool accepted = p.v2 ? length != 0 : length == payload_size;
                    assert(decoded(result) == (accepted ? 1U : 0U));
                    assert(result.malformed_frames == (accepted ? 0U : 1U));
                    if (accepted) values(result, p.id, true);
                    ++lengths;
                }
                if (p.v2) {
                    auto extended_payload = wire.substr(header_size, payload_size);
                    // Restore known omitted zeros before appending unknown extension bytes.
                    const auto full_size = p.id == 100 ? 34U : p.id == 132 ? 39U :
                                           p.id == 0 ? 9U : p.id == 106 ? 44U : p.id == 141 ? 32U : 28U;
                    extended_payload.resize(full_size, '\0');
                    extended_payload.resize(255, static_cast<char>(0xA5));
                    values(vh::inspect_mavlink_telemetry_bytes(with_payload(wire, extended_payload, p.id), {42, 17}), p.id, false);
                    auto compatible = wire.substr(0, checksum_offset);
                    compatible[3] = static_cast<char>(0xff);
                    mavlink_test::finish_crc(compatible, mavlink_test::crc_extra(p.id));
                    values(vh::inspect_mavlink_telemetry_bytes(compatible, {42, 17}), p.id, false);
                    for (unsigned bit = 1; bit < 8; ++bit) {
                        auto unsupported = wire.substr(0, checksum_offset);
                        unsupported[2] = static_cast<char>(1U << bit);
                        mavlink_test::finish_crc(unsupported, mavlink_test::crc_extra(p.id));
                        const auto rejected = vh::inspect_mavlink_telemetry_bytes(unsupported, {42, 17});
                        assert(rejected.unsupported_incompatibility_frames == 1 && rejected.malformed_frames == 1);
                        assert(decoded(rejected) == 0);
                    }
                }
            }
        }
        // Every incomplete prefix, including CRC and signature, supplies no evidence.
        for (std::size_t split = 1; split < wire.size(); ++split) {
            const auto partial = vh::inspect_mavlink_telemetry_bytes(wire.substr(0, split), {42, 17});
            assert(partial.malformed_frames == 1 && decoded(partial) == 0);
            ++prefixes;
        }
    }

    // Unsupported 24-bit ID is counted, skipped and cannot impersonate low-byte ID 0.
    auto unknown = mavlink_test::from_hex(mavlink_golden::packets[14].hex);
    unknown[8] = 1;
    unknown[9] = 2;
    const auto u = vh::inspect_mavlink_telemetry_bytes(unknown, {42, 17});
    assert(u.message_id_counts.at(0x020100) == 1 && u.unsupported_message_frames == 1);
    assert(decoded(u) == 0 && !vh::validate_mavlink_telemetry(u, {}).passed);

    // Bad known frame cannot update the latest valid value or offset, even if
    // callers explicitly tolerate some malformed packets in a retained window.
    const auto hb = mavlink_test::from_hex(mavlink_golden::packets[0].hex);
    const auto att = mavlink_test::from_hex(mavlink_golden::packets[2].hex);
    const auto pos = mavlink_test::from_hex(mavlink_golden::packets[4].hex);
    auto damaged = pos;
    damaged[22] ^= 1;
    const auto retained = vh::inspect_mavlink_telemetry_bytes("noise" + hb + att + pos + damaged + unknown, {42, 17});
    assert(retained.relative_altitude_samples == 1 && retained.latest.relative_altitude_m == 42.5);
    assert(retained.relative_altitude_end_offset == 5 + hb.size() + att.size() + pos.size());
    assert(!vh::validate_mavlink_telemetry(retained, {.expected_source = {42, 17}}).passed);
    vh::MavlinkTelemetryValidationConfig tolerate{.expected_source = {42, 17}};
    tolerate.maximum_malformed_frames = 1;
    assert(vh::validate_mavlink_telemetry(retained, tolerate).passed);
    const auto recovery = vh::inspect_mavlink_telemetry_bytes(damaged + hb + att + pos, {42, 17});
    assert(recovery.latest.relative_altitude_m == 42.5 && recovery.relative_altitude_end_offset == damaged.size() + hb.size() + att.size() + pos.size());
    std::cout << "golden=35 single_bit_corruptions=" << corruptions << " payload_lengths=" << lengths
              << " incomplete_prefixes=" << prefixes << '\n';
}
