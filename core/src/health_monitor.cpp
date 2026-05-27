#include "visual_homing/health_monitor.hpp"

#include <algorithm>

#include "visual_homing/time.hpp"

namespace vh {

HealthMonitor::HealthMonitor(Timestamp started_at)
    : started_at_(started_at) {}

FrameTiming HealthMonitor::observe_processed_frame(
    const Frame& frame,
    Timestamp processing_started,
    Timestamp processing_finished) {
    ++frames_seen_;
    frame_age_ms_ = std::max(0.0, milliseconds_between(frame.timestamp, processing_started));
    processing_latency_ms_ = std::max(0.0, milliseconds_between(processing_started, processing_finished));

    FrameTiming timing;
    timing.frame_id = frame.id;
    timing.timestamp = processing_finished;
    timing.frame_age_ms = frame_age_ms_;
    timing.processing_latency_ms = processing_latency_ms_;
    return timing;
}

void HealthMonitor::observe_dropped_frame(Timestamp timestamp) {
    ++frames_dropped_;
    frame_age_ms_ = std::max(0.0, milliseconds_between(started_at_, timestamp));
}

void HealthMonitor::set_route_match_confidence(double confidence) {
    route_match_confidence_ = std::clamp(confidence, 0.0, 1.0);
}

void HealthMonitor::set_links(bool camera_ok, bool mavlink_ok, bool navigation_ok) {
    camera_ok_ = camera_ok;
    mavlink_ok_ = mavlink_ok;
    navigation_ok_ = navigation_ok;
}

HealthSnapshot HealthMonitor::snapshot(Timestamp timestamp) const {
    HealthSnapshot health;
    health.state = (camera_ok_ && mavlink_ok_ && navigation_ok_) ? HealthState::Ready : HealthState::Degraded;
    health.timestamp = timestamp;
    health.frames_seen = frames_seen_;
    health.frames_dropped = frames_dropped_;
    health.frame_age_ms = frame_age_ms_;
    health.processing_latency_ms = processing_latency_ms_;
    health.route_match_confidence = route_match_confidence_;
    health.camera_ok = camera_ok_;
    health.mavlink_ok = mavlink_ok_;
    health.navigation_ok = navigation_ok_;
    return health;
}

} // namespace vh
