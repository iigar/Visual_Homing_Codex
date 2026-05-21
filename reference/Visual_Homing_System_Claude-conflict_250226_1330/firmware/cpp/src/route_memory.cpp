/**
 * Visual Homing System - Route Memory Implementation
 */

#include "route_memory.hpp"
#include <filesystem>
#include <ctime>
#include <cmath>
#include <iostream>

namespace fs = std::filesystem;

namespace visual_homing {

RouteMemory::RouteMemory(const std::string& route_dir)
    : route_dir_(route_dir)
    , tracker_(500)
{
    fs::create_directories(route_dir_);
}

bool RouteMemory::startRecording(const std::string& route_name) {
    if (recording_) {
        stopRecording();
    }

    // Generate route ID
    auto now = std::time(nullptr);
    current_route_id_ = "route_" + std::to_string(now);
    current_route_path_ = route_dir_ + "/" + current_route_id_;

    fs::create_directories(current_route_path_);

    keyframes_.clear();
    keyframe_features_.clear();
    keyframe_counter_ = 0;
    recording_ = true;

    std::cout << "Started recording route: " << current_route_id_ << std::endl;
    return true;
}

bool RouteMemory::addKeyframe(const cv::Mat& frame, const Pose& pose,
                               double altitude, bool force) {
    if (!recording_) return false;

    // Check distance from last keyframe
    if (!force && keyframe_counter_ > 0) {
        double dx = pose.x - last_keyframe_pose_.x;
        double dy = pose.y - last_keyframe_pose_.y;
        double distance = std::sqrt(dx*dx + dy*dy);

        double angle_diff = std::abs(pose.yaw - last_keyframe_pose_.yaw);
        angle_diff = std::min(angle_diff, 2*M_PI - angle_diff);
        double angle_diff_deg = angle_diff * 180.0 / M_PI;

        if (distance < keyframe_distance_ && angle_diff_deg < keyframe_angle_) {
            return false;
        }
    }

    // Detect features
    auto features = tracker_.detect(frame);
    if (!features || features->count() < min_features_) {
        return false;
    }

    // Create keyframe
    Keyframe kf;
    kf.id = keyframe_counter_++;
    kf.timestamp = pose.timestamp;
    kf.pose = pose;
    kf.features_count = features->count();
    kf.altitude = altitude;

    // Save thumbnail
    cv::Mat thumbnail;
    cv::resize(frame, thumbnail, cv::Size(160, 120));
    std::string thumb_path = current_route_path_ + "/kf_" + std::to_string(kf.id) + "_thumb.jpg";
    cv::imwrite(thumb_path, thumbnail);

    // Store keyframe
    keyframes_.push_back(kf);
    keyframe_features_.push_back(*features);
    last_keyframe_pose_ = pose;

    std::cout << "Added keyframe " << kf.id << " at (" << pose.x << ", " << pose.y << ")" << std::endl;
    return true;
}

bool RouteMemory::stopRecording() {
    if (!recording_) return false;

    recording_ = false;

    // Save route metadata
    std::string meta_path = current_route_path_ + "/route.txt";
    std::ofstream meta(meta_path);
    meta << "id: " << current_route_id_ << "\n";
    meta << "keyframes: " << keyframes_.size() << "\n";
    meta.close();

    std::cout << "Route saved: " << current_route_id_
              << " with " << keyframes_.size() << " keyframes" << std::endl;
    return true;
}

bool RouteMemory::loadRoute(const std::string& route_id) {
    std::string route_path = route_dir_ + "/" + route_id;
    if (!fs::exists(route_path)) {
        std::cerr << "Route not found: " << route_id << std::endl;
        return false;
    }

    // TODO: Load keyframes from disk
    return true;
}

} // namespace visual_homing
