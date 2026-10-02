#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/mavlink_telemetry_adapter.hpp"
#include "mavlink_telemetry_golden.hpp"
#include "mavlink_test_packets.hpp"

namespace {
constexpr vh::MavlinkTelemetrySourceId source{42, 17};
constexpr vh::MavlinkTelemetrySourceId other_system{43, 17};
constexpr vh::MavlinkTelemetrySourceId other_component{42, 18};
const vh::MavlinkTelemetryValidationConfig config{.expected_source = source};
vh::Timestamp at(int ms) { return vh::Timestamp{} + std::chrono::milliseconds(ms); }

std::string packet(unsigned id, vh::MavlinkTelemetrySourceId sender = source, bool v2 = false,
                   unsigned type = 2, unsigned autopilot = 3, unsigned mode = 4,
                   unsigned base = 129, unsigned version = 3) {
    for (const auto& p : mavlink_golden::packets) {
        if (p.id != id || p.v2 != v2 || p.zero || p.signed_frame) continue;
        auto bytes = mavlink_test::from_hex(p.hex);
        bytes.resize(bytes.size() - 2);
        bytes[v2 ? 5 : 3] = static_cast<char>(sender.system_id);
        bytes[v2 ? 6 : 4] = static_cast<char>(sender.component_id);
        if (id == 0) {
            const auto h = v2 ? 10 : 6;
            for (unsigned i = 0; i < 4; ++i) bytes[h + i] = static_cast<char>(mode >> (8 * i));
            bytes[h + 4] = static_cast<char>(type);
            bytes[h + 5] = static_cast<char>(autopilot);
            bytes[h + 6] = static_cast<char>(base);
            bytes[h + 8] = static_cast<char>(version);
        }
        mavlink_test::finish_crc(bytes, mavlink_test::crc_extra(id));
        return bytes;
    }
    throw std::logic_error("Missing golden packet");
}

std::string fixture(vh::MavlinkTelemetrySourceId sender = source, bool v2 = false) {
    return packet(0, sender, v2) + packet(30, sender, v2) + packet(33, sender, v2);
}
vh::MavlinkTelemetryInspectionSummary inspect(const std::string& bytes) {
    return vh::inspect_mavlink_telemetry_bytes(bytes, source);
}
bool valid(const vh::MavlinkTelemetryInspectionSummary& summary) {
    return vh::validate_mavlink_telemetry(summary, config).passed;
}
void append(vh::MavlinkTelemetryByteBuffer& buffer, const std::string& bytes, int ms) {
    buffer.append(bytes.data(), bytes.size(), at(ms));
}
vh::LiveRouteMatchTelemetryObservation observe(const vh::MavlinkTelemetryByteBuffer& buffer, int ms) {
    vh::MavlinkTelemetryStreamSnapshot snapshot;
    snapshot.inspection = buffer.inspection();
    snapshot.receipts = buffer.receipts();
    return vh::live_route_match_telemetry_observation(snapshot, config, at(ms));
}
bool permission(const vh::LiveRouteMatchTelemetryObservation& observation, int ms) {
    vh::MavlinkTelemetryAdapter adapter({.max_telemetry_age_ms = 500});
    if (observation.valid) adapter.observe(observation.telemetry, observation.telemetry.timestamp);
    return adapter.command_permission_ok(at(ms));
}
} // namespace

