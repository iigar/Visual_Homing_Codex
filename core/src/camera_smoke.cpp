#include "visual_homing/camera_smoke.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>

#include "visual_homing/gray8_resize_preprocessor.hpp"
#include "visual_homing/health_monitor.hpp"
#include "visual_homing/route_signature_recorder.hpp"
#include "visual_homing/time.hpp"

namespace vh {

CameraSmokeResult run_pi_camera_smoke(const CameraSmokeConfig& config, std::ostream& metrics) {
    if (config.frames_to_capture == 0) {
        throw std::invalid_argument("Camera smoke frames_to_capture must be positive");
    }
    if (config.target_width <= 0 || config.target_height <= 0) {
        throw std::invalid_argument("Camera smoke target dimensions must be positive");
    }

    PiCameraSource source(config.camera);
    Gray8ResizePreprocessor preprocessor(config.target_width, config.target_height);
    HealthMonitor health(now());
    health.set_links(true, false, true);
    CameraSmokeResult result;

    metrics << "camera_smoke_start width=" << config.camera.width
            << " height=" << config.camera.height
            << " fps=" << config.camera.frame_rate_hz
            << " target=" << config.target_width << "x" << config.target_height
            << " requested_frames=" << config.frames_to_capture << "\n";

    result.started = source.start();
    metrics << "camera_smoke_backend_start_result started=" << (result.started ? "true" : "false")
            << " running=" << (source.running() ? "true" : "false");
    if (!source.last_error().empty()) {
        metrics << " error=" << source.last_error();
    }
    metrics << "\n";

    if (!result.started) {
        metrics << "camera_smoke_unavailable error=" << source.last_error() << "\n";
        metrics << "camera_smoke_done started=false frames_captured=0 empty_polls=0\n";
        return result;
    }

    const auto started_at = now();
    const auto timeout_ms = 2000.0 + (static_cast<double>(config.frames_to_capture) * 1000.0
        / static_cast<double>(config.camera.frame_rate_hz));
    while (result.frames_captured < config.frames_to_capture) {
        if (auto frame = source.poll()) {
            const auto processing_started = now();
            const auto processed = preprocessor.process(*frame);
            const auto processing_finished = now();
            const auto timing = health.observe_processed_frame(processed, processing_started, processing_finished);
            const auto snapshot = health.snapshot(processing_finished);

            ++result.frames_captured;
            result.last_frame_age_ms = timing.frame_age_ms;
            result.last_processing_latency_ms = timing.processing_latency_ms;

            metrics << "camera_frame id=" << frame->id
                    << " size=" << frame->width << "x" << frame->height
                    << " bytes=" << frame->data.size()
                    << " processed=" << processed.width << "x" << processed.height
                    << " processed_bytes=" << processed.data.size()
                    << " age_ms=" << timing.frame_age_ms
                    << " latency_ms=" << timing.processing_latency_ms
                    << " frames_seen=" << snapshot.frames_seen << "\n";
        } else {
            ++result.empty_polls;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (milliseconds_between(started_at, now()) > timeout_ms) {
            break;
        }
    }

    source.stop();
    result.elapsed_ms = milliseconds_between(started_at, now());
    result.effective_fps = result.elapsed_ms > 0.0
        ? (static_cast<double>(result.frames_captured) * 1000.0 / result.elapsed_ms)
        : 0.0;
    metrics << "camera_smoke_done started=true"
            << " frames_captured=" << result.frames_captured
            << " empty_polls=" << result.empty_polls
            << " last_age_ms=" << result.last_frame_age_ms
            << " last_latency_ms=" << result.last_processing_latency_ms
            << " elapsed_ms=" << result.elapsed_ms
            << " effective_fps=" << result.effective_fps << "\n";
    return result;
}

LiveRouteRecordingResult record_live_camera_route(const LiveRouteRecordingConfig& config, std::ostream& metrics) {
    if (config.frames_to_capture == 0) {
        throw std::invalid_argument("Live route recording frames_to_capture must be positive");
    }
    if (config.target_width <= 0 || config.target_height <= 0) {
        throw std::invalid_argument("Live route recording target dimensions must be positive");
    }
    if (config.route_output_path.empty()) {
        throw std::invalid_argument("Live route recording output path must not be empty");
    }

    PiCameraSource source(config.camera);
    Gray8ResizePreprocessor preprocessor(config.target_width, config.target_height);
    RouteSignatureRecorder recorder;
    HealthMonitor health(now());
    health.set_links(true, false, true);
    LiveRouteRecordingResult result;
    std::optional<MavlinkTelemetryStream> telemetry_stream;

    metrics << "live_route_record_start width=" << config.camera.width
            << " height=" << config.camera.height
            << " fps=" << config.camera.frame_rate_hz
            << " target=" << config.target_width << "x" << config.target_height
            << " requested_frames=" << config.frames_to_capture
            << " warmup_frames=" << config.warmup_frames
            << " telemetry_snapshot=" << (config.use_telemetry_snapshot ? "true" : "false")
            << " live_telemetry_stream=" << (config.use_live_telemetry_stream ? "true" : "false")
            << " output=" << config.route_output_path.string() << "\n";

    if (config.use_telemetry_snapshot) {
        metrics << "live_route_telemetry_snapshot"
                << " heartbeat_seen=" << (config.telemetry_snapshot.heartbeat_seen ? "true" : "false")
                << " armed=" << (config.telemetry_snapshot.armed ? "true" : "false")
                << " relative_altitude_m=" << config.telemetry_snapshot.relative_altitude_m
                << " roll_rad=" << config.telemetry_snapshot.roll_rad
                << " pitch_rad=" << config.telemetry_snapshot.pitch_rad
                << " yaw_rad=" << config.telemetry_snapshot.yaw_rad << "\n";
        result.used_telemetry_snapshot = true;
    }

    if (config.use_live_telemetry_stream) {
        telemetry_stream.emplace(config.telemetry_stream);
        telemetry_stream->start();
        result.used_live_telemetry_stream = true;
        metrics << "live_route_telemetry_stream_start"
                << " device=" << config.telemetry_stream.device_path
                << " baud_rate=" << config.telemetry_stream.baud_rate
                << " started=true\n";
    }

    result.started = source.start();
    metrics << "live_route_backend_start_result started=" << (result.started ? "true" : "false")
            << " running=" << (source.running() ? "true" : "false");
    if (!source.last_error().empty()) {
        metrics << " error=" << source.last_error();
    }
    metrics << "\n";

    if (!result.started) {
        metrics << "live_route_unavailable error=" << source.last_error() << "\n";
        metrics << "live_route_record_done started=false warmup_frames_dropped=0 frames_captured=0 entries=0 empty_polls=0 route_written=false\n";
        return result;
    }

    const auto started_at = now();
    const auto requested_source_frames = config.frames_to_capture + config.warmup_frames;
    const auto timeout_ms = 2000.0 + (static_cast<double>(requested_source_frames) * 1000.0
        / static_cast<double>(config.camera.frame_rate_hz));
    while (result.warmup_frames_dropped < config.warmup_frames) {
        if (auto frame = source.poll()) {
            ++result.warmup_frames_dropped;
            metrics << "live_route_warmup_frame id=" << frame->id
                    << " size=" << frame->width << "x" << frame->height
                    << " bytes=" << frame->data.size()
                    << " dropped=" << result.warmup_frames_dropped
                    << "/" << config.warmup_frames << "\n";
        } else {
            ++result.empty_polls;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (milliseconds_between(started_at, now()) > timeout_ms) {
            break;
        }
    }

    while (result.frames_captured < config.frames_to_capture) {
        if (auto frame = source.poll()) {
            const auto processing_started = now();
            const auto processed = preprocessor.process(*frame);
            const auto processing_finished = now();
            const auto timing = health.observe_processed_frame(processed, processing_started, processing_finished);

            NavigationEstimate nav;
            nav.timestamp = processed.timestamp;
            nav.altitude_m = config.altitude_m;
            nav.course_error_rad = config.heading_hint_rad;
            if (config.use_telemetry_snapshot) {
                nav.altitude_m = config.telemetry_snapshot.relative_altitude_m;
                nav.course_error_rad = config.telemetry_snapshot.yaw_rad;
            }
            if (telemetry_stream) {
                const auto telemetry = telemetry_stream->snapshot();
                const auto validation = validate_mavlink_telemetry(telemetry.inspection, {});
                result.telemetry_bytes_captured = telemetry.bytes_captured;
                result.telemetry_frames_seen = telemetry.inspection.frames_seen;
                result.telemetry_heartbeat_messages = telemetry.inspection.heartbeat_messages;
                result.telemetry_attitude_messages = telemetry.inspection.attitude_messages;
                result.telemetry_global_position_int_messages = telemetry.inspection.global_position_int_messages;
                if (validation.passed) {
                    nav.altitude_m = telemetry.inspection.latest.relative_altitude_m;
                    nav.course_error_rad = telemetry.inspection.latest.yaw_rad;
                }
            }
            nav.confidence = 1.0;
            recorder.observe(processed, nav);

            ++result.frames_captured;
            result.route_entries = static_cast<std::uint64_t>(recorder.route().entries.size());
            result.last_frame_age_ms = timing.frame_age_ms;
            result.last_processing_latency_ms = timing.processing_latency_ms;

            metrics << "live_route_frame id=" << processed.id
                    << " size=" << processed.width << "x" << processed.height
                    << " bytes=" << processed.data.size()
                    << " age_ms=" << timing.frame_age_ms
                    << " latency_ms=" << timing.processing_latency_ms
                    << " entries=" << result.route_entries << "\n";
        } else {
            ++result.empty_polls;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (milliseconds_between(started_at, now()) > timeout_ms) {
            break;
        }
    }

    source.stop();
    if (telemetry_stream) {
        telemetry_stream->stop();
        const auto telemetry = telemetry_stream->snapshot();
        result.telemetry_bytes_captured = telemetry.bytes_captured;
        result.telemetry_frames_seen = telemetry.inspection.frames_seen;
        result.telemetry_heartbeat_messages = telemetry.inspection.heartbeat_messages;
        result.telemetry_attitude_messages = telemetry.inspection.attitude_messages;
        result.telemetry_global_position_int_messages = telemetry.inspection.global_position_int_messages;
        metrics << "live_route_telemetry_stream_done"
                << " supported=" << (telemetry.supported ? "true" : "false")
                << " opened=" << (telemetry.opened ? "true" : "false")
                << " bytes_captured=" << telemetry.bytes_captured
                << " frames_seen=" << telemetry.inspection.frames_seen
                << " heartbeat_messages=" << telemetry.inspection.heartbeat_messages
                << " attitude_messages=" << telemetry.inspection.attitude_messages
                << " global_position_int_messages=" << telemetry.inspection.global_position_int_messages
                << " malformed_frames=" << telemetry.inspection.malformed_frames
                << " mode=" << to_string(telemetry.inspection.latest.mode)
                << " relative_altitude_m=" << telemetry.inspection.latest.relative_altitude_m;
        if (!telemetry_stream->last_error().empty()) {
            metrics << " error=" << telemetry_stream->last_error();
        }
        metrics << "\n";
    }
    result.elapsed_ms = milliseconds_between(started_at, now());
    result.effective_fps = result.elapsed_ms > 0.0
        ? (static_cast<double>(result.frames_captured) * 1000.0 / result.elapsed_ms)
        : 0.0;

    if (!recorder.route().entries.empty()) {
        recorder.write_to(config.route_output_path);
        result.route_written = true;
        result.route_entries = static_cast<std::uint64_t>(recorder.route().entries.size());
    }

    metrics << "live_route_record_done started=true"
            << " warmup_frames_dropped=" << result.warmup_frames_dropped
            << " frames_captured=" << result.frames_captured
            << " entries=" << result.route_entries
            << " empty_polls=" << result.empty_polls
            << " route_written=" << (result.route_written ? "true" : "false")
            << " telemetry_snapshot=" << (result.used_telemetry_snapshot ? "true" : "false")
            << " live_telemetry_stream=" << (result.used_live_telemetry_stream ? "true" : "false")
            << " telemetry_bytes_captured=" << result.telemetry_bytes_captured
            << " telemetry_frames_seen=" << result.telemetry_frames_seen
            << " last_age_ms=" << result.last_frame_age_ms
            << " last_latency_ms=" << result.last_processing_latency_ms
            << " elapsed_ms=" << result.elapsed_ms
            << " effective_fps=" << result.effective_fps << "\n";
    return result;
}

} // namespace vh
