#pragma once

#include <string_view>

namespace vh {

enum class LocalCoordinateFrame {
    route_frd,
    local_ned,
    local_enu,
};

enum class BodyCoordinateFrame {
    frd,
    flu,
};

struct Vector3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Unit quaternion describing a body-to-local-frame rotation.
struct Quaterniond {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct RouteFrameAlignment {
    Vector3d origin_ned_m{};
    double heading_ned_rad = 0.0;
};

std::string_view coordinate_frame_name(LocalCoordinateFrame frame) noexcept;
std::string_view coordinate_frame_name(BodyCoordinateFrame frame) noexcept;

Vector3d enu_to_ned(const Vector3d& enu);
Vector3d ned_to_enu(const Vector3d& ned);
Vector3d flu_to_frd(const Vector3d& flu);
Vector3d frd_to_flu(const Vector3d& frd);

double wrap_angle_pi(double angle_rad);
double yaw_enu_to_ned(double yaw_enu_rad);
double yaw_ned_to_enu(double yaw_ned_rad);

Vector3d route_frd_to_local_ned(
    const Vector3d& route_frd_m,
    const RouteFrameAlignment& alignment);

Quaterniond enu_flu_to_ned_frd(const Quaterniond& q_enu_flu);
Quaterniond ned_frd_to_enu_flu(const Quaterniond& q_ned_frd);

} // namespace vh
