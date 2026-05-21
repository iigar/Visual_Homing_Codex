/**
 * Visual Homing System v2.1 - Main Entry Point (C++ version)
 * Supports: Camera (USB/CSI), MATEK 3901-L0X, TF-Luna, Smart RTL
 */

#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

#include "camera.hpp"
#include "feature_tracker.hpp"
#include "visual_odometry.hpp"
#include "route_memory.hpp"
#include "mavlink_interface.hpp"
#include "optical_flow.hpp"
#include "lidar.hpp"
#include "smart_rtl.hpp"

using namespace visual_homing;

std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\nShutdown signal received" << std::endl;
    g_running = false;
}

int main(int argc, char** argv) {
    std::cout << "=====================================" << std::endl;
    std::cout << "  Visual Homing System v2.1 (C++)" << std::endl;
    std::cout << "  Sensors: 3901-L0X + TF-Luna" << std::endl;
    std::cout << "  Smart RTL: Enabled" << std::endl;
    std::cout << "=====================================" << std::endl;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Parse arguments
    std::string camera_device = "/dev/video0";
    std::string mavlink_port = "/dev/serial0";
    std::string flow_port = "/dev/serial1";
    std::string lidar_port = "/dev/serial2";
    bool test_mode = false;
    bool enable_flow = true;
    bool enable_lidar = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--test-mode") test_mode = true;
        else if (arg == "--camera" && i + 1 < argc) camera_device = argv[++i];
        else if (arg == "--port" && i + 1 < argc) mavlink_port = argv[++i];
        else if (arg == "--flow-port" && i + 1 < argc) flow_port = argv[++i];
        else if (arg == "--lidar-port" && i + 1 < argc) lidar_port = argv[++i];
        else if (arg == "--no-flow") enable_flow = false;
        else if (arg == "--no-lidar") enable_lidar = false;
    }

    // Initialize components
    Camera camera(Camera::Type::USB_CAPTURE, camera_device);
    VisualOdometry vo(500);
    RouteMemory route_memory;
    MAVLinkInterface mavlink(mavlink_port);
    OpticalFlowSensor flowSensor(flow_port);
    LidarSensor lidarSensor(lidar_port);
    SmartRTL smartRtl;

    // Start camera
    if (!camera.start()) {
        std::cerr << "Failed to start camera" << std::endl;
        return 1;
    }

    // Connect MAVLink
    if (!test_mode) {
        if (!mavlink.connect()) {
            std::cerr << "MAVLink connection failed - running without FC" << std::endl;
        }
    }

    // Connect sensors
    if (enable_flow) {
        if (flowSensor.connect()) {
            std::cout << "Optical Flow: OK" << std::endl;
        } else {
            std::cerr << "Optical Flow: FAILED" << std::endl;
        }
    }

    if (enable_lidar) {
        if (lidarSensor.connect()) {
            std::cout << "TF-Luna LiDAR: OK" << std::endl;
        } else {
            std::cerr << "TF-Luna LiDAR: FAILED" << std::endl;
        }
    }

    std::cout << "\nSystem started. Press Ctrl+C to stop.\n" << std::endl;

    // Main loop
    cv::Mat frame;
    FrameInfo frame_info;
    int frame_count = 0;

    while (g_running) {
        if (camera.getFrame(frame, frame_info)) {
            frame_count++;

            // Get altitude from MAVLink or LiDAR
            double altitude = 1.0;
            if (mavlink.isConnected()) {
                altitude = mavlink.getAltitude();
            }

            // Use LiDAR for precise low-altitude measurement
            auto lidarData = lidarSensor.getLatest();
            if (lidarData.isValid() && lidarData.distanceM() < 8.0f) {
                altitude = lidarData.distanceM();
            }
            if (altitude < 0.5) altitude = 0.5;

            // Get optical flow data
            auto flowData = flowSensor.getLatest();

            // Update visual odometry
            vo.setAltitude(altitude);
            auto [pose, velocity] = vo.processFrame(frame, frame_info.timestamp);

            if (pose) {
                // Send to ArduPilot
                if (mavlink.isConnected()) {
                    mavlink.sendVisionPosition(
                        pose->x, pose->y, altitude,
                        0, 0, pose->yaw,
                        pose->confidence
                    );

                    if (velocity) {
                        mavlink.sendVisionSpeed(
                            velocity->vx, velocity->vy, velocity->vz
                        );
                    }
                }

                // Update Smart RTL if active
                if (smartRtl.isActive()) {
                    float homeDistance = std::sqrt(pose->x * pose->x + pose->y * pose->y);
                    smartRtl.update(altitude, homeDistance, flowData.quality, pose->confidence);

                    auto cmd = smartRtl.getVelocityCommand();
                    // Apply velocity command to FC (via MAVLink SET_POSITION_TARGET_LOCAL_NED)
                }

                // Status output every 100 frames
                if (frame_count % 100 == 0) {
                    std::cout << "Pos: (" << pose->x << ", " << pose->y << ") "
                              << "Alt: " << altitude << "m "
                              << "Yaw: " << (pose->yaw * 180 / 3.14159) << "deg "
                              << "Flow: q=" << flowData.quality
                              << " LiDAR: " << lidarData.distanceM() << "m";
                    
                    if (smartRtl.isActive()) {
                        std::cout << " RTL: " << smartRtl.navSource()
                                  << " " << (smartRtl.progress() * 100) << "%";
                    }
                    std::cout << std::endl;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Cleanup
    std::cout << "\nShutting down..." << std::endl;
    camera.stop();
    flowSensor.disconnect();
    lidarSensor.disconnect();
    mavlink.disconnect();

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
