#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>

#include "visual_homing/mavlink.hpp"

namespace vh {

struct PipelineConfig {
    std::filesystem::path manifest_path;
    int target_width = 32;
    int target_height = 24;
};

struct RouteRecordingConfig {
    std::filesystem::path manifest_path;
    std::filesystem::path route_output_path;
    int target_width = 32;
    int target_height = 24;
    double altitude_m = 0.0;
    double heading_hint_rad = 0.0;
};

struct RouteMatchingConfig {
    std::filesystem::path route_path;
    std::filesystem::path manifest_path;
    int target_width = 32;
    int target_height = 24;
    std::size_t window_radius = 0;
    double minimum_confidence = 0.0;
    int max_direction_shift_px = 2;
    double radians_per_pixel = 0.02;
    double navigator_minimum_confidence = 0.70;
    double navigator_max_match_age_ms = 1.0e12;
    double navigator_yaw_gain = 1.0;
    double navigator_max_yaw_rate_radps = 0.35;
    double navigator_max_yaw_accel_radps2 = 1.0;
    double navigator_forward_speed_mps = 0.0;
    bool dry_run_mavlink_armed = true;
    FlightMode dry_run_mavlink_mode = FlightMode::Guided;
};

struct PipelineResult {
    std::uint64_t frames_processed = 0;
    double last_frame_age_ms = 0.0;
    double last_processing_latency_ms = 0.0;
};

PipelineResult run_replay_pipeline(const PipelineConfig& config, std::ostream& metrics);
PipelineResult record_replay_route(const RouteRecordingConfig& config, std::ostream& metrics);
PipelineResult match_replay_route(const RouteMatchingConfig& config, std::ostream& metrics);

} // namespace vh
