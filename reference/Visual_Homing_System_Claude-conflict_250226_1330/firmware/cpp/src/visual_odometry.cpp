/**
 * Visual Homing System - Visual Odometry Implementation
 */

#include "visual_odometry.hpp"
#include <cmath>
#include <iostream>

namespace visual_homing {

VisualOdometry::VisualOdometry(int n_features)
    : tracker_(n_features)
{
    // Default camera matrix
    camera_matrix_ = (cv::Mat_<double>(3, 3) <<
        500, 0, 360,
        0, 500, 288,
        0, 0, 1);
}

void VisualOdometry::setAltitude(double altitude) {
    current_altitude_ = std::max(0.5, altitude);
}

std::pair<std::optional<Pose>, std::optional<Velocity>>
VisualOdometry::processFrame(const cv::Mat& frame, double timestamp) {
    // Detect features
    auto current_features = tracker_.detect(frame);

    if (!current_features || current_features->count() < 10) {
        prev_frame_ = frame.clone();
        prev_features_ = current_features;
        prev_timestamp_ = timestamp;
        return {std::nullopt, std::nullopt};
    }

    // First frame
    if (!prev_features_) {
        prev_frame_ = frame.clone();
        prev_features_ = current_features;
        prev_timestamp_ = timestamp;
        return {pose_, velocity_};
    }

    // Match features
    auto match_result = tracker_.match(*prev_features_, *current_features);

    if (!match_result || match_result->inlierCount() < 8) {
        prev_frame_ = frame.clone();
        prev_features_ = current_features;
        prev_timestamp_ = timestamp;
        return {std::nullopt, std::nullopt};
    }

    // Calculate dt
    double dt = timestamp - prev_timestamp_;
    if (dt <= 0) dt = 0.033;

    // Decompose homography
    if (!match_result->homography.empty()) {
        auto [dx, dy, dyaw] = decomposeHomography(match_result->homography);

        // Update pose
        pose_.x += dx;
        pose_.y += dy;
        pose_.yaw += dyaw;
        pose_.z = current_altitude_;
        pose_.timestamp = timestamp;
        pose_.confidence = static_cast<double>(match_result->inlierCount()) /
                          static_cast<double>(match_result->matches.size());

        // Update velocity
        velocity_.vx = dx / dt;
        velocity_.vy = dy / dt;
        velocity_.timestamp = timestamp;
    }

    // Update previous
    prev_frame_ = frame.clone();
    prev_features_ = current_features;
    prev_timestamp_ = timestamp;

    return {pose_, velocity_};
}

std::tuple<double, double, double>
VisualOdometry::decomposeHomography(const cv::Mat& H) {
    try {
        double fx = camera_matrix_.at<double>(0, 0);
        double fy = camera_matrix_.at<double>(1, 1);

        double tx_px = H.at<double>(0, 2);
        double ty_px = H.at<double>(1, 2);

        double dx = tx_px * current_altitude_ / fx;
        double dy = ty_px * current_altitude_ / fy;

        double dyaw = std::atan2(H.at<double>(1, 0), H.at<double>(0, 0));

        return {dx, dy, dyaw};
    } catch (...) {
        return {0.0, 0.0, 0.0};
    }
}

void VisualOdometry::reset() {
    prev_frame_.release();
    prev_features_ = std::nullopt;
    prev_timestamp_ = 0.0;
    pose_ = Pose{};
    velocity_ = Velocity{};
}

} // namespace visual_homing
