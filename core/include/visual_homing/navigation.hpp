#pragma once

#include "visual_homing/time.hpp"

namespace vh {

struct NavigationEstimate {
    Timestamp timestamp{};
    double course_error_rad = 0.0;
    double ground_speed_mps = 0.0;
    double altitude_m = 0.0;
    double confidence = 0.0;
};

struct NavigationCommand {
    Timestamp timestamp{};
    double vx_mps = 0.0;
    double vy_mps = 0.0;
    double yaw_rate_radps = 0.0;
    double confidence = 0.0;
    bool valid = false;
};

} // namespace vh
