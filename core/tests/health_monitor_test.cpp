#include <cassert>
#include <chrono>

#include "visual_homing/health_monitor.hpp"

namespace {

vh::Timestamp at_ms(int milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(std::chrono::milliseconds(milliseconds)));
}

} // namespace

int main() {
    vh::HealthMonitor monitor(at_ms(0));
    monitor.set_links(true, true, true);
    monitor.set_route_match_confidence(1.5);

    vh::Frame frame;
    frame.id = 7;
    frame.timestamp = at_ms(100);

    const auto timing = monitor.observe_processed_frame(frame, at_ms(125), at_ms(130));
    assert(timing.frame_id == 7);
    assert(timing.frame_age_ms == 25.0);
    assert(timing.processing_latency_ms == 5.0);

    auto health = monitor.snapshot(at_ms(130));
    assert(health.state == vh::HealthState::Ready);
    assert(health.frames_seen == 1);
    assert(health.frames_dropped == 0);
    assert(health.frame_age_ms == 25.0);
    assert(health.processing_latency_ms == 5.0);
    assert(health.route_match_confidence == 1.0);
    assert(health.camera_ok);
    assert(health.mavlink_ok);
    assert(health.navigation_ok);

    monitor.observe_dropped_frame(at_ms(150));
    monitor.set_links(false, false, false);
    health = monitor.snapshot(at_ms(150));
    assert(health.state == vh::HealthState::Degraded);
    assert(health.frames_seen == 1);
    assert(health.frames_dropped == 1);

    return 0;
}