int main() {
    for (bool v2 : {false, true}) {
        const auto bytes = fixture(source, v2);
        const auto selected = inspect(bytes);
        assert(valid(selected) && selected.latest.mode == vh::FlightMode::Guided);
        assert(selected.selected_source_frames == 3 && selected.unselected_source_frames == 0);
        assert(!vh::validate_mavlink_telemetry(selected, {}).passed);
        assert(!vh::validate_mavlink_telemetry(selected, {.expected_source = other_system}).passed);
        const auto unset = vh::inspect_mavlink_telemetry_bytes(bytes);
        assert(unset.frames_seen == 3 && unset.unselected_source_frames == 3);
        assert(unset.heartbeat_messages == 0 && unset.heartbeat_end_offset == 0);
        assert(!valid(unset) && unset.latest.mode == vh::FlightMode::Unknown);
        assert(!vh::validate_mavlink_telemetry(unset, {0, 0, 0, 999, source}).passed);

        for (const auto wrong : {other_system, other_component, vh::MavlinkTelemetrySourceId{0, 17},
                                vh::MavlinkTelemetrySourceId{42, 0}, vh::MavlinkTelemetrySourceId{0, 0}}) {
            assert(!valid(inspect(fixture(wrong, v2))));
            for (unsigned missing : {0U, 30U, 33U}) {
                const auto mixed = inspect(packet(0, missing == 0 ? wrong : source, v2)
                    + packet(30, missing == 30 ? wrong : source, v2)
                    + packet(33, missing == 33 ? wrong : source, v2));
                assert(!valid(mixed) && mixed.selected_source_frames == 2);
                assert(mixed.invalid_source_frames == (wrong.configured() ? 0U : 1U));
                assert(mixed.unselected_source_frames == (wrong.configured() ? 1U : 0U));
            }
        }
        // Foreign packets cannot replace any selected field, including auxiliary sensors.
        for (unsigned id : {0U, 30U, 33U, 100U, 106U, 132U, 141U}) {
            const auto foreign = inspect(packet(id, other_component, v2));
            assert(foreign.selected_source_frames == 0 && foreign.unselected_source_frames == 1);
            assert(!foreign.latest.heartbeat_seen && !foreign.latest.relative_altitude_seen);
            assert(!foreign.distance_sensor_seen && !foreign.optical_flow_distance_seen);
        }
        const auto surrounded = inspect(fixture(other_system, v2) + bytes + fixture(other_component, v2));
        assert(valid(surrounded) && surrounded.selected_source_frames == 3 && surrounded.unselected_source_frames == 6);
        assert(surrounded.relative_altitude_end_offset == fixture(other_system, v2).size() + bytes.size());
        for (auto boundary : {vh::MavlinkTelemetrySourceId{1, 1}, vh::MavlinkTelemetrySourceId{255, 255}}) {
            assert(vh::validate_mavlink_telemetry(vh::inspect_mavlink_telemetry_bytes(fixture(boundary, v2), boundary),
                                               {.expected_source = boundary}).passed);
        }

        // Mode family and custom-mode flag are independent of transport framing.
        for (unsigned type : {2U, 3U, 4U, 13U, 14U, 15U}) {
            const auto s = inspect(packet(0, source, v2, type) + packet(30) + packet(33));
            assert(valid(s) && s.latest.mode == vh::FlightMode::Guided);
        }
        for (unsigned type : {0U, 1U, 6U, 10U, 12U, 18U, 19U, 20U, 255U}) {
            const auto s = inspect(bytes + packet(0, source, v2, type));
            assert(!valid(s) && !s.heartbeat_contract_passed && s.latest.mode == vh::FlightMode::Unknown);
        }
        for (unsigned autopilot : {0U, 8U, 12U, 255U}) {
            const auto s = inspect(bytes + packet(0, source, v2, 2, autopilot));
            assert(!valid(s) && s.latest.mode == vh::FlightMode::Unknown);
        }
        for (unsigned version : {0U, 1U, 2U, 255U}) {
            assert(!valid(inspect(bytes + packet(0, source, v2, 2, 3, 4, 129, version))));
        }
        for (unsigned base : {0U, 8U, 128U, 136U}) {
            const auto s = inspect(bytes + packet(0, source, v2, 2, 3, 4, base));
            assert(valid(s) && s.latest.mode == vh::FlightMode::Unknown);
        }
        for (auto [mode, expected] : {std::pair{0U, vh::FlightMode::Stabilize}, {2U, vh::FlightMode::AltHold},
             {3U, vh::FlightMode::Auto}, {4U, vh::FlightMode::Guided}, {6U, vh::FlightMode::Rtl},
             {9U, vh::FlightMode::Land}, {5U, vh::FlightMode::Unknown}, {20U, vh::FlightMode::Unknown},
             {0xffffffffU, vh::FlightMode::Unknown}}) {
            assert(inspect(packet(0, source, v2, 2, 3, mode)).latest.mode == expected);
        }
    }

    // Real bounded buffer -> runtime observation -> adapter. Foreign traffic never
    // refreshes receipt times; losing the selected producer fails after the same age.
    const auto bytes = fixture();
    vh::MavlinkTelemetryByteBuffer buffer(4096, source);
    append(buffer, bytes, 1000);
    assert(permission(observe(buffer, 1500), 1500));
    append(buffer, fixture(other_system, true), 1501);
    const auto stale = observe(buffer, 1501);
    assert(stale.valid && stale.telemetry.timestamp == at(1000) && !permission(stale, 1501));
    append(buffer, packet(0, source, true, 1, 12), 1502);
    assert(!observe(buffer, 1502).valid && !permission(observe(buffer, 1502), 1502));
    append(buffer, packet(0, source, true), 1503);
    assert(observe(buffer, 1503).valid && !permission(observe(buffer, 1503), 1503));
    append(buffer, fixture(source, true), 1600);
    assert(permission(observe(buffer, 1600), 1600));
    append(buffer, packet(0, source, true, 2, 3, 4, 128), 1601);
    assert(observe(buffer, 1601).valid && !permission(observe(buffer, 1601), 1601));
    append(buffer, packet(0, source, true, 2, 3, 4, 1), 1602);
    assert(!permission(observe(buffer, 1602), 1602));

    unsigned splits = 0;
    for (bool v2 : {false, true}) {
        const auto selected = fixture(source, v2);
        for (std::size_t split = 1; split < selected.size(); ++split) {
            vh::MavlinkTelemetryByteBuffer fragmented(4096, source);
            append(fragmented, fixture(other_component), 900);
            append(fragmented, selected.substr(0, split), 1000);
            assert(!observe(fragmented, 1000).valid);
            append(fragmented, selected.substr(split), 1100);
            const auto recovered = observe(fragmented, 1100);
            assert(recovered.valid && permission(recovered, 1100));
            assert(recovered.altitude.timestamp == at(1100));
            ++splits;
        }
    }
    vh::MavlinkTelemetryByteBuffer rolling(bytes.size(), source);
    append(rolling, bytes, 1000);
    append(rolling, fixture(other_system), 1100);
    assert(!observe(rolling, 1100).valid && rolling.receipts().heartbeat.end_offset == 0);
    append(rolling, bytes, 1200);
    assert(permission(observe(rolling, 1200), 1200));
    rolling.clear();
    assert(!observe(rolling, 1200).valid);
    append(rolling, fixture(other_component), 1300);
    assert(!observe(rolling, 1300).valid);
    append(rolling, bytes, 1400);
    assert(permission(observe(rolling, 1400), 1400));

    // Construction validates configuration before any worker/device can be opened.
    for (auto missing : {vh::MavlinkTelemetrySourceId{}, {42, 0}, {0, 17}}) {
        bool rejected = false;
        try { vh::MavlinkTelemetryStream stream({.device_path = "not-opened", .expected_source = missing}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    std::cout << "source_mode_contract_passed packet_splits=" << splits << '\n';
}
