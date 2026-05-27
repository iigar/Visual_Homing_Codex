#pragma once

#include <cstdint>

#include "visual_homing/frame.hpp"
#include "visual_homing/health.hpp"

namespace vh {

struct FrameTiming {
    std::uint64_t frame_id = 0;
    Timestamp timestamp{};
    double frame_age_ms = 0.0;
    double processing_latency_ms = 0.0;
};

class HealthMonitor {
public:
    explicit HealthMonitor(Timestamp started_at);

    FrameTiming observe_processed_frame(const Frame& frame, Timestamp processing_started, Timestamp processing_finished);
    void observe_dropped_frame(Timestamp timestamp);
    void set_route_match_confidence(double confidence);
    void set_links(bool camera_ok, bool mavlink_ok, bool navigation_ok);

    HealthSnapshot snapshot(Timestamp timestamp) const;

private:
    Timestamp started_at_{};
    std::uint64_t frames_seen_ = 0;
    std::uint64_t frames_dropped_ = 0;
    double frame_age_ms_ = 0.0;
    double processing_latency_ms_ = 0.0;
    double route_match_confidence_ = 0.0;
    bool camera_ok_ = false;
    bool mavlink_ok_ = false;
    bool navigation_ok_ = false;
};

} // namespace vh
