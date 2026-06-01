#pragma once

#include "visual_homing/time.hpp"

namespace vh {

enum class FlightMode {
    Unknown,
    Manual,
    Stabilize,
    AltHold,
    Guided,
    Auto,
    Rtl,
    Land
};

struct MavlinkTelemetry {
    Timestamp timestamp{};
    bool heartbeat_seen = false;
    bool armed = false;
    FlightMode mode = FlightMode::Unknown;
    double roll_rad = 0.0;
    double pitch_rad = 0.0;
    double yaw_rad = 0.0;
    double relative_altitude_m = 0.0;
};

} // namespace vh
