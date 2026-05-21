#pragma once

#include <cstdint>

#include "visual_homing/time.hpp"

namespace vh {

enum class HealthState {
    Booting,
    Ready,
    Degraded,
    Failsafe,
    Shutdown
};

struct HealthSnapshot {
    HealthState state = HealthState::Booting;
    Timestamp timestamp{};
    std::uint64_t frames_seen = 0;
    std::uint64_t frames_dropped = 0;
    double frame_age_ms = 0.0;
    double processing_latency_ms = 0.0;
    double route_match_confidence = 0.0;
    bool camera_ok = false;
    bool mavlink_ok = false;
    bool navigation_ok = false;
};

} // namespace vh
