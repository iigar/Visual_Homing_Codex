#include "visual_homing/coordinate_frames.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace vh {
namespace {

using Matrix3d = std::array<std::array<double, 3>, 3>;

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double quaternion_norm_epsilon = 1.0e-12;

void require_finite(const Vector3d& value, const char* label) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        throw std::invalid_argument(std::string(label) + " must be finite");
    }
}

Quaterniond normalized(const Quaterniond& value) {
    if (!std::isfinite(value.w) || !std::isfinite(value.x)
        || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        throw std::invalid_argument("quaternion must be finite");
    }
    const auto norm = std::sqrt(
        value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z);
    if (!std::isfinite(norm) || norm <= quaternion_norm_epsilon) {
        throw std::invalid_argument("quaternion norm must be positive");
    }
    return {value.w / norm, value.x / norm, value.y / norm, value.z / norm};
}

Matrix3d multiply(const Matrix3d& lhs, const Matrix3d& rhs) {
    Matrix3d result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t index = 0; index < 3; ++index) {
                result[row][column] += lhs[row][index] * rhs[index][column];
            }
        }
    }
    return result;
}

Matrix3d quaternion_to_matrix(const Quaterniond& input) {
    const auto q = normalized(input);
    return {{
        {{1.0 - 2.0 * (q.y * q.y + q.z * q.z), 2.0 * (q.x * q.y - q.z * q.w), 2.0 * (q.x * q.z + q.y * q.w)}},
        {{2.0 * (q.x * q.y + q.z * q.w), 1.0 - 2.0 * (q.x * q.x + q.z * q.z), 2.0 * (q.y * q.z - q.x * q.w)}},
        {{2.0 * (q.x * q.z - q.y * q.w), 2.0 * (q.y * q.z + q.x * q.w), 1.0 - 2.0 * (q.x * q.x + q.y * q.y)}}
    }};
}

Quaterniond matrix_to_quaternion(const Matrix3d& matrix) {
    Quaterniond result;
    const auto trace = matrix[0][0] + matrix[1][1] + matrix[2][2];
    if (trace > 0.0) {
        const auto scale = 2.0 * std::sqrt(trace + 1.0);
        result.w = 0.25 * scale;
        result.x = (matrix[2][1] - matrix[1][2]) / scale;
        result.y = (matrix[0][2] - matrix[2][0]) / scale;
        result.z = (matrix[1][0] - matrix[0][1]) / scale;
    } else if (matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2]) {
        const auto scale = 2.0 * std::sqrt(1.0 + matrix[0][0] - matrix[1][1] - matrix[2][2]);
        result.w = (matrix[2][1] - matrix[1][2]) / scale;
        result.x = 0.25 * scale;
        result.y = (matrix[0][1] + matrix[1][0]) / scale;
        result.z = (matrix[0][2] + matrix[2][0]) / scale;
    } else if (matrix[1][1] > matrix[2][2]) {
        const auto scale = 2.0 * std::sqrt(1.0 + matrix[1][1] - matrix[0][0] - matrix[2][2]);
        result.w = (matrix[0][2] - matrix[2][0]) / scale;
        result.x = (matrix[0][1] + matrix[1][0]) / scale;
        result.y = 0.25 * scale;
        result.z = (matrix[1][2] + matrix[2][1]) / scale;
    } else {
        const auto scale = 2.0 * std::sqrt(1.0 + matrix[2][2] - matrix[0][0] - matrix[1][1]);
        result.w = (matrix[1][0] - matrix[0][1]) / scale;
        result.x = (matrix[0][2] + matrix[2][0]) / scale;
        result.y = (matrix[1][2] + matrix[2][1]) / scale;
        result.z = 0.25 * scale;
    }
    return normalized(result);
}

const Matrix3d enu_to_ned_basis{{
    {{0.0, 1.0, 0.0}},
    {{1.0, 0.0, 0.0}},
    {{0.0, 0.0, -1.0}}
}};

const Matrix3d frd_to_flu_basis{{
    {{1.0, 0.0, 0.0}},
    {{0.0, -1.0, 0.0}},
    {{0.0, 0.0, -1.0}}
}};

} // namespace

std::string_view coordinate_frame_name(LocalCoordinateFrame frame) noexcept {
    switch (frame) {
    case LocalCoordinateFrame::route_frd: return "route_frd";
    case LocalCoordinateFrame::local_ned: return "local_ned";
    case LocalCoordinateFrame::local_enu: return "local_enu";
    }
    return "unknown";
}

std::string_view coordinate_frame_name(BodyCoordinateFrame frame) noexcept {
    switch (frame) {
    case BodyCoordinateFrame::frd: return "frd";
    case BodyCoordinateFrame::flu: return "flu";
    }
    return "unknown";
}

Vector3d enu_to_ned(const Vector3d& enu) {
    require_finite(enu, "ENU vector");
    return {enu.y, enu.x, -enu.z};
}

Vector3d ned_to_enu(const Vector3d& ned) {
    require_finite(ned, "NED vector");
    return {ned.y, ned.x, -ned.z};
}

Vector3d flu_to_frd(const Vector3d& flu) {
    require_finite(flu, "FLU vector");
    return {flu.x, -flu.y, -flu.z};
}

Vector3d frd_to_flu(const Vector3d& frd) {
    require_finite(frd, "FRD vector");
    return {frd.x, -frd.y, -frd.z};
}

double wrap_angle_pi(double angle_rad) {
    if (!std::isfinite(angle_rad)) {
        throw std::invalid_argument("angle must be finite");
    }
    return std::remainder(angle_rad, 2.0 * pi);
}

double yaw_enu_to_ned(double yaw_enu_rad) {
    return wrap_angle_pi((pi / 2.0) - yaw_enu_rad);
}

double yaw_ned_to_enu(double yaw_ned_rad) {
    return wrap_angle_pi((pi / 2.0) - yaw_ned_rad);
}

Vector3d route_frd_to_local_ned(const Vector3d& route_frd_m, const RouteFrameAlignment& alignment) {
    require_finite(route_frd_m, "route FRD position");
    require_finite(alignment.origin_ned_m, "route NED origin");
    if (!std::isfinite(alignment.heading_ned_rad)) {
        throw std::invalid_argument("route NED heading must be finite");
    }
    const auto cosine = std::cos(alignment.heading_ned_rad);
    const auto sine = std::sin(alignment.heading_ned_rad);
    return {
        alignment.origin_ned_m.x + cosine * route_frd_m.x - sine * route_frd_m.y,
        alignment.origin_ned_m.y + sine * route_frd_m.x + cosine * route_frd_m.y,
        alignment.origin_ned_m.z + route_frd_m.z,
    };
}

Quaterniond enu_flu_to_ned_frd(const Quaterniond& q_enu_flu) {
    const auto rotation = multiply(
        multiply(enu_to_ned_basis, quaternion_to_matrix(q_enu_flu)),
        frd_to_flu_basis);
    return matrix_to_quaternion(rotation);
}

Quaterniond ned_frd_to_enu_flu(const Quaterniond& q_ned_frd) {
    const auto rotation = multiply(
        multiply(enu_to_ned_basis, quaternion_to_matrix(q_ned_frd)),
        frd_to_flu_basis);
    return matrix_to_quaternion(rotation);
}

} // namespace vh
