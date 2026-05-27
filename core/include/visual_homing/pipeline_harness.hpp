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

struct PipelineResult {
    std::uint64_t frames_processed = 0;
    double last_frame_age_ms = 0.0;
    double last_processing_latency_ms = 0.0;
};

PipelineResult run_replay_pipeline(const PipelineConfig& config, std::ostream& metrics);

} // namespace vh
