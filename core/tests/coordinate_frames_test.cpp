#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "visual_homing/coordinate_frames.hpp"

namespace {

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double tolerance = 1.0e-10;

bool near(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= tolerance;
}

void assert_vector(const vh::Vector3d& actual, const vh::Vector3d& expected) {
    assert(near(actual.x, expected.x));
    assert(near(actual.y, expected.y));
    assert(near(actual.z, expected.z));
}

double quaternion_alignment(const vh::Quaterniond& lhs, const vh::Quaterniond& rhs) {
    return std::abs(lhs.w * rhs.w + lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z);
}

} // namespace

int main() {
    assert(vh::coordinate_frame_name(vh::LocalCoordinateFrame::route_frd) == "route_frd");
    assert(vh::coordinate_frame_name(vh::LocalCoordinateFrame::local_ned) == "local_ned");
    assert(vh::coordinate_frame_name(vh::LocalCoordinateFrame::local_enu) == "local_enu");
    assert(vh::coordinate_frame_name(vh::BodyCoordinateFrame::frd) == "frd");
    assert(vh::coordinate_frame_name(vh::BodyCoordinateFrame::flu) == "flu");

    assert_vector(vh::enu_to_ned({1.0, 0.0, 0.0}), {0.0, 1.0, 0.0});
    assert_vector(vh::enu_to_ned({0.0, 1.0, 0.0}), {1.0, 0.0, 0.0});
    assert_vector(vh::enu_to_ned({0.0, 0.0, 1.0}), {0.0, 0.0, -1.0});
    const vh::Vector3d enu{2.0, -3.0, 4.0};
    assert_vector(vh::ned_to_enu(vh::enu_to_ned(enu)), enu);

    assert_vector(vh::flu_to_frd({1.0, 2.0, 3.0}), {1.0, -2.0, -3.0});
    const vh::Vector3d flu{-2.0, 5.0, -7.0};
    assert_vector(vh::frd_to_flu(vh::flu_to_frd(flu)), flu);

    assert(near(vh::yaw_enu_to_ned(0.0), pi / 2.0));
    assert(near(vh::yaw_enu_to_ned(pi / 2.0), 0.0));
    assert(near(vh::yaw_ned_to_enu(pi / 2.0), 0.0));
    const auto yaw_enu = -2.4;
    assert(near(vh::yaw_ned_to_enu(vh::yaw_enu_to_ned(yaw_enu)), yaw_enu));

    vh::RouteFrameAlignment north_alignment;
    north_alignment.origin_ned_m = {10.0, 20.0, 3.0};
    north_alignment.heading_ned_rad = 0.0;
    assert_vector(
        vh::route_frd_to_local_ned({4.0, 2.0, -1.0}, north_alignment),
        {14.0, 22.0, 2.0});

    vh::RouteFrameAlignment east_alignment;
    east_alignment.heading_ned_rad = pi / 2.0;
    assert_vector(
        vh::route_frd_to_local_ned({4.0, 2.0, -1.0}, east_alignment),
        {-2.0, 4.0, -1.0});

    const auto identity_enu_flu = vh::Quaterniond{};
    const auto identity_as_ned_frd = vh::enu_flu_to_ned_frd(identity_enu_flu);
    const auto root_half = std::sqrt(0.5);
    assert(quaternion_alignment(identity_as_ned_frd, {root_half, 0.0, 0.0, root_half}) > 1.0 - tolerance);
    const auto identity_round_trip = vh::ned_frd_to_enu_flu(identity_as_ned_frd);
    assert(quaternion_alignment(identity_round_trip, identity_enu_flu) > 1.0 - tolerance);

    const vh::Quaterniond north_facing_enu_flu{root_half, 0.0, 0.0, root_half};
    const auto north_facing_ned_frd = vh::enu_flu_to_ned_frd(north_facing_enu_flu);
    assert(quaternion_alignment(north_facing_ned_frd, vh::Quaterniond{}) > 1.0 - tolerance);

    bool rejected = false;
    try {
        (void)vh::enu_to_ned({std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        (void)vh::enu_flu_to_ned_frd({0.0, 0.0, 0.0, 0.0});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        vh::RouteFrameAlignment invalid;
        invalid.heading_ned_rad = std::numeric_limits<double>::infinity();
        (void)vh::route_frd_to_local_ned({}, invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    return 0;
}
