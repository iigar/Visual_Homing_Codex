#include "visual_homing/pipeline_harness.hpp"

#include <iostream>
#include <stdexcept>

#include "visual_homing/gray8_resize_preprocessor.hpp"
#include "visual_homing/health_monitor.hpp"
#include "visual_homing/replay_frame_source.hpp"
#include "visual_homing/route_signature_recorder.hpp"
#include "visual_homing/time.hpp"

namespace vh {

PipelineResult run_replay_pipeline(const PipelineConfig& config, std::ostream& metrics) {
    if (config.target_width <= 0 || config.target_height <= 0) {
        throw std::invalid_argument("Pipeline target dimensions must be positive");
    }

    auto replay = ReplayFrameSource::load_manifest(config.manifest_path);
    Gray8ResizePreprocessor preprocessor(config.target_width, config.target_height);
    HealthMonitor health(now());
    health.set_links(true, false, true);

    replay.start();

    PipelineResult result;
    metrics << "pipeline_start frames_available=" << replay.size()
            << " target=" << config.target_width << "x" << config.target_height << "\n";

    while (const auto frame = replay.poll()) {
        const auto processing_started = now();
        const auto processed = preprocessor.process(*frame);
        const auto processing_finished = now();
        const auto timing = health.observe_processed_frame(processed, processing_started, processing_finished);
        const auto snapshot = health.snapshot(processing_finished);

        ++result.frames_processed;
        result.last_frame_age_ms = timing.frame_age_ms;
        result.last_processing_latency_ms = timing.processing_latency_ms;

        metrics << "frame id=" << processed.id
                << " size=" << processed.width << "x" << processed.height
                << " age_ms=" << timing.frame_age_ms
                << " latency_ms=" << timing.processing_latency_ms
                << " frames_seen=" << snapshot.frames_seen
                << " state=" << (snapshot.state == HealthState::Ready ? "ready" : "degraded")
                << "\n";
    }

    replay.stop();
    metrics << "pipeline_done frames_processed=" << result.frames_processed
            << " last_age_ms=" << result.last_frame_age_ms
            << " last_latency_ms=" << result.last_processing_latency_ms << "\n";

    return result;
}

PipelineResult record_replay_route(const RouteRecordingConfig& config, std::ostream& metrics) {
    if (config.target_width <= 0 || config.target_height <= 0) {
        throw std::invalid_argument("Route recording target dimensions must be positive");
    }
    if (config.route_output_path.empty()) {
        throw std::invalid_argument("Route recording output path must not be empty");
    }

    auto replay = ReplayFrameSource::load_manifest(config.manifest_path);
    Gray8ResizePreprocessor preprocessor(config.target_width, config.target_height);
    RouteSignatureRecorder recorder;
    HealthMonitor health(now());
    health.set_links(true, false, true);

    replay.start();

    PipelineResult result;
    metrics << "route_record_start frames_available=" << replay.size()
            << " target=" << config.target_width << "x" << config.target_height
            << " output=" << config.route_output_path.string() << "\n";

    while (const auto frame = replay.poll()) {
        const auto processing_started = now();
        const auto processed = preprocessor.process(*frame);
        const auto processing_finished = now();
        const auto timing = health.observe_processed_frame(processed, processing_started, processing_finished);

        NavigationEstimate nav;
        nav.timestamp = processed.timestamp;
        nav.altitude_m = config.altitude_m;
        nav.course_error_rad = config.heading_hint_rad;
        nav.confidence = 1.0;
        recorder.observe(processed, nav);

        ++result.frames_processed;
        result.last_frame_age_ms = timing.frame_age_ms;
        result.last_processing_latency_ms = timing.processing_latency_ms;

        metrics << "route_frame id=" << processed.id
                << " size=" << processed.width << "x" << processed.height
                << " age_ms=" << timing.frame_age_ms
                << " latency_ms=" << timing.processing_latency_ms
                << "\n";
    }

    replay.stop();
    recorder.write_to(config.route_output_path);

    metrics << "route_record_done frames_processed=" << result.frames_processed
            << " entries=" << recorder.route().entries.size() << "\n";

    return result;
}

} // namespace vh
