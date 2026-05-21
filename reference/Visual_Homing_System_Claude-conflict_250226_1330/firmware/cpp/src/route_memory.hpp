/**
 * Visual Homing System - Route Memory
 */

#ifndef ROUTE_MEMORY_HPP
#define ROUTE_MEMORY_HPP

#include "feature_tracker.hpp"
#include "visual_odometry.hpp"
#include <vector>
#include <string>
#include <fstream>

namespace visual_homing {

struct Keyframe {
    int id;
    double timestamp;
    Pose pose;
    int features_count;
    double altitude;
};

/**
 * Route Memory - stores and retrieves keyframes
 */
class RouteMemory {
public:
    RouteMemory(const std::string& route_dir = "/home/pi/visual_homing/routes");

    // Recording
    bool startRecording(const std::string& route_name = "");
    bool addKeyframe(const cv::Mat& frame, const Pose& pose,
                     double altitude, bool force = false);
    bool stopRecording();

    // Playback
    bool loadRoute(const std::string& route_id);
    const std::vector<Keyframe>& getKeyframes() const { return keyframes_; }

    // Status
    bool isRecording() const { return recording_; }
    int keyframeCount() const { return static_cast<int>(keyframes_.size()); }

private:
    std::string route_dir_;
    std::string current_route_id_;
    std::string current_route_path_;

    bool recording_ = false;
    std::vector<Keyframe> keyframes_;
    std::vector<Features> keyframe_features_;

    FeatureTracker tracker_;
    Pose last_keyframe_pose_;
    int keyframe_counter_ = 0;

    double keyframe_distance_ = 2.0;
    double keyframe_angle_ = 15.0;
    int min_features_ = 50;
};

} // namespace visual_homing

#endif // ROUTE_MEMORY_HPP
