/**
 * Visual Homing System - Visual Odometry
 */

#ifndef VISUAL_ODOMETRY_HPP
#define VISUAL_ODOMETRY_HPP

#include "feature_tracker.hpp"
#include <optional>

namespace visual_homing {

struct Pose {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double yaw = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double timestamp = 0.0;
    double confidence = 0.0;
};

struct Velocity {
    double vx = 0.0;
    double vy = 0.0;
    double vz = 0.0;
    double timestamp = 0.0;
};

/**
 * Visual Odometry for motion estimation
 */
class VisualOdometry {
public:
    VisualOdometry(int n_features = 500);

    // Set current altitude from external source
    void setAltitude(double altitude);

    // Process frame and estimate motion
    std::pair<std::optional<Pose>, std::optional<Velocity>>
    processFrame(const cv::Mat& frame, double timestamp);

    // Reset odometry
    void reset();

    // Getters
    const Pose& getPose() const { return pose_; }
    const Velocity& getVelocity() const { return velocity_; }

private:
    std::tuple<double, double, double> decomposeHomography(const cv::Mat& H);

    FeatureTracker tracker_;
    cv::Mat camera_matrix_;

    cv::Mat prev_frame_;
    std::optional<Features> prev_features_;
    double prev_timestamp_ = 0.0;

    Pose pose_;
    Velocity velocity_;
    double current_altitude_ = 1.0;
};

} // namespace visual_homing

#endif // VISUAL_ODOMETRY_HPP
