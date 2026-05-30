#include <cassert>
#include <chrono>

#include "visual_homing/mavlink_telemetry_adapter.hpp"

namespace {

vh::Timestamp at_ms(int milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(std::chrono::milliseconds(milliseconds)));
}

} // namespace

int main() {
    vh::MavlinkTelemetryAdapter adapter({.max_telemetry_age_ms = 100.0, .navigation_confidence = 0.8});
    vh::HealthMonitor health(at_ms(0));

    assert(!adapter.has_telemetry());
    assert(!adapter.mavlink_ok(at_ms(10)));
    adapter.apply_to_health(health, at_ms(10), true, true);
    auto snapshot = health.snapshot(at_ms(10));
    assert(snapshot.state == vh::HealthState::Degraded);
    assert(snapshot.camera_ok);
    assert(!snapshot.mavlink_ok);
    assert(!snapshot.navigation_ok);

    vh::MavlinkTelemetry telemetry;
    telemetry.heartbeat_seen = true;
    telemetry.armed = true;
    telemetry.mode = vh::FlightMode::Guided;
    telemetry.yaw_rad = 0.35;
    telemetry.relative_altitude_m = 42.5;

    adapter.observe(telemetry, at_ms(20));
    assert(adapter.has_telemetry());
    assert(adapter.mavlink_ok(at_ms(90)));
    assert(adapter.command_permission_ok(at_ms(90)));
    assert(!adapter.mavlink_ok(at_ms(121)));
    assert(!adapter.command_permission_ok(at_ms(121)));

    adapter.apply_to_health(health, at_ms(90), true, true);
    snapshot = health.snapshot(at_ms(90));
    assert(snapshot.state == vh::HealthState::Ready);
    assert(snapshot.mavlink_ok);
    assert(snapshot.navigation_ok);

    const auto nav = adapter.navigation_estimate();
    assert(nav.has_value());
    assert(nav->timestamp == at_ms(20));
    assert(nav->course_error_rad == 0.35);
    assert(nav->altitude_m == 42.5);
    assert(nav->confidence == 0.8);

    telemetry.heartbeat_seen = false;
    adapter.observe(telemetry, at_ms(130));
    assert(!adapter.mavlink_ok(at_ms(130)));
    assert(!adapter.command_permission_ok(at_ms(130)));
    const auto no_heartbeat_nav = adapter.navigation_estimate();
    assert(no_heartbeat_nav.has_value());
    assert(no_heartbeat_nav->confidence == 0.0);

    telemetry.heartbeat_seen = true;
    telemetry.armed = false;
    telemetry.mode = vh::FlightMode::Guided;
    adapter.observe(telemetry, at_ms(150));
    assert(adapter.mavlink_ok(at_ms(150)));
    assert(!adapter.command_permission_ok(at_ms(150)));
    adapter.apply_to_health(health, at_ms(150), true, true);
    snapshot = health.snapshot(at_ms(150));
    assert(snapshot.mavlink_ok);
    assert(!snapshot.navigation_ok);
    assert(snapshot.state == vh::HealthState::Degraded);

    telemetry.armed = true;
    telemetry.mode = vh::FlightMode::Manual;
    adapter.observe(telemetry, at_ms(160));
    assert(adapter.mavlink_ok(at_ms(160)));
    assert(!adapter.command_permission_ok(at_ms(160)));

    telemetry.mode = vh::FlightMode::Guided;
    adapter.observe(telemetry, at_ms(170));
    assert(adapter.command_permission_ok(at_ms(170)));

    return 0;
}
