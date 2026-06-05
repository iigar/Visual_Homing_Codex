#include "visual_homing/camera_smoke.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <thread>

#include "visual_homing/bounded_navigator.hpp"
#include "visual_homing/dry_run_command_sink.hpp"
#include "visual_homing/gray8_resize_preprocessor.hpp"
#include "visual_homing/gray8_route_matcher.hpp"
#include "visual_homing/health_monitor.hpp"
#include "visual_homing/live_mavlink_output_audit_log.hpp"
#include "visual_homing/live_mavlink_output_session.hpp"
#include "visual_homing/live_mavlink_output_safety_gate.hpp"
#include "visual_homing/mavlink_telemetry_adapter.hpp"
#include "visual_homing/route_signature.hpp"
#include "visual_homing/route_signature_recorder.hpp"
#include "visual_homing/time.hpp"

namespace vh {

namespace {

std::string wall_time_utc_iso8601() {
    const auto current = std::chrono::system_clock::now();
    const auto current_time = std::chrono::system_clock::to_time_t(current);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &current_time);
#else
    gmtime_r(&current_time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

void emit_bells(std::ostream& metrics, int count) {
    for (int index = 0; index < count; ++index) {
        metrics << '\a';
    }
    metrics.flush();
}

void emit_operator_start_cue(bool enabled,
                             std::size_t countdown_seconds,
                             bool bell_enabled,
                             const std::string& phase,
                             const std::string& details,
                             std::ostream& metrics) {
    if (!enabled) {
        return;
    }

    metrics << "\n"
            << "###############################################################################\n"
            << "###############################################################################\n"
            << "### VISUAL_HOMING_OPERATOR_CUE phase=" << phase << "\n"
            << "### " << details << "\n"
            << "### Camera backend is running and warmup frames are already dropped.\n"
            << "###############################################################################\n"
            << "operator_cue phase=" << phase
            << " details=\"" << details << "\""
            << " countdown_s=" << countdown_seconds
            << " wall_time_utc=" << wall_time_utc_iso8601() << "\n";

    auto remaining = countdown_seconds;
    while (remaining > 0) {
        if (bell_enabled) {
            emit_bells(metrics, 1);
        }
        metrics << "operator_cue_countdown phase=" << phase
                << " starts_in_s=" << remaining << "\n";
        metrics.flush();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        --remaining;
    }

    if (bell_enabled) {
        emit_bells(metrics, 4);
    }
    metrics << "###############################################################################\n"
            << "###############################################################################\n"
            << "### >>> START NOW <<< START NOW <<< START NOW <<<\n"
            << "### >>> LIVE CAPTURE BEGINS ON THE NEXT FRAME <<<\n"
            << "### >>> START NOW <<< START NOW <<< START NOW <<<\n"
            << "###############################################################################\n"
            << "operator_cue_go phase=" << phase
            << " wall_time_utc=" << wall_time_utc_iso8601() << "\n"
            << "###############################################################################\n\n";
    metrics.flush();
}

void emit_operator_stop_cue(bool enabled,
                            bool bell_enabled,
                            const std::string& phase,
                            std::ostream& metrics) {
    if (!enabled) {
        return;
    }

    if (bell_enabled) {
        emit_bells(metrics, 3);
    }
    metrics << "\n"
            << "###############################################################################\n"
            << "###############################################################################\n"
            << "### >>> STOP NOW <<< STOP NOW <<< STOP NOW <<<\n"
            << "### >>> LIVE CAPTURE COMPLETE <<<\n"
            << "### >>> STOP NOW <<< STOP NOW <<< STOP NOW <<<\n"
            << "###############################################################################\n"
            << "operator_cue_stop phase=" << phase
            << " wall_time_utc=" << wall_time_utc_iso8601() << "\n"
            << "###############################################################################\n\n";
    metrics.flush();
}

std::uint64_t drain_pending_frames(PiCameraSource& source) {
    std::uint64_t drained = 0;
    while (source.poll()) {
        ++drained;
    }
    return drained;
}

void copy_telemetry_stream_metrics(
    LiveRouteRecordingResult& result,
    const MavlinkTelemetryStreamSnapshot& telemetry) {
    result.telemetry_bytes_captured = telemetry.bytes_captured;
    result.telemetry_bytes_retained = telemetry.bytes_retained;
    result.telemetry_bytes_dropped = telemetry.bytes_dropped;
    result.telemetry_frames_seen = telemetry.inspection.frames_seen;
    result.telemetry_heartbeat_messages = telemetry.inspection.heartbeat_messages;
    result.telemetry_attitude_messages = telemetry.inspection.attitude_messages;
    result.telemetry_global_position_int_messages = telemetry.inspection.global_position_int_messages;
}

void copy_telemetry_stream_metrics(
    LiveRouteMatchingResult& result,
    const MavlinkTelemetryStreamSnapshot& telemetry) {
    result.telemetry_bytes_captured = telemetry.bytes_captured;
    result.telemetry_bytes_retained = telemetry.bytes_retained;
    result.telemetry_bytes_dropped = telemetry.bytes_dropped;
    result.telemetry_frames_seen = telemetry.inspection.frames_seen;
    result.telemetry_heartbeat_messages = telemetry.inspection.heartbeat_messages;
    result.telemetry_attitude_messages = telemetry.inspection.attitude_messages;
    result.telemetry_global_position_int_messages = telemetry.inspection.global_position_int_messages;
}

LiveMavlinkOutputSafetyConfig live_output_gate_config_from_match_config(
    const LiveRouteMatchingConfig& config,
    bool dry_run_quality_passed) {
    LiveMavlinkOutputSafetyConfig gate_config;
    gate_config.runtime_enabled = true;
    gate_config.operator_confirmed = true;
    gate_config.dry_run_quality_passed = dry_run_quality_passed;
    gate_config.audit_log_enabled = true;
    gate_config.audit_log_ready = true;
    gate_config.single_writer = true;
    gate_config.max_telemetry_age_ms = config.telemetry_max_age_ms;
    gate_config.min_match_confidence = config.minimum_confidence;
    gate_config.max_match_age_ms = config.navigator.max_match_age_ms;
    gate_config.max_abs_yaw_rate_radps = config.max_abs_dry_run_yaw_rate_radps;
    gate_config.max_abs_forward_speed_mps = std::max(0.0, std::abs(config.navigator.forward_speed_mps));
    return gate_config;
}

LiveMavlinkOutputSafetySnapshot live_output_gate_snapshot(
    Timestamp timestamp,
    const MavlinkTelemetry& telemetry,
    const RouteMatch& match,
    const NavigationCommand& command) {
    LiveMavlinkOutputSafetySnapshot snapshot;
    snapshot.now = timestamp;
    snapshot.telemetry = telemetry;
    snapshot.match = match;
    snapshot.command = command;
    return snapshot;
}

std::string format_reason_counts(const std::map<std::string, std::uint64_t>& reason_counts) {
    if (reason_counts.empty()) {
        return "none";
    }

    std::ostringstream output;
    bool first = true;
    for (const auto& [reason, count] : reason_counts) {
        if (!first) {
            output << ",";
        }
        first = false;
        output << reason << ":" << count;
    }
    return output.str();
}

const char* bool_word(const bool value) {
    return value ? "true" : "false";
}

} // namespace

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
    std::optional<MavlinkTelemetry> last_valid_live_telemetry;

