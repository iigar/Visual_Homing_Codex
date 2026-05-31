#include "visual_homing/pipeline_harness.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"
#include "visual_homing/gray8_resize_preprocessor.hpp"
#include "visual_homing/bounded_navigator.hpp"
#include "visual_homing/dry_run_command_sink.hpp"
#include "visual_homing/dry_run_mavlink_bridge.hpp"
#include "visual_homing/health_monitor.hpp"
#include "visual_homing/mavlink_telemetry_adapter.hpp"
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

PipelineResult match_replay_route(const RouteMatchingConfig& config, std::ostream& metrics) {
    if (config.target_width <= 0 || config.target_height <= 0) {
        throw std::invalid_argument("Route matching target dimensions must be positive");
    }

    double radians_per_pixel = config.radians_per_pixel;
    if (config.camera_profile) {
        const auto scale = derive_camera_angular_scale(*config.camera_profile);
        radians_per_pixel = scale.radians_per_target_pixel_x;
    }

    auto route = read_route_signature_file(config.route_path);
    Gray8RouteMatcher matcher(std::move(route), {
        .window_radius = config.window_radius,
        .minimum_confidence = config.minimum_confidence,
        .max_direction_shift_px = config.max_direction_shift_px,
        .radians_per_pixel = radians_per_pixel,
    });
    BoundedNavigator navigator({
        .minimum_confidence = config.navigator_minimum_confidence,
        .max_match_age_ms = config.navigator_max_match_age_ms,
        .yaw_gain = config.navigator_yaw_gain,
        .max_yaw_rate_radps = config.navigator_max_yaw_rate_radps,
        .max_yaw_accel_radps2 = config.navigator_max_yaw_accel_radps2,
        .forward_speed_mps = config.navigator_forward_speed_mps,
    });
    auto replay = ReplayFrameSource::load_manifest(config.manifest_path);
    std::vector<MavlinkTelemetry> telemetry_script(replay.size());
    for (auto& telemetry : telemetry_script) {
        telemetry.heartbeat_seen = true;
        telemetry.armed = config.dry_run_mavlink_armed;
        telemetry.mode = config.dry_run_mavlink_mode;
    }
    Gray8ResizePreprocessor preprocessor(config.target_width, config.target_height);
    HealthMonitor health(now());
    health.set_links(true, false, true);
    DryRunCommandSink command_sink(&metrics);
    DryRunMavlinkBridge mavlink_bridge(std::move(telemetry_script), &metrics);
    MavlinkTelemetryAdapter telemetry_adapter({.max_telemetry_age_ms = 1.0e12, .navigation_confidence = 1.0});
    command_sink.start();
    mavlink_bridge.start();

    replay.start();

    PipelineResult result;
    metrics << "route_match_start frames_available=" << replay.size()
            << " route=" << config.route_path.string()
            << " target=" << config.target_width << "x" << config.target_height
            << " window_radius=" << config.window_radius
            << " minimum_confidence=" << config.minimum_confidence
            << " radians_per_pixel=" << radians_per_pixel;
    if (config.camera_profile) {
        metrics << " camera_profile=" << config.camera_profile->id
                << " profile_target=" << config.camera_profile->target_width << "x" << config.camera_profile->target_height
                << " horizontal_fov_rad=" << config.camera_profile->horizontal_fov_rad
                << " vertical_fov_rad=" << config.camera_profile->vertical_fov_rad;
    }
    metrics << "\n";

    while (const auto frame = replay.poll()) {
        const auto processing_started = now();
        const auto processed = preprocessor.process(*frame);
        const auto match = matcher.match(processed);
        const auto processing_finished = now();
        if (auto telemetry = mavlink_bridge.poll_telemetry()) {
            telemetry_adapter.observe(*telemetry, processing_finished);
        }
        const auto timing = health.observe_processed_frame(processed, processing_started, processing_finished);
        health.set_route_match_confidence(match.confidence);
        telemetry_adapter.apply_to_health(health, processing_finished, true, true);
        auto snapshot = health.snapshot(processing_finished);
        const auto command = navigator.update(match, snapshot);
        command_sink.send(command);

        ++result.frames_processed;
        result.last_frame_age_ms = timing.frame_age_ms;
        result.last_processing_latency_ms = timing.processing_latency_ms;

        metrics << "match_frame id=" << processed.id
                << " route_index=" << match.route_index
                << " progress=" << match.progress
                << " direction_error_rad=" << match.direction_error_rad
                << " confidence=" << match.confidence
                << " valid=" << (match.valid ? "true" : "false")
                << " mavlink_ok=" << (snapshot.mavlink_ok ? "true" : "false")
                << " navigation_ok=" << (snapshot.navigation_ok ? "true" : "false")
                << " command_valid=" << (command.valid ? "true" : "false")
                << " yaw_rate_radps=" << command.yaw_rate_radps
                << " latency_ms=" << timing.processing_latency_ms
                << "\n";
    }

    replay.stop();
    mavlink_bridge.stop();
    command_sink.stop();
    metrics << "route_match_done frames_processed=" << result.frames_processed << "\n";

    return result;
}

} // namespace vh
