#pragma once

#include <optional>

#include "visual_homing/health_monitor.hpp"
#include "visual_homing/mavlink.hpp"
#include "visual_homing/navigation.hpp"
#include "visual_homing/time.hpp"

namespace vh {

struct MavlinkTelemetryAdapterConfig {
    double max_telemetry_age_ms = 500.0;
    double navigation_confidence = 1.0;
};

class MavlinkTelemetryAdapter {
public:
    explicit MavlinkTelemetryAdapter(MavlinkTelemetryAdapterConfig config = {});

    void observe(const MavlinkTelemetry& telemetry, Timestamp received_at);
    bool has_telemetry() const;
    bool mavlink_ok(Timestamp timestamp) const;
    void apply_to_health(HealthMonitor& health, Timestamp timestamp, bool camera_ok, bool navigation_ok) const;
    std::optional<NavigationEstimate> navigation_estimate() const;

private:
    MavlinkTelemetryAdapterConfig config_;
    std::optional<MavlinkTelemetry> telemetry_;
    Timestamp received_at_{};
};

} // namespace vh