    metrics << "live_route_record_start width=" << config.camera.width
            << " height=" << config.camera.height
            << " fps=" << config.camera.frame_rate_hz
            << " target=" << config.target_width << "x" << config.target_height
            << " requested_frames=" << config.frames_to_capture
            << " warmup_frames=" << config.warmup_frames
            << " telemetry_snapshot=" << (config.use_telemetry_snapshot ? "true" : "false")
            << " live_telemetry_stream=" << (config.use_live_telemetry_stream ? "true" : "false")
            << " telemetry_warmup_timeout_ms=" << config.telemetry_warmup_timeout_ms
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
        const auto telemetry_warmup_started = now();
        while (milliseconds_between(telemetry_warmup_started, now()) <
               static_cast<double>(config.telemetry_warmup_timeout_ms)) {
            const auto telemetry = telemetry_stream->snapshot();
            const auto validation = validate_mavlink_telemetry(telemetry.inspection, {});
            copy_telemetry_stream_metrics(result, telemetry);
            if (validation.passed) {
                result.telemetry_warmup_passed = true;
                last_valid_live_telemetry = telemetry.inspection.latest;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        result.telemetry_warmup_elapsed_ms = milliseconds_between(telemetry_warmup_started, now());
        metrics << "live_route_telemetry_warmup"
                << " timeout_ms=" << config.telemetry_warmup_timeout_ms
                << " elapsed_ms=" << result.telemetry_warmup_elapsed_ms
                << " passed=" << (result.telemetry_warmup_passed ? "true" : "false")
                << " bytes_captured=" << result.telemetry_bytes_captured
                << " bytes_retained=" << result.telemetry_bytes_retained
                << " bytes_dropped=" << result.telemetry_bytes_dropped
                << " frames_seen=" << result.telemetry_frames_seen
                << " heartbeat_messages=" << result.telemetry_heartbeat_messages
                << " attitude_messages=" << result.telemetry_attitude_messages
                << " global_position_int_messages=" << result.telemetry_global_position_int_messages;
        if (!telemetry_stream->last_error().empty()) {
            metrics << " error=" << telemetry_stream->last_error();
        }
        metrics << "\n";
        if (!result.telemetry_warmup_passed) {
            telemetry_stream->stop();
            metrics << "live_route_record_done started=false warmup_frames_dropped=0 frames_captured=0 entries=0 empty_polls=0 route_written=false"
                    << " telemetry_snapshot=" << (result.used_telemetry_snapshot ? "true" : "false")
                    << " live_telemetry_stream=true"
                    << " telemetry_warmup_passed=false"
                    << " telemetry_bytes_captured=" << result.telemetry_bytes_captured
                    << " telemetry_bytes_retained=" << result.telemetry_bytes_retained
                    << " telemetry_bytes_dropped=" << result.telemetry_bytes_dropped
                    << " telemetry_frames_seen=" << result.telemetry_frames_seen << "\n";
            return result;
        }
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

    const auto warmup_started_at = now();
    const auto requested_source_frames = config.frames_to_capture + config.warmup_frames;
    const auto warmup_timeout_ms = 2000.0 + (static_cast<double>(requested_source_frames) * 1000.0
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

        if (milliseconds_between(warmup_started_at, now()) > warmup_timeout_ms) {
            break;
        }
    }

    emit_operator_start_cue(config.operator_cue_enabled,
                            config.operator_cue_seconds,
                            config.operator_cue_bell,
                            "record_live_route",
                            "Route recording starts on the next captured frame; route_output=" + config.route_output_path.string(),
                            metrics);
    if (config.operator_cue_enabled) {
        metrics << "live_route_pre_capture_frame_drain drained_frames=" << drain_pending_frames(source) << "\n";
    }

    const auto capture_started_at = now();
    const auto capture_timeout_ms = 2000.0 + (static_cast<double>(config.frames_to_capture) * 1000.0
        / static_cast<double>(config.camera.frame_rate_hz));
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
                copy_telemetry_stream_metrics(result, telemetry);
                if (validation.passed) {
                    last_valid_live_telemetry = telemetry.inspection.latest;
                }
                if (last_valid_live_telemetry) {
                    nav.altitude_m = last_valid_live_telemetry->relative_altitude_m;
                    nav.course_error_rad = last_valid_live_telemetry->yaw_rad;
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

        if (milliseconds_between(capture_started_at, now()) > capture_timeout_ms) {
            break;
        }
    }

    source.stop();
    emit_operator_stop_cue(config.operator_cue_enabled,
                           config.operator_cue_bell,
                           "record_live_route",
                           metrics);
    if (telemetry_stream) {
        telemetry_stream->stop();
        const auto telemetry = telemetry_stream->snapshot();
        copy_telemetry_stream_metrics(result, telemetry);
        metrics << "live_route_telemetry_stream_done"
                << " supported=" << (telemetry.supported ? "true" : "false")
                << " opened=" << (telemetry.opened ? "true" : "false")
                << " bytes_captured=" << telemetry.bytes_captured
                << " bytes_retained=" << telemetry.bytes_retained
                << " bytes_dropped=" << telemetry.bytes_dropped
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
    result.elapsed_ms = milliseconds_between(capture_started_at, now());
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
            << " telemetry_warmup_passed=" << (result.telemetry_warmup_passed ? "true" : "false")
            << " telemetry_bytes_captured=" << result.telemetry_bytes_captured
            << " telemetry_bytes_retained=" << result.telemetry_bytes_retained
            << " telemetry_bytes_dropped=" << result.telemetry_bytes_dropped
            << " telemetry_frames_seen=" << result.telemetry_frames_seen
            << " last_age_ms=" << result.last_frame_age_ms
            << " last_latency_ms=" << result.last_processing_latency_ms
            << " elapsed_ms=" << result.elapsed_ms
            << " effective_fps=" << result.effective_fps << "\n";
    return result;
}

LiveRouteMatchingResult match_live_camera_route(const LiveRouteMatchingConfig& config, std::ostream& metrics) {
    if (config.frames_to_capture == 0) {
        throw std::invalid_argument("Live route matching frames_to_capture must be positive");
    }
    if (config.target_width <= 0 || config.target_height <= 0) {
        throw std::invalid_argument("Live route matching target dimensions must be positive");
    }
    if (config.route_path.empty()) {
        throw std::invalid_argument("Live route matching route path must not be empty");
    }
    if (config.max_progress_rollback < 0.0) {
        throw std::invalid_argument("Live route matching max_progress_rollback must not be negative");
    }
    if (config.expected_progress != "any" && config.expected_progress != "forward" && config.expected_progress != "reverse") {
        throw std::invalid_argument("Live route matching expected_progress must be one of: any, forward, reverse");
    }
    if (config.endpoint_start_progress < 0.0 || config.endpoint_start_progress > 1.0) {
        throw std::invalid_argument("Live route matching endpoint_start_progress must be in [0, 1]");
    }
    if (config.endpoint_end_progress < 0.0 || config.endpoint_end_progress > 1.0) {
        throw std::invalid_argument("Live route matching endpoint_end_progress must be in [0, 1]");
    }
    if (config.endpoint_start_progress >= config.endpoint_end_progress) {
        throw std::invalid_argument("Live route matching endpoint_start_progress must be less than endpoint_end_progress");
    }
    if (config.minimum_valid_dry_run_command_fraction < 0.0 || config.minimum_valid_dry_run_command_fraction > 1.0) {
        throw std::invalid_argument("Live route matching minimum_valid_dry_run_command_fraction must be in [0, 1]");
    }
    if (config.max_abs_dry_run_yaw_rate_radps < 0.0) {
        throw std::invalid_argument("Live route matching max_abs_dry_run_yaw_rate_radps must be non-negative");
    }
    if (config.max_dry_run_yaw_rate_delta_radps < 0.0) {
        throw std::invalid_argument("Live route matching max_dry_run_yaw_rate_delta_radps must be non-negative");
    }
    if (config.telemetry_warmup_timeout_ms == 0 && config.use_live_telemetry_stream) {
        throw std::invalid_argument("Live route matching telemetry_warmup_timeout_ms must be positive when telemetry stream is enabled");
    }
    if (config.telemetry_max_age_ms < 0.0) {
        throw std::invalid_argument("Live route matching telemetry_max_age_ms must be non-negative");
    }
    if (config.emit_live_output_session_audit) {
        if (!config.emit_dry_run_commands) {
            throw std::invalid_argument("Live route matching session audit requires dry-run commands");
        }
        if (config.live_output_session_audit_path.empty()) {
            throw std::invalid_argument("Live route matching session audit path must not be empty");
        }
    }

    const auto route = read_route_signature_file(config.route_path);
    Gray8RouteMatcherConfig matcher_config;
    matcher_config.window_radius = config.window_radius;
    matcher_config.minimum_confidence = config.minimum_confidence;
    matcher_config.max_direction_shift_px = config.max_direction_shift_px;
    matcher_config.radians_per_pixel = config.radians_per_pixel;

    PiCameraSource source(config.camera);
    Gray8ResizePreprocessor preprocessor(config.target_width, config.target_height);
    Gray8RouteMatcher matcher(route, matcher_config);
    std::optional<BoundedNavigator> navigator;
    if (config.emit_dry_run_commands) {
        navigator.emplace(config.navigator);
    }
    DryRunCommandSink command_sink(&metrics);
    std::optional<LiveMavlinkOutputAuditLog> session_audit_log;
    std::optional<DryRunCommandSink> session_bridge;
    std::optional<LiveMavlinkOutputSession> live_output_session;
    HealthMonitor health(now());
    health.set_links(true, config.emit_dry_run_commands && !config.use_live_telemetry_stream, true);
    std::optional<MavlinkTelemetryStream> telemetry_stream;
    MavlinkTelemetryAdapter telemetry_adapter({
        .max_telemetry_age_ms = config.telemetry_max_age_ms,
        .navigation_confidence = 1.0,
        .require_armed = false,
        .required_mode = FlightMode::Guided,
    });
    LiveRouteMatchingResult result;

    metrics << "live_route_match_start width=" << config.camera.width
            << " height=" << config.camera.height
            << " fps=" << config.camera.frame_rate_hz
            << " target=" << config.target_width << "x" << config.target_height
            << " requested_frames=" << config.frames_to_capture
            << " warmup_frames=" << config.warmup_frames
            << " route=" << config.route_path.string()
            << " route_entries=" << route.entries.size()
            << " window_radius=" << config.window_radius
            << " minimum_confidence=" << config.minimum_confidence
            << " max_direction_shift_px=" << config.max_direction_shift_px
            << " radians_per_pixel=" << config.radians_per_pixel
            << " expected_progress=" << config.expected_progress
            << " max_progress_regressions=" << config.max_progress_regressions
            << " max_progress_rollback=" << config.max_progress_rollback
            << " require_endpoint_progress=" << (config.require_endpoint_progress ? "true" : "false")
            << " endpoint_start_progress=" << config.endpoint_start_progress
            << " endpoint_end_progress=" << config.endpoint_end_progress
            << " dry_run_commands=" << (config.emit_dry_run_commands ? "true" : "false")
            << " live_telemetry_stream=" << (config.use_live_telemetry_stream ? "true" : "false")
            << " telemetry_warmup_timeout_ms=" << config.telemetry_warmup_timeout_ms
            << " telemetry_max_age_ms=" << config.telemetry_max_age_ms
            << " require_live_telemetry_health=" << (config.require_live_telemetry_health ? "true" : "false");
    if (config.emit_dry_run_commands) {
        metrics << " navigator_minimum_confidence=" << config.navigator.minimum_confidence
                << " navigator_max_match_age_ms=" << config.navigator.max_match_age_ms
                << " navigator_yaw_gain=" << config.navigator.yaw_gain
                << " navigator_max_yaw_rate_radps=" << config.navigator.max_yaw_rate_radps
                << " navigator_max_yaw_accel_radps2=" << config.navigator.max_yaw_accel_radps2
                << " navigator_forward_speed_mps=" << config.navigator.forward_speed_mps
                << " require_command_quality=" << (config.require_dry_run_command_quality ? "true" : "false")
                << " minimum_valid_command_fraction=" << config.minimum_valid_dry_run_command_fraction
                << " max_invalid_command_streak=" << config.max_invalid_dry_run_command_streak
                << " max_abs_yaw_rate_radps=" << config.max_abs_dry_run_yaw_rate_radps
                << " max_yaw_rate_sign_flips=" << config.max_dry_run_yaw_rate_sign_flips
                << " max_yaw_rate_delta_radps=" << config.max_dry_run_yaw_rate_delta_radps
                << " session_audit=" << (config.emit_live_output_session_audit ? "true" : "false");
        if (config.emit_live_output_session_audit) {
            metrics << " session_audit_path=" << config.live_output_session_audit_path.string();
        }
    }
    metrics << "\n";

    if (config.use_live_telemetry_stream) {
        telemetry_stream.emplace(config.telemetry_stream);
        telemetry_stream->start();
        result.used_live_telemetry_stream = true;
        metrics << "live_route_match_telemetry_stream_start"
                << " device=" << config.telemetry_stream.device_path
                << " baud_rate=" << config.telemetry_stream.baud_rate
                << " started=true\n";
        const auto telemetry_warmup_started = now();
        while (milliseconds_between(telemetry_warmup_started, now()) <
               static_cast<double>(config.telemetry_warmup_timeout_ms)) {
            const auto telemetry = telemetry_stream->snapshot();
            const auto validation = validate_mavlink_telemetry(telemetry.inspection, {});
            copy_telemetry_stream_metrics(result, telemetry);
            if (validation.passed) {
                result.telemetry_warmup_passed = true;
                telemetry_adapter.observe(telemetry.inspection.latest, now());
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        result.telemetry_warmup_elapsed_ms = milliseconds_between(telemetry_warmup_started, now());
        metrics << "live_route_match_telemetry_warmup"
                << " timeout_ms=" << config.telemetry_warmup_timeout_ms
                << " elapsed_ms=" << result.telemetry_warmup_elapsed_ms
                << " passed=" << (result.telemetry_warmup_passed ? "true" : "false")
                << " bytes_captured=" << result.telemetry_bytes_captured
                << " bytes_retained=" << result.telemetry_bytes_retained
                << " bytes_dropped=" << result.telemetry_bytes_dropped
                << " frames_seen=" << result.telemetry_frames_seen
                << " heartbeat_messages=" << result.telemetry_heartbeat_messages
                << " attitude_messages=" << result.telemetry_attitude_messages
                << " global_position_int_messages=" << result.telemetry_global_position_int_messages;
        if (!telemetry_stream->last_error().empty()) {
            metrics << " error=" << telemetry_stream->last_error();
        }
        metrics << "\n";
        if (config.require_live_telemetry_health && !result.telemetry_warmup_passed) {
            telemetry_stream->stop();
            metrics << "live_route_match_done started=false warmup_frames_dropped=0 frames_captured=0 valid_matches=0 progress_regressions=0 empty_polls=0"
                    << " live_telemetry_stream=true"
                    << " telemetry_warmup_passed=false"
                    << " telemetry_bytes_captured=" << result.telemetry_bytes_captured
                    << " telemetry_bytes_retained=" << result.telemetry_bytes_retained
                    << " telemetry_bytes_dropped=" << result.telemetry_bytes_dropped
                    << " telemetry_frames_seen=" << result.telemetry_frames_seen
                    << " live_telemetry_health_passed=false"
                    << " passed=false\n";
            return result;
        }
    }

    result.started = source.start();
    metrics << "live_route_match_backend_start_result started=" << (result.started ? "true" : "false")
            << " running=" << (source.running() ? "true" : "false");
    if (!source.last_error().empty()) {
        metrics << " error=" << source.last_error();
    }
    metrics << "\n";

    if (!result.started) {
        metrics << "live_route_match_unavailable error=" << source.last_error() << "\n";
        metrics << "live_route_match_done started=false warmup_frames_dropped=0 frames_captured=0 valid_matches=0 progress_regressions=0 empty_polls=0 passed=false\n";
        if (telemetry_stream) {
            telemetry_stream->stop();
        }
        return result;
    }

    if (config.emit_dry_run_commands) {
        command_sink.start();
    }
    if (config.emit_live_output_session_audit) {
        session_audit_log.emplace(LiveMavlinkOutputAuditLogConfig{config.live_output_session_audit_path, false});
        session_bridge.emplace(nullptr);
        live_output_session.emplace(
            LiveMavlinkOutputSessionConfig{
                "match_live_route",
                live_output_gate_config_from_match_config(config, true),
            },
            *session_audit_log,
            *session_bridge);
        result.live_output_session_audit_path = config.live_output_session_audit_path.string();
        result.live_output_session_audit_started = live_output_session->start();
        metrics << "live_route_match_session_audit_start"
                << " path=" << result.live_output_session_audit_path
                << " started=" << (result.live_output_session_audit_started ? "true" : "false") << "\n";
        if (!result.live_output_session_audit_started) {
            source.stop();
            command_sink.stop();
            if (telemetry_stream) {
                telemetry_stream->stop();
            }
            metrics << "live_route_match_done started=false warmup_frames_dropped=" << result.warmup_frames_dropped
                    << " frames_captured=0 valid_matches=0 progress_regressions=0 empty_polls=" << result.empty_polls
                    << " live_output_session_audit_started=false passed=false\n";
            return result;
        }
    }

    const auto warmup_started_at = now();
    const auto requested_source_frames = config.frames_to_capture + config.warmup_frames;
    const auto warmup_timeout_ms = 2000.0 + (static_cast<double>(requested_source_frames) * 1000.0
        / static_cast<double>(config.camera.frame_rate_hz));
    while (result.warmup_frames_dropped < config.warmup_frames) {
        if (auto frame = source.poll()) {
            ++result.warmup_frames_dropped;
            metrics << "live_route_match_warmup_frame id=" << frame->id
                    << " size=" << frame->width << "x" << frame->height
                    << " bytes=" << frame->data.size()
                    << " dropped=" << result.warmup_frames_dropped
                    << "/" << config.warmup_frames << "\n";
        } else {
            ++result.empty_polls;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (milliseconds_between(warmup_started_at, now()) > warmup_timeout_ms) {
            break;
        }
    }

    emit_operator_start_cue(config.operator_cue_enabled,
                            config.operator_cue_seconds,
                            config.operator_cue_bell,
                            "match_live_route",
                            "Live route matching starts on the next captured frame; expected_progress=" + config.expected_progress
                                + " route_output=" + config.route_path.string(),
                            metrics);
    if (config.operator_cue_enabled) {
        metrics << "live_route_match_pre_capture_frame_drain drained_frames=" << drain_pending_frames(source) << "\n";
    }

    const auto capture_started_at = now();
    const auto capture_timeout_ms = 2000.0 + (static_cast<double>(config.frames_to_capture) * 1000.0
        / static_cast<double>(config.camera.frame_rate_hz));
    double confidence_sum = 0.0;
    std::optional<double> last_valid_progress;
    std::optional<double> previous_valid_command_yaw_rate;
    std::optional<LiveMavlinkOutputSafetySnapshot> last_live_output_gate_snapshot;
    MavlinkTelemetry latest_gate_telemetry;
    std::map<std::string, std::uint64_t> live_output_gate_block_reasons;
    std::uint64_t current_invalid_command_streak = 0;
    while (result.frames_captured < config.frames_to_capture) {
        if (auto frame = source.poll()) {
            const auto processing_started = now();
            const auto processed = preprocessor.process(*frame);
            const auto match = matcher.match(processed);
            const auto processing_finished = now();
            const auto timing = health.observe_processed_frame(processed, processing_started, processing_finished);
            health.set_route_match_confidence(match.confidence);
            if (telemetry_stream) {
                const auto telemetry = telemetry_stream->snapshot();
                const auto validation = validate_mavlink_telemetry(telemetry.inspection, {});
                copy_telemetry_stream_metrics(result, telemetry);
                if (validation.passed) {
                    latest_gate_telemetry = telemetry.inspection.latest;
                    latest_gate_telemetry.timestamp = processing_finished;
                    telemetry_adapter.observe(latest_gate_telemetry, processing_finished);
                }
                const bool telemetry_health_ready = telemetry_adapter.mavlink_ok(processing_finished);
                health.set_links(true, telemetry_health_ready, true);
                if (telemetry_health_ready) {
                    ++result.telemetry_health_ready_frames;
                } else {
                    ++result.telemetry_health_degraded_frames;
                }
            }
            const auto health_snapshot = health.snapshot(processing_finished);
            NavigationCommand command;
            LiveMavlinkOutputSafetyResult live_output_gate_result{false, "dry_run_commands_disabled"};
            if (config.emit_dry_run_commands) {
                command = navigator->update(match, health_snapshot);
                command_sink.send(command);
                ++result.dry_run_commands;
                if (command.valid) {
                    ++result.valid_dry_run_commands;
                    current_invalid_command_streak = 0;
                    const auto abs_yaw_rate = std::abs(command.yaw_rate_radps);
                    result.max_abs_dry_run_yaw_rate_radps =
                        std::max(result.max_abs_dry_run_yaw_rate_radps, abs_yaw_rate);
                    if (previous_valid_command_yaw_rate) {
                        result.max_dry_run_yaw_rate_delta_radps = std::max(
                            result.max_dry_run_yaw_rate_delta_radps,
                            std::abs(command.yaw_rate_radps - *previous_valid_command_yaw_rate));
                        if ((*previous_valid_command_yaw_rate < 0.0 && command.yaw_rate_radps > 0.0)
                            || (*previous_valid_command_yaw_rate > 0.0 && command.yaw_rate_radps < 0.0)) {
                            ++result.dry_run_yaw_rate_sign_flips;
                        }
                    }
                    previous_valid_command_yaw_rate = command.yaw_rate_radps;
                } else {
                    ++current_invalid_command_streak;
                    result.max_invalid_dry_run_command_streak =
                        std::max(result.max_invalid_dry_run_command_streak, current_invalid_command_streak);
                }
                const LiveMavlinkOutputSafetyGate gate(
                    live_output_gate_config_from_match_config(config, true));
                last_live_output_gate_snapshot =
                    live_output_gate_snapshot(processing_finished, latest_gate_telemetry, match, command);
                live_output_gate_result = gate.evaluate(*last_live_output_gate_snapshot);
                if (live_output_session) {
                    const auto session_result = live_output_session->process(*last_live_output_gate_snapshot);
                    if (session_result.safety.reason != live_output_gate_result.reason
                        || session_result.safety.allowed != live_output_gate_result.allowed) {
                        throw std::runtime_error("Live output session audit safety result diverged from gate diagnostics");
                    }
                }
                if (live_output_gate_result.allowed) {
                    ++result.live_output_gate_allowed_frames;
                } else {
                    ++result.live_output_gate_blocked_frames;
                    ++live_output_gate_block_reasons[live_output_gate_result.reason];
                }
            }

            ++result.frames_captured;
            confidence_sum += match.confidence;
            result.minimum_confidence_seen = result.frames_captured == 1
                ? match.confidence
                : std::min(result.minimum_confidence_seen, match.confidence);
            if (result.frames_captured == 1) {
                result.first_progress = match.progress;
                result.min_progress_seen = match.progress;
                result.max_progress_seen = match.progress;
            } else {
                result.min_progress_seen = std::min(result.min_progress_seen, match.progress);
                result.max_progress_seen = std::max(result.max_progress_seen, match.progress);
            }
            result.last_progress = match.progress;
            result.last_frame_age_ms = timing.frame_age_ms;
            result.last_processing_latency_ms = timing.processing_latency_ms;

            if (match.valid) {
                ++result.valid_matches;
                if (last_valid_progress && match.progress < *last_valid_progress) {
                    ++result.progress_regressions;
                    result.progress_rollback += *last_valid_progress - match.progress;
                    result.progress_monotonic = false;
                }
                if (last_valid_progress && match.progress > *last_valid_progress) {
                    ++result.reverse_progress_regressions;
                    result.reverse_progress_rollback += match.progress - *last_valid_progress;
                    result.reverse_progress_monotonic = false;
                }
                last_valid_progress = match.progress;
            }

            metrics << "live_route_match_frame id=" << processed.id
                    << " size=" << processed.width << "x" << processed.height
                    << " bytes=" << processed.data.size()
                    << " route_index=" << match.route_index
                    << " progress=" << match.progress
                    << " confidence=" << match.confidence
                    << " valid=" << (match.valid ? "true" : "false")
                    << " direction_error_rad=" << match.direction_error_rad
                    << " age_ms=" << timing.frame_age_ms
                    << " latency_ms=" << timing.processing_latency_ms;
            if (config.emit_dry_run_commands) {
                metrics << " command_valid=" << (command.valid ? "true" : "false")
                        << " command_yaw_rate_radps=" << command.yaw_rate_radps
                        << " command_vx_mps=" << command.vx_mps
                        << " command_confidence=" << command.confidence
                        << " live_output_gate_allowed=" << (live_output_gate_result.allowed ? "true" : "false")
                        << " live_output_gate_reason=" << live_output_gate_result.reason;
            }
            if (telemetry_stream) {
                metrics << " telemetry_mavlink_ok=" << (health_snapshot.mavlink_ok ? "true" : "false")
                        << " telemetry_frames_seen=" << result.telemetry_frames_seen
                        << " telemetry_heartbeat_messages=" << result.telemetry_heartbeat_messages;
            }
            metrics << "\n";
        } else {
            ++result.empty_polls;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (milliseconds_between(capture_started_at, now()) > capture_timeout_ms) {
            break;
        }
    }

    source.stop();
    if (live_output_session) {
        live_output_session->stop("match_live_route_complete");
    }
    if (config.emit_dry_run_commands) {
        command_sink.stop();
    }
    if (telemetry_stream) {
        telemetry_stream->stop();
        const auto telemetry = telemetry_stream->snapshot();
        copy_telemetry_stream_metrics(result, telemetry);
    }
    emit_operator_stop_cue(config.operator_cue_enabled,
                           config.operator_cue_bell,
                           "match_live_route",
                           metrics);
    result.elapsed_ms = milliseconds_between(capture_started_at, now());
    result.effective_fps = result.elapsed_ms > 0.0
        ? (static_cast<double>(result.frames_captured) * 1000.0 / result.elapsed_ms)
        : 0.0;
    result.average_confidence = result.frames_captured > 0
        ? confidence_sum / static_cast<double>(result.frames_captured)
        : 0.0;
    if (config.expected_progress == "forward") {
        result.directional_progress_passed =
            result.progress_regressions <= config.max_progress_regressions
            && result.progress_rollback <= config.max_progress_rollback;
    } else if (config.expected_progress == "reverse") {
        result.directional_progress_passed =
            result.reverse_progress_regressions <= config.max_progress_regressions
            && result.reverse_progress_rollback <= config.max_progress_rollback;
    } else {
        result.directional_progress_passed = true;
    }

    if (config.expected_progress == "forward") {
        result.endpoint_progress_passed =
            result.first_progress <= config.endpoint_start_progress
            && result.max_progress_seen >= config.endpoint_end_progress;
    } else if (config.expected_progress == "reverse") {
        result.endpoint_progress_passed =
            result.first_progress >= config.endpoint_end_progress
            && result.min_progress_seen <= config.endpoint_start_progress;
    } else {
        result.endpoint_progress_passed = true;
    }

    result.progress_gate_passed = config.require_endpoint_progress
        ? result.endpoint_progress_passed
        : result.directional_progress_passed;

    if (config.emit_dry_run_commands) {
        result.valid_dry_run_command_fraction = result.dry_run_commands > 0
            ? static_cast<double>(result.valid_dry_run_commands) / static_cast<double>(result.dry_run_commands)
            : 0.0;
        result.dry_run_command_quality_passed =
            result.dry_run_commands == result.frames_captured
            && result.valid_dry_run_command_fraction >= config.minimum_valid_dry_run_command_fraction
            && result.max_invalid_dry_run_command_streak <= config.max_invalid_dry_run_command_streak
            && result.max_abs_dry_run_yaw_rate_radps <= config.max_abs_dry_run_yaw_rate_radps
            && result.dry_run_yaw_rate_sign_flips <= config.max_dry_run_yaw_rate_sign_flips
            && result.max_dry_run_yaw_rate_delta_radps <= config.max_dry_run_yaw_rate_delta_radps;
    }
    if (config.use_live_telemetry_stream) {
        result.live_telemetry_health_passed =
            result.telemetry_warmup_passed
            && result.telemetry_health_degraded_frames == 0
            && result.telemetry_health_ready_frames == result.frames_captured;
    }
    if (config.emit_dry_run_commands && last_live_output_gate_snapshot) {
        const LiveMavlinkOutputSafetyGate final_gate(
            live_output_gate_config_from_match_config(config, result.dry_run_command_quality_passed));
        const auto final_gate_result = final_gate.evaluate(*last_live_output_gate_snapshot);
        result.final_live_output_gate_allowed = final_gate_result.allowed;
        result.final_live_output_gate_reason = final_gate_result.reason;
    }
    result.live_output_gate_block_reasons = format_reason_counts(live_output_gate_block_reasons);

    result.passed = result.started
        && result.frames_captured == static_cast<std::uint64_t>(config.frames_to_capture)
        && result.valid_matches == result.frames_captured
        && result.progress_gate_passed
        && (!config.require_live_telemetry_health || result.live_telemetry_health_passed)
        && (!config.require_dry_run_command_quality || result.dry_run_command_quality_passed);

    metrics << "live_route_match_done started=true"
            << " warmup_frames_dropped=" << result.warmup_frames_dropped
            << " frames_captured=" << result.frames_captured
            << " valid_matches=" << result.valid_matches
            << " progress_regressions=" << result.progress_regressions
            << " reverse_progress_regressions=" << result.reverse_progress_regressions
            << " progress_rollback=" << result.progress_rollback
            << " reverse_progress_rollback=" << result.reverse_progress_rollback
            << " max_progress_regressions=" << config.max_progress_regressions
            << " max_progress_rollback=" << config.max_progress_rollback
            << " empty_polls=" << result.empty_polls
            << " minimum_confidence_seen=" << result.minimum_confidence_seen
            << " average_confidence=" << result.average_confidence
            << " first_progress=" << result.first_progress
            << " last_progress=" << result.last_progress
            << " min_progress_seen=" << result.min_progress_seen
            << " max_progress_seen=" << result.max_progress_seen
            << " progress_monotonic=" << (result.progress_monotonic ? "true" : "false")
            << " reverse_progress_monotonic=" << (result.reverse_progress_monotonic ? "true" : "false")
            << " expected_progress=" << config.expected_progress
            << " directional_progress_passed=" << (result.directional_progress_passed ? "true" : "false")
            << " endpoint_progress_passed=" << (result.endpoint_progress_passed ? "true" : "false")
            << " progress_gate_passed=" << (result.progress_gate_passed ? "true" : "false")
            << " live_telemetry_stream=" << (result.used_live_telemetry_stream ? "true" : "false")
            << " telemetry_warmup_passed=" << (result.telemetry_warmup_passed ? "true" : "false")
            << " telemetry_warmup_elapsed_ms=" << result.telemetry_warmup_elapsed_ms
            << " telemetry_bytes_captured=" << result.telemetry_bytes_captured
            << " telemetry_bytes_retained=" << result.telemetry_bytes_retained
            << " telemetry_bytes_dropped=" << result.telemetry_bytes_dropped
            << " telemetry_frames_seen=" << result.telemetry_frames_seen
            << " telemetry_heartbeat_messages=" << result.telemetry_heartbeat_messages
            << " telemetry_attitude_messages=" << result.telemetry_attitude_messages
            << " telemetry_global_position_int_messages=" << result.telemetry_global_position_int_messages
            << " telemetry_health_ready_frames=" << result.telemetry_health_ready_frames
            << " telemetry_health_degraded_frames=" << result.telemetry_health_degraded_frames
            << " live_telemetry_health_passed=" << (result.live_telemetry_health_passed ? "true" : "false")
            << " last_age_ms=" << result.last_frame_age_ms
            << " last_latency_ms=" << result.last_processing_latency_ms
            << " elapsed_ms=" << result.elapsed_ms
            << " effective_fps=" << result.effective_fps
            << " dry_run_commands=" << result.dry_run_commands
            << " valid_dry_run_commands=" << result.valid_dry_run_commands
            << " valid_dry_run_command_fraction=" << result.valid_dry_run_command_fraction
            << " max_invalid_dry_run_command_streak=" << result.max_invalid_dry_run_command_streak
            << " max_abs_dry_run_yaw_rate_radps=" << result.max_abs_dry_run_yaw_rate_radps
            << " dry_run_yaw_rate_sign_flips=" << result.dry_run_yaw_rate_sign_flips
            << " max_dry_run_yaw_rate_delta_radps=" << result.max_dry_run_yaw_rate_delta_radps
            << " dry_run_command_quality_passed=" << (result.dry_run_command_quality_passed ? "true" : "false")
            << " live_output_gate_allowed_frames=" << result.live_output_gate_allowed_frames
            << " live_output_gate_blocked_frames=" << result.live_output_gate_blocked_frames
            << " live_output_gate_block_reasons=" << result.live_output_gate_block_reasons
            << " final_live_output_gate_allowed=" << (result.final_live_output_gate_allowed ? "true" : "false")
            << " final_live_output_gate_reason=" << result.final_live_output_gate_reason
            << " live_output_session_audit_started=" << (result.live_output_session_audit_started ? "true" : "false")
            << " live_output_session_audit_path=" << result.live_output_session_audit_path
            << " passed=" << (result.passed ? "true" : "false") << "\n";

    metrics << "live_route_match_compact"
            << " passed=" << bool_word(result.passed)
            << " frames=" << result.frames_captured << "/" << config.frames_to_capture
            << " valid_matches=" << result.valid_matches
            << " progress=" << result.first_progress << ".." << result.last_progress
            << " minmax_progress=" << result.min_progress_seen << ".." << result.max_progress_seen
            << " endpoint_passed=" << bool_word(result.endpoint_progress_passed)
            << " progress_gate_passed=" << bool_word(result.progress_gate_passed)
            << " confidence_min_avg=" << result.minimum_confidence_seen << "/" << result.average_confidence
            << " telemetry_health=" << bool_word(result.live_telemetry_health_passed)
            << " telemetry_dropped=" << result.telemetry_bytes_dropped
            << " dry_run_quality=" << bool_word(result.dry_run_command_quality_passed)
            << " dry_run_valid=" << result.valid_dry_run_commands << "/" << result.dry_run_commands
            << " live_output_gate_allowed=" << result.live_output_gate_allowed_frames
            << " live_output_gate_blocked=" << result.live_output_gate_blocked_frames
            << " live_output_gate_block_reasons=" << result.live_output_gate_block_reasons
            << " final_live_output_gate_reason=" << result.final_live_output_gate_reason
            << " live_output_session_audit=" << bool_word(result.live_output_session_audit_started)
            << "\n";
    return result;
}

} // namespace vh
