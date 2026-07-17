#pragma once

#include <cstdint>
#include <vector>

namespace vh {

// Pose in a fixed route-local Forward/Right/Down frame. The origin is the
// recorded route start; z is displacement down from that origin, not AGL.
struct RouteLocalOdometryEstimate {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    double yaw_rad = 0.0;
    std::uint8_t reset_counter = 0;
    bool valid_for_fc = false;
};

// Encodes the ArduCopter 4.3.6 ODOMETRY contract used by handle_odometry:
// pose in MAV_FRAME_LOCAL_FRD, twist in MAV_FRAME_BODY_FRD. Velocity and
// covariance are unknown (NaN) so this position/yaw-only boundary cannot
// accidentally inject zero velocity into the EKF.
std::vector<std::uint8_t> encode_mavlink2_route_local_odometry(
    const RouteLocalOdometryEstimate& estimate,
    std::uint64_t time_usec,
    std::uint8_t sequence,
    std::uint8_t source_system,
    std::uint8_t source_component);

} // namespace vh
