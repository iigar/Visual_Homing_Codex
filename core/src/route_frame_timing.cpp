#include "visual_homing/route_frame_timing.hpp"

#include <ostream>

#include "visual_homing/mavlink_telemetry_adapter.hpp"

namespace vh {

RouteFrameTiming::RouteFrameTiming(Timestamp source_timestamp, Timestamp started)
    : source_timestamp_(source_timestamp) {
    boundaries_[0] = started;
}

void RouteFrameTiming::complete(RouteFrameStage stage, Timestamp finished) {
    if (static_cast<std::size_t>(stage) != completed_ || completed_ >= boundaries_.size() - 1) {
        monotonic_ = false;
        return;
    }
    monotonic_ = monotonic_ && finished >= boundaries_[completed_];
    boundaries_[++completed_] = finished;
}

RouteFrameTimingSummary RouteFrameTiming::summary() const {
    RouteFrameTimingSummary result;
    result.valid = monotonic_ && completed_ == result.stage_ms.size();
    result.source_timestamp_present = source_timestamp_ != Timestamp{};
    for (std::size_t i = 0; i < completed_; ++i) {
        result.stage_ms[i] = milliseconds_between(boundaries_[i], boundaries_[i + 1]);
    }
    result.frame_work_ms = milliseconds_between(boundaries_[0], boundaries_[completed_]);
    result.source_age_at_start_ms = milliseconds_between(source_timestamp_, boundaries_[0]);
    result.source_age_at_finish_ms = milliseconds_between(source_timestamp_, boundaries_[completed_]);
    return result;
}

void log_route_frame_timing(std::ostream& output, const char* caller, std::uint64_t frame_id,
                           const RouteFrameTimingSummary& timing) {
    constexpr std::array<const char*, static_cast<std::size_t>(RouteFrameStage::Count)> names{
        "preprocess_ms", "match_ms", "diagnostics_ms", "navigation_ms", "scale_ms",
        "external_nav_ms", "verification_ms", "reporting_endpoint_ms"};
    output << "route_frame_timing caller=" << caller << " id=" << frame_id
           << " timing_valid=" << (timing.valid ? "true" : "false")
           << " source_timestamp_present=" << (timing.source_timestamp_present ? "true" : "false")
           << " frame_work_ms=" << timing.frame_work_ms
           << " source_age_at_start_ms=" << timing.source_age_at_start_ms
           << " source_age_at_finish_ms=" << timing.source_age_at_finish_ms;
    for (std::size_t i = 0; i < names.size(); ++i) output << ' ' << names[i] << '=' << timing.stage_ms[i];
    output << '\n';
}

HealthSnapshot refresh_route_frame_health(const HealthSnapshot& previous,
                                         const MavlinkTelemetryAdapter* telemetry,
                                         Timestamp evaluated_at) {
    auto result = previous;
    result.timestamp = evaluated_at;
    if (previous.timestamp == Timestamp{} || evaluated_at == Timestamp{} || evaluated_at < previous.timestamp) {
        result.camera_ok = result.mavlink_ok = result.navigation_ok = false;
    } else if (telemetry) {
        result.mavlink_ok = previous.mavlink_ok && telemetry->mavlink_ok(evaluated_at);
    }
    if (result.state == HealthState::Ready
        && !(result.camera_ok && result.mavlink_ok && result.navigation_ok)) {
        result.state = HealthState::Degraded;
    }
    return result;
}

} // namespace vh
