/**
 * Visual Homing System - Camera Module
 * Захоплення відео з USB та CSI камер
 */

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

namespace visual_homing {

struct FrameInfo {
    double timestamp;
    uint64_t frame_number;
    int width;
    int height;
};

using FrameCallback = std::function<void(const cv::Mat&, const FrameInfo&)>;

/**
 * Camera class for video capture
 * Supports USB capture (EasyCap) and Pi Camera
 */
class Camera {
public:
    enum class Type {
        USB_CAPTURE,
        PI_CAMERA
    };

    Camera(Type type = Type::USB_CAPTURE,
           const std::string& device = "/dev/video0",
           int width = 720,
           int height = 576,
           int fps = 25);
    
    ~Camera();

    // Start/stop capture
    bool start();
    void stop();

    // Get latest frame
    bool getFrame(cv::Mat& frame, FrameInfo& info);

    // Register callback for new frames
    void registerCallback(FrameCallback callback);

    // Status
    bool isRunning() const { return running_; }
    uint64_t getFrameCount() const { return frame_count_; }

private:
    void captureLoop();

    Type type_;
    std::string device_;
    int width_, height_, fps_;

    cv::VideoCapture cap_;
    cv::Mat current_frame_;
    FrameInfo current_info_;

    std::thread capture_thread_;
    std::mutex frame_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> frame_count_{0};

    std::vector<FrameCallback> callbacks_;
};

} // namespace visual_homing

#endif // CAMERA_HPP
