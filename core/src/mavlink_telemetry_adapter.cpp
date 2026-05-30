#include "visual_homing/mavlink_telemetry_adapter.hpp"

#include <algorithm>

namespace vh {

MavlinkTelemetryAdapter::MavlinkTelemetryAdapter(MavlinkTelemetryAdapterConfig config)
    : config_(config) {}

void MavlinkTelemetryAdapter::observe(const MavlinkTelemetry& telemetry, Timestamp received_at) {
    telemetry_ = telemetry;
    received_at_ = received_at;
}

bool MavlinkTelemetryAdapter::has_telemetry() const {
    return telemetry_.has_value();
}

bool MavlinkTelemetryAdapter::mavlink_ok(Timestamp timestamp) const {
    if (!telemetry_.has_value() || !telemetry_->heartbeat_seen) {
        return false;
    }

    const auto age_ms = milliseconds_between(received_at_, timestamp);
    return age_ms >= 0.0 && age_ms <= config_.max_telemetry_age_ms;
}

bool MavlinkTelemetryAdapter::command_permission_ok(Timestamp timestamp) const {
    if (!mavlink_ok(timestamp)) {
        return false;
    }
    if (config_.require_armed && !telemetry_->armed) {
        return false;
    }
    return telemetry_->mode == config_.required_mode;
}

void MavlinkTelemetryAdapter::apply_to_health(
    HealthMonitor& health,
    Timestamp timestamp,
    bool camera_ok,
    bool navigation_ok) const {
    const bool link_ok = mavlink_ok(timestamp);
    health.set_links(camera_ok, link_ok, navigation_ok && command_permission_ok(timestamp));
}

std::optional<NavigationEstimate> MavlinkTelemetryAdapter::navigation_estimate() const {
    if (!telemetry_.has_value()) {
        return std::nullopt;
    }

    NavigationEstimate estimate;
    estimate.timestamp = telemetry_->timestamp == Timestamp{} ? received_at_ : telemetry_->timestamp;
    estimate.course_error_rad = telemetry_->yaw_rad;
    estimate.altitude_m = telemetry_->relative_altitude_m;
    estimate.confidence = telemetry_->heartbeat_seen ? std::clamp(config_.navigation_confidence, 0.0, 1.0) : 0.0;
    return estimate;
}

} // namespace vh
