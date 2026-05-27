#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>

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

struct PipelineResult {
    std::uint64_t frames_processed = 0;
    double last_frame_age_ms = 0.0;
    double last_processing_latency_ms = 0.0;
};

PipelineResult run_replay_pipeline(const PipelineConfig& config, std::ostream& metrics);
PipelineResult record_replay_route(const RouteRecordingConfig& config, std::ostream& metrics);

} // namespace vh
