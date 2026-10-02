#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>

#include "visual_homing/health.hpp"

namespace vh {

class MavlinkTelemetryAdapter;

enum class RouteFrameStage : std::size_t {
    Preprocess, Match, Diagnostics, Navigation, Scale, ExternalNavigation, Verification, Reporting, Count
};

struct RouteFrameTimingSummary {
    bool valid = false;
    bool source_timestamp_present = false;
    std::array<double, static_cast<std::size_t>(RouteFrameStage::Count)> stage_ms{};
    double frame_work_ms = 0.0;
    // Signed ages, never clamped: a future/absent source must remain visible.
    double source_age_at_start_ms = 0.0;
    double source_age_at_finish_ms = 0.0;
};

// Fixed per-frame storage. Intervals partition synchronous work after poll,
// through reporting/endpoint/export; the timing log itself and async work are
// excluded. In replay the caller explicitly completes absent stages at no cost.
class RouteFrameTiming {
public:
    RouteFrameTiming(Timestamp source_timestamp, Timestamp started);
    void complete(RouteFrameStage stage, Timestamp finished);
    RouteFrameTimingSummary summary() const;

private:
    Timestamp source_timestamp_;
    std::array<Timestamp, static_cast<std::size_t>(RouteFrameStage::Count) + 1> boundaries_{};
    std::size_t completed_ = 0;
    bool monotonic_ = true;
};

void log_route_frame_timing(std::ostream& output, const char* caller, std::uint64_t frame_id,
                           const RouteFrameTimingSummary& timing);

// Re-evaluate at the consumer boundary without observing another frame, changing
// source timestamps, or promoting any previous health failure. The adapter is
// optional only when telemetry is not part of this caller's health contract.
HealthSnapshot refresh_route_frame_health(const HealthSnapshot& previous,
                                         const MavlinkTelemetryAdapter* telemetry,
                                         Timestamp evaluated_at);

} // namespace vh
