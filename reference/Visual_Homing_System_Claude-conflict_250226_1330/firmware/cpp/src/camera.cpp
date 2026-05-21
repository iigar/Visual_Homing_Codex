/**
 * Visual Homing System - Camera Implementation
 */

#include "camera.hpp"
#include <iostream>

namespace visual_homing {

Camera::Camera(Type type, const std::string& device, int width, int height, int fps)
    : type_(type)
    , device_(device)
    , width_(width)
    , height_(height)
    , fps_(fps)
{}

Camera::~Camera() {
    stop();
}

bool Camera::start() {
    if (running_) {
        std::cerr << "Camera already running" << std::endl;
        return false;
    }

    // Open camera device
    int device_id = 0;
    if (device_.find("video") != std::string::npos) {
        device_id = std::stoi(device_.substr(device_.find_last_of("video") + 1));
    }

    cap_.open(device_id, cv::CAP_V4L2);

    if (!cap_.isOpened()) {
        std::cerr << "Failed to open camera: " << device_ << std::endl;
        return false;
    }

    // Configure
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap_.set(cv::CAP_PROP_FPS, fps_);

    running_ = true;
    capture_thread_ = std::thread(&Camera::captureLoop, this);

    std::cout << "Camera started: " << device_ 
              << " at " << width_ << "x" << height_ << "@" << fps_ << "fps" << std::endl;

    return true;
}

void Camera::stop() {
    running_ = false;

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    if (cap_.isOpened()) {
        cap_.release();
    }

    std::cout << "Camera stopped" << std::endl;
}

void Camera::captureLoop() {
    cv::Mat frame;

    while (running_) {
        if (cap_.read(frame) && !frame.empty()) {
            auto now = std::chrono::high_resolution_clock::now();
            double timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();

            frame_count_++;

            FrameInfo info{
                timestamp,
                frame_count_.load(),
                frame.cols,
                frame.rows
            };

            {
                std::lock_guard<std::mutex> lock(frame_mutex_);
                current_frame_ = frame.clone();
                current_info_ = info;
            }

            // Call callbacks
            for (const auto& callback : callbacks_) {
                callback(frame, info);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

bool Camera::getFrame(cv::Mat& frame, FrameInfo& info) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (current_frame_.empty()) {
        return false;
    }
    frame = current_frame_.clone();
    info = current_info_;
    return true;
}

void Camera::registerCallback(FrameCallback callback) {
    callbacks_.push_back(callback);
}

} // namespace visual_homing
