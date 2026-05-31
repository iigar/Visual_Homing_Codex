#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>

#include "visual_homing/pi_camera_source.hpp"

namespace vh {

struct CameraSmokeConfig {
    PiCameraConfig camera;
    int target_width = 32;
    int target_height = 24;
    std::size_t frames_to_capture = 30;
};

struct CameraSmokeResult {
    bool started = false;
    std::uint64_t frames_captured = 0;
    std::uint64_t empty_polls = 0;
    double last_frame_age_ms = 0.0;
    double last_processing_latency_ms = 0.0;
    double elapsed_ms = 0.0;
    double effective_fps = 0.0;
};

struct LiveRouteRecordingConfig {
    PiCameraConfig camera;
    std::filesystem::path route_output_path;
    int target_width = 32;
    int target_height = 24;
    std::size_t frames_to_capture = 120;
    double altitude_m = 0.0;
    double heading_hint_rad = 0.0;
};

struct LiveRouteRecordingResult {
    bool started = false;
    bool route_written = false;
    std::uint64_t frames_captured = 0;
    std::uint64_t route_entries = 0;
    std::uint64_t empty_polls = 0;
    double last_frame_age_ms = 0.0;
    double last_processing_latency_ms = 0.0;
    double elapsed_ms = 0.0;
    double effective_fps = 0.0;
};

CameraSmokeResult run_pi_camera_smoke(const CameraSmokeConfig& config, std::ostream& metrics);
LiveRouteRecordingResult record_live_camera_route(const LiveRouteRecordingConfig& config, std::ostream& metrics);

} // namespace vh
