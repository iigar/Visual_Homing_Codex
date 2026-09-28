#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/mavlink_telemetry_adapter.hpp"
#include "mavlink_test_packets.hpp"

namespace {
using namespace std::chrono_literals;
vh::Timestamp at(int ms) { return vh::Timestamp{} + std::chrono::milliseconds(ms); }

void u32(std::string& payload, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) payload.at(offset + i) = static_cast<char>(value >> (8 * i));
}

// Valid CRCs, with an optional deliberately unauthenticated signature suffix.
std::string packet(unsigned id, const std::string& payload, bool v2 = true, bool signed_frame = false) {
    std::string bytes(v2 ? 10 : 6, '\0');
    bytes[0] = static_cast<char>(v2 ? 0xFD : 0xFE);
    bytes[1] = static_cast<char>(payload.size());
    bytes[v2 ? 5 : 3] = 1;
    bytes[v2 ? 6 : 4] = 1;
    bytes[v2 ? 7 : 5] = static_cast<char>(id);
    if (v2 && signed_frame) bytes[2] = 1;
    bytes += payload;
    mavlink_test::finish_crc(bytes, mavlink_test::crc_extra(id));
    if (v2 && signed_frame) bytes += std::string(13, '\0');
    return bytes;
}

std::string heartbeat(bool v2 = false) {
    std::string payload(9, '\0');
    u32(payload, 0, 4); // Guided, armed.
    payload[6] = static_cast<char>(128);
    return packet(0, payload, v2);
}
std::string attitude(bool v2 = true) { return packet(30, std::string(28, '\0'), v2); }
std::string position(int altitude_mm, bool v2 = true, bool signed_frame = false) {
    std::string payload(28, '\0');
    u32(payload, 16, static_cast<std::uint32_t>(altitude_mm));
    return packet(33, payload, v2, signed_frame);
}
std::string altitude(float value) {
    std::string payload(32, '\0');
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    u32(payload, 20, bits);
    return packet(141, payload);
}
std::string fixture(int altitude_mm = 42500) {
    return "noise" + heartbeat() + attitude() + position(altitude_mm);
}
void append(vh::MavlinkTelemetryByteBuffer& buffer, const std::string& bytes, int ms) {
    buffer.append(bytes.data(), bytes.size(), at(ms));
}
vh::MavlinkTelemetryStreamSnapshot snapshot(const vh::MavlinkTelemetryByteBuffer& buffer) {
    vh::MavlinkTelemetryStreamSnapshot result;
    result.bytes_captured = buffer.bytes_captured();
    result.bytes_retained = buffer.bytes_retained();
    result.bytes_dropped = buffer.bytes_dropped();
    result.inspection = buffer.inspection();
    result.receipts = buffer.receipts();
    return result;
}
vh::LiveRouteMatchTelemetryObservation observe(const vh::MavlinkTelemetryByteBuffer& buffer, int ms) {
    return vh::live_route_match_telemetry_observation(snapshot(buffer), {}, at(ms));
}
bool ready(const vh::LiveRouteMatchTelemetryObservation& observation, vh::Timestamp evaluated_at) {
    vh::MavlinkTelemetryAdapter adapter({.max_telemetry_age_ms = 500.0});
    if (observation.valid) adapter.observe(observation.telemetry, observation.telemetry.timestamp);
    return observation.valid && adapter.mavlink_ok(evaluated_at);
}
} // namespace

int main() {
    const auto bytes = fixture();
    assert(bytes.size() == 102);
    vh::MavlinkTelemetryByteBuffer buffer(bytes.size());
    append(buffer, bytes, 1000);
    auto observation = observe(buffer, 1000);
    assert(observation.valid && observation.altitude.valid && observation.altitude.value == 42.5);
    assert(observation.telemetry.timestamp == at(1000) && observation.altitude.timestamp == at(1000));
    assert(ready(observation, at(1500)) && !ready(observation, at(1500) + 1ns));

    // A retained old snapshot cannot turn expired adapter health back on.
    observation = observe(buffer, 10000);
    assert(observation.valid && !ready(observation, at(10000)));
    assert(observation.telemetry.timestamp == at(1000));
    buffer.append(nullptr, 0, at(10000));
    assert(buffer.receipts().relative_altitude.received_at == at(1000));

    // Same window count, new sample; repeated values/sequence numbers are also new receipts.
    const auto old_id = buffer.receipts().relative_altitude.end_offset;
    append(buffer, fixture(1000), 10010);
    observation = observe(buffer, 10010);
    assert(buffer.inspection().relative_altitude_samples == 1);
    assert(buffer.receipts().relative_altitude.end_offset > old_id);
    assert(observation.altitude.value == 1.0 && observation.altitude.timestamp == at(10010));
    assert(ready(observation, at(10010)));
    const auto next_id = buffer.receipts().relative_altitude.end_offset;
    append(buffer, fixture(1000), 10020);
    assert(buffer.receipts().relative_altitude.end_offset > next_id);
    assert(observe(buffer, 10020).altitude.timestamp == at(10020));
    // Positive append large enough to replace the complete retained tail.
    append(buffer, "discarded-prefix" + fixture(2000), 10030);
    assert(buffer.bytes() == fixture(2000));
    assert(observe(buffer, 10030).altitude.value == 2.0);
    assert(buffer.receipts().relative_altitude.end_offset == buffer.bytes_captured());

    // New noise, irrelevant messages, or only one fresh component cannot refresh the others.
    vh::MavlinkTelemetryByteBuffer mixed(4096);
    append(mixed, bytes, 1000);
    append(mixed, "noise" + packet(200, "payload"), 10000);
    assert(observe(mixed, 10000).telemetry.timestamp == at(1000));
    append(mixed, position(3000), 10010);
    observation = observe(mixed, 10010);
    assert(observation.altitude.value == 3.0 && observation.altitude.timestamp == at(10010));
    assert(!ready(observation, at(10010)) && observation.telemetry.timestamp == at(1000));
    append(mixed, heartbeat(), 10020);
    assert(!ready(observe(mixed, 10020), at(10020))); // Attitude is still old.
    append(mixed, attitude(), 10030);
    assert(ready(observe(mixed, 10030), at(10030)));
    assert(observe(mixed, 10030).telemetry.timestamp == at(10010));

    // Every possible split of a frame, including header, checksum and v2 signature.
    for (const auto& message : {position(6000, false), position(6000), position(6000, true, true)}) {
        for (std::size_t split = 1; split < message.size(); ++split) {
            vh::MavlinkTelemetryByteBuffer fragmented(4096);
            append(fragmented, bytes, 1000);
            append(fragmented, message.substr(0, split), 1100);
            assert(fragmented.receipts().relative_altitude.received_at == at(1000));
            assert(!observe(fragmented, 1100).valid); // Existing strict malformed-tail policy.
            append(fragmented, message.substr(split), 1200);
            const auto complete = observe(fragmented, 1200);
            if (static_cast<unsigned char>(message[0]) == 0xFD && message[2] == 1) {
                assert(!complete.valid);
                assert(fragmented.inspection().unsupported_signed_frames == 1);
                assert(fragmented.receipts().relative_altitude.received_at == at(1000));
                continue;
            }
            assert(complete.valid && complete.altitude.value == 6.0);
            assert(complete.altitude.timestamp == at(1200));
            assert(complete.telemetry.timestamp == at(1000));
        }
    }

    // Byte-at-a-time reception and cache equivalence to the existing full inspector.
    vh::MavlinkTelemetryByteBuffer fragmented(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        fragmented.append(bytes.data() + i, 1, at(1000 + static_cast<int>(i)));
        const auto expected = vh::inspect_mavlink_telemetry_bytes(fragmented.bytes());
        assert(fragmented.inspection().frames_seen == expected.frames_seen);
        assert(fragmented.inspection().malformed_frames == expected.malformed_frames);
        assert(fragmented.inspection().message_id_counts == expected.message_id_counts);
    }
    assert(fragmented.receipts().heartbeat.received_at == at(1021));
    assert(fragmented.receipts().attitude.received_at == at(1061));
    assert(fragmented.receipts().relative_altitude.received_at == at(1101));

    // Rollover with only part of the old tail retained; absolute identity stays stable.
    vh::MavlinkTelemetryByteBuffer rolling(2 * bytes.size());
    append(rolling, bytes, 1000);
    append(rolling, bytes, 1100);
    const auto retained_id = rolling.receipts().relative_altitude.end_offset;
    append(rolling, "noise", 1200);
    assert(rolling.receipts().relative_altitude.end_offset == retained_id);
    assert(rolling.receipts().relative_altitude.received_at == at(1100));
    append(rolling, fixture(9000), 1300);
    assert(observe(rolling, 1300).altitude.value == 9.0);
    assert(observe(rolling, 1300).altitude.timestamp == at(1300));
    append(rolling, std::string(204, 'x'), 1400);
    assert(!observe(rolling, 1400).valid && !rolling.receipts().heartbeat.received_at);
    append(rolling, bytes, 1500);
    assert(ready(observe(rolling, 1500), at(1500)));

    // ALTITUDE replaces GLOBAL_POSITION_INT only with the existing explicit config.
    vh::MavlinkTelemetryByteBuffer alternate(4096);
    append(alternate, heartbeat() + attitude() + altitude(7.5F), 1000);
    assert(!observe(alternate, 1000).valid);
    vh::MavlinkTelemetryValidationConfig altitude_only;
    altitude_only.minimum_global_position_int_messages = 0;
    auto alternate_observation = vh::live_route_match_telemetry_observation(snapshot(alternate), altitude_only, at(1000));
    assert(alternate_observation.valid && alternate_observation.altitude.value == 7.5);
    append(alternate, altitude(std::numeric_limits<float>::quiet_NaN()), 1100);
    alternate_observation = vh::live_route_match_telemetry_observation(snapshot(alternate), altitude_only, at(1100));
    assert(alternate_observation.valid && !alternate_observation.altitude.valid);

    // No receive metadata, zero/future receive time, and malformed payloads fail closed.
    auto missing = snapshot(buffer);
    missing.receipts.attitude.received_at.reset();
    assert(!vh::live_route_match_telemetry_observation(missing, {}, at(11000)).valid);
    missing = snapshot(buffer);
    missing.receipts.heartbeat.received_at = vh::Timestamp{};
    assert(!vh::live_route_match_telemetry_observation(missing, {}, at(11000)).valid);
    missing = snapshot(buffer);
    missing.receipts.attitude.received_at = at(11001);
    assert(!vh::live_route_match_telemetry_observation(missing, {}, at(11000)).valid);
    append(mixed, packet(33, std::string(19, '\0'), false), 11000); // v1 cannot truncate.
    assert(!observe(mixed, 11000).valid);
    assert(mixed.receipts().relative_altitude.received_at == at(10010));

    // Corrupt packets cannot replace values or receive times. A relaxed malformed
    // threshold can retain old evidence, but cannot make it fresh in the adapter.
    vh::MavlinkTelemetryByteBuffer integrity(bytes.size() * 4);
    append(integrity, bytes, 1000);
    for (auto corrupt : {heartbeat(), attitude(), position(9000)}) {
        corrupt[corrupt.size() - 1] ^= 1;
        append(integrity, corrupt, 10000);
        assert(!observe(integrity, 10000).valid);
        assert(integrity.receipts().heartbeat.received_at == at(1000));
        assert(integrity.receipts().attitude.received_at == at(1000));
        assert(integrity.receipts().relative_altitude.received_at == at(1000));
        assert(integrity.inspection().latest.relative_altitude_m == 42.5);
    }
    vh::MavlinkTelemetryValidationConfig tolerate;
    tolerate.maximum_malformed_frames = 3;
    const auto retained = vh::live_route_match_telemetry_observation(snapshot(integrity), tolerate, at(10000));
    assert(retained.valid && retained.telemetry.timestamp == at(1000) && !ready(retained, at(10000)));
    append(integrity, std::string(bytes.size() * 4, 'x') + fixture(9000), 10010);
    assert(ready(observe(integrity, 10010), at(10010)));
    assert(observe(integrity, 10010).altitude.value == 9.0);

    buffer.clear();
    assert(!observe(buffer, 12000).valid);
    assert(buffer.inspection().bytes_read == 0 && buffer.receipts().relative_altitude.end_offset == 0);
    append(buffer, fixture(-1000), 12010);
    assert(observe(buffer, 12010).valid && !observe(buffer, 12010).altitude.valid);
}
