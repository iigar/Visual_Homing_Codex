#include "visual_homing/camera_smoke.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <vector>

#include "visual_homing/bounded_navigator.hpp"
#include "visual_homing/dry_run_command_sink.hpp"
#include "visual_homing/gray8_resize_preprocessor.hpp"
#include "visual_homing/gray8_route_matcher.hpp"
#include "visual_homing/health_monitor.hpp"
#include "visual_homing/live_external_nav_output_audit_log.hpp"
#include "visual_homing/live_external_nav_output_session.hpp"
#include "visual_homing/live_mavlink_bridge.hpp"
#include "visual_homing/live_mavlink_external_nav_writer.hpp"
#include "visual_homing/live_mavlink_output_audit_log.hpp"
#include "visual_homing/live_mavlink_output_session.hpp"
#include "visual_homing/live_mavlink_output_safety_gate.hpp"
#include "visual_homing/live_mavlink_serial_writer.hpp"
#include "visual_homing/mavlink_telemetry_adapter.hpp"
#include "visual_homing/route_signature.hpp"
#include "visual_homing/route_signature_recorder.hpp"
#include "visual_homing/time.hpp"

namespace vh {

namespace {

constexpr double kTrackedProgressRegressionDeadband = 0.01;
constexpr double kTrackedVisualScaleMaxStep = 0.05;

struct FocusRoiPixels {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

std::uint64_t monotonic_time_usec(const Timestamp timestamp) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch()).count());
}

class DisabledExternalNavWriter final : public ExternalNavWriter {
public:
    bool start() override {
        return false;
    }

    void stop() override {}

    void send_vision_position_estimate(const ExternalNavEstimate&, std::uint64_t) override {
        throw std::runtime_error("External-nav output writer is not attached");
    }

    bool running() const override {
        return false;
    }

    std::string unavailable_reason() const override {
        return "external_nav_writer_not_attached";
    }
};

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

std::string wall_time_utc_compact() {
    const auto current = std::chrono::system_clock::now();
    const auto current_time = std::chrono::system_clock::to_time_t(current);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &current_time);
#else
    gmtime_r(&current_time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y%m%dT%H%M%SZ");
    return output.str();
}

void write_gray8_frame_pgm(const std::filesystem::path& path, const Frame& frame) {
    if (frame.format != PixelFormat::Gray8) {
        throw std::runtime_error("Endpoint stop frame export requires Gray8 frame");
    }
    const auto expected_size = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    if (frame.width <= 0 || frame.height <= 0 || frame.data.size() != expected_size) {
        throw std::runtime_error("Endpoint stop frame export received invalid frame dimensions");
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot open endpoint stop frame path for write: " + path.string());
    }
    output << "P5\n" << frame.width << " " << frame.height << "\n255\n";
    output.write(reinterpret_cast<const char*>(frame.data.data()), static_cast<std::streamsize>(frame.data.size()));
    if (!output) {
        throw std::runtime_error("Failed to write endpoint stop frame: " + path.string());
    }
}

FocusRoiPixels focus_roi_pixels(
    int width,
    int height,
    double left_fraction,
    double right_fraction,
    double top_fraction,
    double bottom_fraction) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Focus ROI source dimensions must be positive");
    }
    const auto roi_x = static_cast<int>(std::lround(static_cast<double>(width) * left_fraction));
    const auto roi_y = static_cast<int>(std::lround(static_cast<double>(height) * top_fraction));
    const auto roi_right = static_cast<int>(std::lround(static_cast<double>(width) * (1.0 - right_fraction)));
    const auto roi_bottom = static_cast<int>(std::lround(static_cast<double>(height) * (1.0 - bottom_fraction)));
    return {
        std::clamp(roi_x, 0, width),
        std::clamp(roi_y, 0, height),
        std::clamp(roi_right - roi_x, 0, width),
        std::clamp(roi_bottom - roi_y, 0, height),
    };
}

Frame crop_gray8_frame(const Frame& frame, const FocusRoiPixels& roi) {
    if (frame.format != PixelFormat::Gray8) {
        throw std::invalid_argument("Focus ROI diagnostics require Gray8 frames");
    }
    const auto expected_size = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    if (frame.width <= 0 || frame.height <= 0 || frame.data.size() != expected_size) {
        throw std::invalid_argument("Focus ROI diagnostics received an invalid frame");
    }
    if (roi.x < 0 || roi.y < 0 || roi.width <= 0 || roi.height <= 0
        || roi.x + roi.width > frame.width || roi.y + roi.height > frame.height) {
        throw std::invalid_argument("Focus ROI crop is outside the source frame");
    }

    Frame cropped;
    cropped.id = frame.id;
    cropped.timestamp = frame.timestamp;
    cropped.width = roi.width;
    cropped.height = roi.height;
    cropped.format = PixelFormat::Gray8;
    cropped.data.resize(static_cast<std::size_t>(roi.width) * static_cast<std::size_t>(roi.height));
    for (int row = 0; row < roi.height; ++row) {
        const auto source_offset = static_cast<std::size_t>(roi.y + row) * static_cast<std::size_t>(frame.width)
            + static_cast<std::size_t>(roi.x);
        const auto destination_offset = static_cast<std::size_t>(row) * static_cast<std::size_t>(roi.width);
        std::copy_n(
            frame.data.begin() + static_cast<std::ptrdiff_t>(source_offset),
            roi.width,
            cropped.data.begin() + static_cast<std::ptrdiff_t>(destination_offset));
    }
    return cropped;
}

RouteSignatureFile crop_gray8_route(const RouteSignatureFile& route, const FocusRoiPixels& roi) {
    RouteSignatureFile cropped;
    cropped.version = route.version;
    cropped.entries.reserve(route.entries.size());
    for (const auto& entry : route.entries) {
        Frame frame;
        frame.id = entry.frame_id;
        frame.timestamp = Timestamp{};
        frame.width = entry.width;
        frame.height = entry.height;
        frame.format = entry.format;
        frame.data = entry.payload;
        const auto cropped_frame = crop_gray8_frame(frame, roi);

        RouteSignatureEntry cropped_entry = entry;
        cropped_entry.width = static_cast<std::uint16_t>(cropped_frame.width);
        cropped_entry.height = static_cast<std::uint16_t>(cropped_frame.height);
        cropped_entry.payload = cropped_frame.data;
        cropped.entries.push_back(std::move(cropped_entry));
    }
    return cropped;
}

std::string ratio_histogram_text(const std::map<double, std::uint64_t>& histogram) {
    if (histogram.empty()) {
        return "none";
    }

    std::ostringstream output;
    bool first = true;
    for (const auto& [ratio, count] : histogram) {
        if (!first) {
            output << ",";
        }
        first = false;
        output << ratio << ":" << count;
    }
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

struct VisualScaleDiagnostic {
    bool valid = false;
    double scale_ratio = 0.0;
    double altitude_m = 0.0;
    double confidence = 0.0;
};

double scaled_reference_distance(const Frame& current, const RouteSignatureEntry& reference, double scale_ratio) {
    if (current.format != PixelFormat::Gray8 || reference.format != PixelFormat::Gray8
        || current.width != reference.width || current.height != reference.height
        || current.data.size() != reference.payload.size() || current.width <= 0 || current.height <= 0
        || scale_ratio <= 0.0 || !std::isfinite(scale_ratio)) {
        return std::numeric_limits<double>::infinity();
    }

    const double center_x = (static_cast<double>(current.width) - 1.0) * 0.5;
    const double center_y = (static_cast<double>(current.height) - 1.0) * 0.5;
    std::uint64_t sum = 0;
    std::size_t count = 0;
    for (int y = 0; y < current.height; ++y) {
        for (int x = 0; x < current.width; ++x) {
            const auto reference_x = static_cast<int>(
                std::lround(center_x + (static_cast<double>(x) - center_x) / scale_ratio));
            const auto reference_y = static_cast<int>(
                std::lround(center_y + (static_cast<double>(y) - center_y) / scale_ratio));
            if (reference_x < 0 || reference_x >= current.width || reference_y < 0 || reference_y >= current.height) {
                continue;
            }
            const auto current_index = static_cast<std::size_t>(y) * static_cast<std::size_t>(current.width)
                + static_cast<std::size_t>(x);
            const auto reference_index = static_cast<std::size_t>(reference_y) * static_cast<std::size_t>(current.width)
                + static_cast<std::size_t>(reference_x);
            const auto delta = static_cast<int>(current.data[current_index]) - static_cast<int>(reference.payload[reference_index]);
            sum += static_cast<std::uint64_t>(std::abs(delta));
            ++count;
        }
    }

    if (count == 0) {
        return std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(sum) / (static_cast<double>(count) * 255.0);
}

VisualScaleDiagnostic estimate_visual_scale_diagnostic(
    const Frame& current,
    const RouteSignatureEntry& reference,
    double reference_altitude_m) {
    VisualScaleDiagnostic diagnostic;
    if (reference_altitude_m <= 0.0 || !std::isfinite(reference_altitude_m)) {
        return diagnostic;
    }

    const std::vector<double> candidates{
        0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75,
        0.80, 0.85, 0.90, 0.95, 1.0, 1.05, 1.10, 1.15, 1.20, 1.25,
        1.30, 1.35, 1.40, 1.50};
    double best_distance = std::numeric_limits<double>::infinity();
    double best_scale = 1.0;
    for (const auto scale : candidates) {
        const auto distance = scaled_reference_distance(current, reference, scale);
        if (distance < best_distance) {
            best_distance = distance;
            best_scale = scale;
        }
    }

    if (!std::isfinite(best_distance)) {
        return diagnostic;
    }
    diagnostic.valid = true;
    diagnostic.scale_ratio = best_scale;
    diagnostic.altitude_m = reference_altitude_m / best_scale;
    diagnostic.confidence = std::clamp(1.0 - best_distance, 0.0, 1.0);
    return diagnostic;
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
    result.telemetry_altitude_messages = telemetry.inspection.altitude_messages;
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
    result.telemetry_altitude_messages = telemetry.inspection.altitude_messages;
}

MavlinkTelemetryValidationConfig live_route_match_telemetry_validation_config(
    const LiveRouteMatchingConfig& config) {
    MavlinkTelemetryValidationConfig validation_config;
    if (config.emit_external_nav_estimates) {
        validation_config.minimum_global_position_int_messages = 0;
    }
    return validation_config;
}

LiveMavlinkOutputSafetyConfig live_output_gate_config_from_match_config(
    const LiveRouteMatchingConfig& config,
    bool dry_run_quality_passed) {
    LiveMavlinkOutputSafetyConfig gate_config;
    gate_config.live_output_available =
        config.live_output_runtime_controls_provided ? LiveMavlinkBridge::command_output_available() : true;
    gate_config.runtime_enabled =
        config.live_output_runtime_controls_provided ? config.live_output_runtime_enabled : true;
    gate_config.operator_confirmed =
        config.live_output_runtime_controls_provided ? config.live_output_operator_confirmed : true;
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

} // namespace

bool live_route_match_endpoint_reached(const LiveRouteMatchingConfig& config, double progress) {
    if (config.expected_progress == "forward") {
        return progress >= config.endpoint_end_progress;
    }
    if (config.expected_progress == "reverse") {
        return progress <= config.endpoint_start_progress;
    }
    return false;
}

bool live_route_match_endpoint_confirmation_passed(const LiveRouteMatchingConfig& config,
                                                   double top_match_gap,
                                                   double edge_top_match_gap) {
    if (!config.endpoint_require_unambiguous_match) {
        return true;
    }
    return top_match_gap >= config.endpoint_min_top_match_gap
        && edge_top_match_gap >= config.endpoint_min_edge_top_match_gap;
}

bool live_route_match_endpoint_progress_passed(const LiveRouteMatchingConfig& config,
                                               const LiveRouteMatchingResult& result) {
    if (config.expected_progress == "forward") {
        return result.first_tracked_progress <= config.endpoint_start_progress
            && result.max_tracked_progress_seen >= config.endpoint_end_progress;
    }
    if (config.expected_progress == "reverse") {
        return result.first_tracked_progress >= config.endpoint_end_progress
            && result.min_tracked_progress_seen <= config.endpoint_start_progress;
    }
    return true;
}

bool live_route_match_has_required_frame_count(const LiveRouteMatchingConfig& config,
                                               const LiveRouteMatchingResult& result) {
    if (result.frames_captured == static_cast<std::uint64_t>(config.frames_to_capture)) {
        return true;
    }
    return config.stop_at_endpoint_progress
        && result.endpoint_stop_triggered
        && result.frames_captured > 0
        && result.frames_captured < static_cast<std::uint64_t>(config.frames_to_capture);
}

double live_route_match_next_tracked_progress(const std::string& expected_progress,
                                              double previous_tracked_progress,
                                              double raw_progress) {
    if (!std::isfinite(previous_tracked_progress) || !std::isfinite(raw_progress)) {
        throw std::invalid_argument("live route progress tracker requires finite progress values");
    }
    if (previous_tracked_progress < 0.0 || previous_tracked_progress > 1.0
        || raw_progress < 0.0 || raw_progress > 1.0) {
        throw std::invalid_argument("live route progress tracker progress values must be within 0..1");
    }

    const auto delta = raw_progress - previous_tracked_progress;
    if (delta == 0.0) {
        return previous_tracked_progress;
    }

    const auto expected_direction =
        expected_progress == "forward" ? 1.0 : (expected_progress == "reverse" ? -1.0 : 0.0);
    const auto follows_expected_direction = expected_direction == 0.0 || delta * expected_direction >= 0.0;
    if (expected_direction != 0.0 && !follows_expected_direction) {
        return previous_tracked_progress;
    }

    const auto alpha = expected_direction == 0.0 ? 0.35 : 0.45;
    const auto max_step = expected_direction == 0.0 ? 0.06 : 0.06;
    const auto step = std::clamp(delta * alpha, -max_step, max_step);
    return std::clamp(previous_tracked_progress + step, 0.0, 1.0);
}

double live_route_match_next_tracked_visual_scale_ratio(double previous_tracked_scale_ratio,
                                                        double raw_scale_ratio) {
    if (!std::isfinite(previous_tracked_scale_ratio) || !std::isfinite(raw_scale_ratio)) {
        throw std::invalid_argument("live route visual-scale tracker requires finite scale ratio values");
    }
    if (previous_tracked_scale_ratio <= 0.0 || raw_scale_ratio <= 0.0) {
        throw std::invalid_argument("live route visual-scale tracker scale ratios must be positive");
    }

    const auto delta = raw_scale_ratio - previous_tracked_scale_ratio;
    const auto step = std::clamp(delta, -kTrackedVisualScaleMaxStep, kTrackedVisualScaleMaxStep);
    return previous_tracked_scale_ratio + step;
}

namespace {

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

std::string format_top_match_candidates(const std::vector<RouteMatchCandidate>& candidates) {
    if (candidates.empty()) {
        return "none";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (index != 0) {
            output << ",";
        }
        output << candidates[index].route_index
               << ":" << candidates[index].progress
               << ":" << candidates[index].confidence;
    }
    return output.str();
}

std::string format_zone_probe_candidates(const std::vector<RouteMatchZoneCandidate>& zones) {
    if (zones.empty()) {
        return "none";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < zones.size(); ++index) {
        if (index != 0) {
            output << ",";
        }
        const auto& zone = zones[index];
        output << zone.name << "=";
        if (zone.valid) {
            output << zone.candidate.route_index
                   << ":" << zone.candidate.progress
                   << ":" << zone.candidate.confidence;
        } else {
            output << "none";
        }
    }
    return output.str();
}

std::optional<RouteMatchCandidate> find_zone_candidate(
    const std::vector<RouteMatchZoneCandidate>& zones,
    const std::string& name) {
    for (const auto& zone : zones) {
        if (zone.valid && std::string(zone.name) == name) {
            return zone.candidate;
        }
    }
    return std::nullopt;
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
                << " global_position_int_messages=" << result.telemetry_global_position_int_messages
                << " altitude_messages=" << result.telemetry_altitude_messages;
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
    if (config.stop_at_endpoint_progress && config.expected_progress == "any") {
        throw std::invalid_argument("Live route matching stop_at_endpoint_progress requires forward or reverse expected_progress");
    }
    if (!std::isfinite(config.endpoint_dwell_ms) || config.endpoint_dwell_ms < 0.0) {
        throw std::invalid_argument("Live route matching endpoint_dwell_ms must be finite and non-negative");
    }
    if (!std::isfinite(config.endpoint_min_top_match_gap) || config.endpoint_min_top_match_gap < 0.0) {
        throw std::invalid_argument("Live route matching endpoint_min_top_match_gap must be finite and non-negative");
    }
    if (!std::isfinite(config.endpoint_min_edge_top_match_gap) || config.endpoint_min_edge_top_match_gap < 0.0) {
        throw std::invalid_argument(
            "Live route matching endpoint_min_edge_top_match_gap must be finite and non-negative");
    }
    if (!std::isfinite(config.endpoint_ambiguous_hold_dwell_ms)
        || config.endpoint_ambiguous_hold_dwell_ms < 0.0) {
        throw std::invalid_argument(
            "Live route matching endpoint_ambiguous_hold_dwell_ms must be finite and non-negative");
    }
    if (config.endpoint_allow_ambiguous_hold) {
        if (!config.endpoint_require_unambiguous_match) {
            throw std::invalid_argument(
                "Live route matching ambiguous endpoint hold requires endpoint unambiguous confirmation");
        }
        if ((config.live_output_runtime_controls_provided && config.live_output_runtime_enabled)
            || (config.external_nav_output_runtime_controls_provided
                && config.external_nav_output_runtime_enabled)) {
            throw std::invalid_argument(
                "Live route matching ambiguous endpoint hold is attach-only and requires runtime output disabled");
        }
    }
    if (config.export_endpoint_stop_frame && config.endpoint_stop_frame_dir.empty()) {
        throw std::invalid_argument("Live route matching endpoint stop frame export requires a non-empty directory");
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
    if (config.emit_external_nav_estimates && !config.use_live_telemetry_stream) {
        throw std::invalid_argument("Live route matching external-nav estimates require live telemetry stream");
    }
    if (config.visual_scale_diagnostics) {
        if (!config.emit_external_nav_estimates) {
            throw std::invalid_argument("Live route matching visual-scale diagnostics require external-nav estimates");
        }
        if (!std::isfinite(config.visual_scale_reference_altitude_m)
            || config.visual_scale_reference_altitude_m <= 0.0) {
            throw std::invalid_argument(
                "Live route matching visual-scale reference altitude must be finite and positive");
        }
    }
    if (config.emit_external_nav_estimates
        && (config.external_nav_expected_relative_altitude_m != 0.0
            || config.external_nav_expected_relative_altitude_tolerance_m != 0.0)) {
        if (!std::isfinite(config.external_nav_expected_relative_altitude_m)
            || config.external_nav_expected_relative_altitude_m <= 0.0) {
            throw std::invalid_argument(
                "Live route matching external-nav expected relative altitude must be finite and positive");
        }
        if (!std::isfinite(config.external_nav_expected_relative_altitude_tolerance_m)
            || config.external_nav_expected_relative_altitude_tolerance_m <= 0.0) {
            throw std::invalid_argument(
                "Live route matching external-nav expected relative altitude tolerance must be finite and positive");
        }
    }
    if (config.emit_live_output_session_audit) {
        if (!config.emit_dry_run_commands) {
            throw std::invalid_argument("Live route matching session audit requires dry-run commands");
        }
        if (config.live_output_session_audit_path.empty()) {
            throw std::invalid_argument("Live route matching session audit path must not be empty");
        }
        if (!std::isfinite(config.live_output_max_duration_ms) || config.live_output_max_duration_ms < 0.0) {
            throw std::invalid_argument("Live route matching live_output_max_duration_ms must be finite and non-negative");
        }
#if VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE
        if (config.live_output_runtime_controls_provided && !config.use_live_telemetry_stream) {
            throw std::invalid_argument("Live route matching attached live output requires live telemetry stream device configuration");
        }
#endif
    }
    if ((config.top_match_diagnostics || config.endpoint_require_unambiguous_match)
        && config.top_match_count == 0) {
        throw std::invalid_argument(
            "Live route matching top_match_count must be positive when top diagnostics or endpoint confirmation are enabled");
    }
    if ((config.edge_match_diagnostics || config.endpoint_require_unambiguous_match)
        && config.edge_match_top_count == 0) {
        throw std::invalid_argument(
            "Live route matching edge_match_top_count must be positive when edge diagnostics or endpoint confirmation are enabled");
    }
    if (config.focus_roi_diagnostics) {
        if (!std::isfinite(config.focus_roi_left_fraction)
            || !std::isfinite(config.focus_roi_right_fraction)
            || !std::isfinite(config.focus_roi_top_fraction)
            || !std::isfinite(config.focus_roi_bottom_fraction)
            || config.focus_roi_left_fraction < 0.0
            || config.focus_roi_right_fraction < 0.0
            || config.focus_roi_top_fraction < 0.0
            || config.focus_roi_bottom_fraction < 0.0
            || config.focus_roi_left_fraction + config.focus_roi_right_fraction >= 1.0
            || config.focus_roi_top_fraction + config.focus_roi_bottom_fraction >= 1.0) {
            throw std::invalid_argument(
                "Live route matching focus ROI fractions must be finite, non-negative, and leave a non-empty window");
        }
        if (config.focus_roi_top_count == 0) {
            throw std::invalid_argument("Live route matching focus ROI top count must be positive");
        }
    }

    const auto route = read_route_signature_file(config.route_path);
    const auto route_summary = summarize_route_signature(route);
    Gray8RouteMatcherConfig matcher_config;
    matcher_config.window_radius = config.window_radius;
    matcher_config.minimum_confidence = config.minimum_confidence;
    matcher_config.max_direction_shift_px = config.max_direction_shift_px;
    matcher_config.radians_per_pixel = config.radians_per_pixel;
    matcher_config.enable_scale_refinement = config.scale_refinement_enabled;
    matcher_config.scale_refinement_radius = config.scale_refinement_radius;
    matcher_config.top_candidate_count =
        (config.top_match_diagnostics || config.endpoint_require_unambiguous_match)
            ? std::max<std::size_t>(config.top_match_count, 2)
            : 0;
    matcher_config.initial_progress_window_enabled = config.initial_progress_window_enabled;
    matcher_config.initial_progress_min = config.initial_progress_min;
    matcher_config.initial_progress_max = config.initial_progress_max;
    if (config.directional_search_enabled) {
        if (config.expected_progress == "reverse") {
            matcher_config.directional_search_direction = -1;
        } else if (config.expected_progress == "forward") {
            matcher_config.directional_search_direction = 1;
        }
    }
    matcher_config.directional_search_bias = config.directional_search_bias;

    PiCameraSource source(config.camera);
    Gray8ResizePreprocessor preprocessor(config.target_width, config.target_height);
    Gray8RouteMatcher matcher(route, matcher_config);
    std::optional<FocusRoiPixels> focus_roi;
    std::optional<Gray8RouteMatcher> focus_matcher;
    if (config.focus_roi_diagnostics) {
        focus_roi = focus_roi_pixels(
            config.target_width,
            config.target_height,
            config.focus_roi_left_fraction,
            config.focus_roi_right_fraction,
            config.focus_roi_top_fraction,
            config.focus_roi_bottom_fraction);
        if (focus_roi->width <= 0 || focus_roi->height <= 0) {
            throw std::invalid_argument("Live route matching focus ROI resolves to an empty window");
        }
        auto focus_matcher_config = matcher_config;
        focus_matcher_config.top_candidate_count = std::max<std::size_t>(config.focus_roi_top_count, 2);
        focus_matcher.emplace(crop_gray8_route(route, *focus_roi), focus_matcher_config);
    }
    std::optional<BoundedNavigator> navigator;
    if (config.emit_dry_run_commands) {
        navigator.emplace(config.navigator);
    }
    DryRunCommandSink command_sink(&metrics);
    std::optional<LiveMavlinkOutputAuditLog> session_audit_log;
    std::optional<DryRunCommandSink> session_dry_run_bridge;
    std::optional<PosixSerialByteTransport> live_output_transport;
    std::optional<LiveMavlinkSerialCommandWriter> live_output_writer;
    std::optional<LiveMavlinkBridge> live_output_bridge;
    std::optional<LiveMavlinkOutputSession> live_output_session;
    std::optional<LiveExternalNavOutputAuditLog> external_nav_output_audit_log;
    DisabledExternalNavWriter disabled_external_nav_writer;
    std::optional<PosixSerialByteTransport> external_nav_output_transport;
    std::optional<LiveMavlinkExternalNavWriter> external_nav_output_writer;
    std::optional<LiveExternalNavOutputSession> external_nav_output_session;
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
            << " stop_at_endpoint_progress=" << (config.stop_at_endpoint_progress ? "true" : "false")
            << " endpoint_dwell_ms=" << config.endpoint_dwell_ms
            << " endpoint_require_unambiguous_match=" << bool_word(config.endpoint_require_unambiguous_match)
            << " endpoint_min_top_match_gap=" << config.endpoint_min_top_match_gap
            << " endpoint_min_edge_top_match_gap=" << config.endpoint_min_edge_top_match_gap
            << " endpoint_allow_ambiguous_hold=" << bool_word(config.endpoint_allow_ambiguous_hold)
            << " endpoint_ambiguous_hold_dwell_ms=" << config.endpoint_ambiguous_hold_dwell_ms
            << " export_endpoint_stop_frame=" << bool_word(config.export_endpoint_stop_frame)
            << " endpoint_stop_frame_dir="
            << (config.endpoint_stop_frame_dir.empty() ? "none" : config.endpoint_stop_frame_dir.string())
            << " dry_run_commands=" << (config.emit_dry_run_commands ? "true" : "false")
            << " live_telemetry_stream=" << (config.use_live_telemetry_stream ? "true" : "false")
            << " telemetry_warmup_timeout_ms=" << config.telemetry_warmup_timeout_ms
            << " telemetry_max_age_ms=" << config.telemetry_max_age_ms
            << " require_live_telemetry_health=" << (config.require_live_telemetry_health ? "true" : "false")
            << " external_nav_output_build_requested="
            << (LiveMavlinkExternalNavWriter::build_requested() ? "true" : "false")
            << " external_nav_output_bench_scope="
            << (LiveMavlinkExternalNavWriter::bench_props_off_scope() ? "true" : "false")
            << " external_nav_output_available="
            << (LiveMavlinkExternalNavWriter::external_nav_output_available() ? "true" : "false")
            << " external_nav_writer_attached="
            << (LiveMavlinkExternalNavWriter::writer_attached() ? "true" : "false")
            << " external_nav_estimates=" << (config.emit_external_nav_estimates ? "true" : "false")
            << " visual_scale_diagnostics=" << (config.visual_scale_diagnostics ? "true" : "false")
            << " visual_scale_reference_altitude_m=" << config.visual_scale_reference_altitude_m
            << " scale_refinement=" << (matcher_config.enable_scale_refinement ? "true" : "false")
            << " scale_refinement_radius=" << matcher_config.scale_refinement_radius
            << " top_match_diagnostics=" << bool_word(config.top_match_diagnostics)
            << " top_match_count=" << matcher_config.top_candidate_count
            << " zone_probe_diagnostics=" << bool_word(config.zone_probe_diagnostics)
            << " edge_match_diagnostics=" << bool_word(config.edge_match_diagnostics)
            << " edge_match_top_count=" << config.edge_match_top_count
            << " focus_roi_diagnostics=" << bool_word(config.focus_roi_diagnostics)
            << " focus_roi_left_right_top_bottom=" << config.focus_roi_left_fraction
            << "/" << config.focus_roi_right_fraction
            << "/" << config.focus_roi_top_fraction
            << "/" << config.focus_roi_bottom_fraction
            << " focus_roi_top_count=" << config.focus_roi_top_count
            << " focus_roi_window="
            << (focus_roi ? focus_roi->x : 0)
            << "," << (focus_roi ? focus_roi->y : 0)
            << "," << (focus_roi ? focus_roi->width : 0)
            << "x" << (focus_roi ? focus_roi->height : 0)
            << " initial_progress_window=" << bool_word(config.initial_progress_window_enabled)
            << " initial_progress_min_max=" << config.initial_progress_min
            << "/" << config.initial_progress_max
            << " directional_search=" << bool_word(config.directional_search_enabled)
            << " directional_search_direction=" << matcher_config.directional_search_direction
            << " directional_search_bias=" << matcher_config.directional_search_bias;
    if (config.emit_external_nav_estimates) {
        metrics << " external_nav_nominal_route_length_m=" << config.external_nav.nominal_route_length_m
                << " external_nav_minimum_match_confidence=" << config.external_nav.minimum_match_confidence
                << " external_nav_maximum_altitude_age_ms=" << config.external_nav.maximum_altitude_age_ms
                << " external_nav_source=" << config.external_nav.source_tag
                << " external_nav_bench_diagnostic_altitude_m=" << config.external_nav.bench_diagnostic_altitude_m
                << " external_nav_expected_relative_altitude_m="
                << config.external_nav_expected_relative_altitude_m
                << " external_nav_expected_relative_altitude_tolerance_m="
                << config.external_nav_expected_relative_altitude_tolerance_m;
    }
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
            metrics << " session_audit_path=" << config.live_output_session_audit_path.string()
                    << " live_output_runtime_controls_provided="
                    << (config.live_output_runtime_controls_provided ? "true" : "false")
                    << " live_output_runtime_enabled=" << (config.live_output_runtime_enabled ? "true" : "false")
                    << " live_output_operator_confirmed=" << (config.live_output_operator_confirmed ? "true" : "false")
                    << " live_output_max_commands=" << config.live_output_max_commands
                    << " live_output_max_duration_ms=" << config.live_output_max_duration_ms
                    << " live_output_writer_attached="
                    << (LiveMavlinkBridge::command_output_available() ? "true" : "false");
#if VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE
            if (config.live_output_runtime_controls_provided) {
                metrics << " live_output_command_device=" << config.telemetry_stream.device_path
                        << " live_output_command_baud=" << config.telemetry_stream.baud_rate;
            }
#endif
        }
    }
    if (config.emit_external_nav_output_session_audit) {
        metrics << " external_nav_output_session_audit=true"
                << " external_nav_output_session_audit_path="
                << config.external_nav_output_session_audit_path.string()
                << " external_nav_output_runtime_controls_provided="
                << bool_word(config.external_nav_output_runtime_controls_provided)
                << " external_nav_output_runtime_enabled="
                << bool_word(config.external_nav_output_runtime_enabled)
                << " external_nav_output_operator_confirmed="
                << bool_word(config.external_nav_output_operator_confirmed)
                << " external_nav_output_single_writer_confirmed="
                << bool_word(config.external_nav_output_single_writer_confirmed)
                << " external_nav_output_max_messages=" << config.external_nav_output_max_messages
                << " external_nav_output_max_duration_ms=" << config.external_nav_output_max_duration_ms;
#if VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE
        if (config.external_nav_output_runtime_controls_provided) {
            metrics << " external_nav_output_device=" << config.telemetry_stream.device_path
                    << " external_nav_output_baud=" << config.telemetry_stream.baud_rate;
        }
#endif
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
            const auto validation = validate_mavlink_telemetry(
                telemetry.inspection,
                live_route_match_telemetry_validation_config(config));
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
                << " global_position_int_messages=" << result.telemetry_global_position_int_messages
                << " altitude_messages=" << result.telemetry_altitude_messages;
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
                    << " telemetry_altitude_messages=" << result.telemetry_altitude_messages
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

    if (config.emit_live_output_session_audit) {
        session_audit_log.emplace(LiveMavlinkOutputAuditLogConfig{config.live_output_session_audit_path, false});
        MavlinkBridge* session_bridge = nullptr;
#if VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE
        if (config.live_output_runtime_controls_provided) {
            LiveMavlinkSerialWriterConfig writer_config;
            writer_config.device_path = config.telemetry_stream.device_path;
            writer_config.baud_rate = config.telemetry_stream.baud_rate;
            writer_config.max_abs_yaw_rate_radps = config.max_abs_dry_run_yaw_rate_radps;
            live_output_transport.emplace(writer_config.device_path, writer_config.baud_rate);
            live_output_writer.emplace(writer_config, *live_output_transport);
            live_output_bridge.emplace(*live_output_writer);
            session_bridge = &*live_output_bridge;
        } else
#endif
        {
            session_dry_run_bridge.emplace(nullptr);
            session_bridge = &*session_dry_run_bridge;
        }
        live_output_session.emplace(
            LiveMavlinkOutputSessionConfig{
                "match_live_route",
                live_output_gate_config_from_match_config(config, true),
                config.live_output_max_commands,
                config.live_output_max_duration_ms,
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

    if (config.emit_external_nav_output_session_audit) {
        if (!config.emit_external_nav_estimates) {
            source.stop();
            command_sink.stop();
            if (live_output_session) {
                live_output_session->stop("external_nav_output_requires_external_nav_estimates");
            }
            if (telemetry_stream) {
                telemetry_stream->stop();
            }
            metrics << "live_route_match_external_nav_output_audit_start"
                    << " path=" << config.external_nav_output_session_audit_path.string()
                    << " started=false reason=external_nav_estimates_disabled\n";
            metrics << "live_route_match_done started=false warmup_frames_dropped=" << result.warmup_frames_dropped
                    << " frames_captured=0 valid_matches=0 progress_regressions=0 empty_polls=" << result.empty_polls
                    << " external_nav_output_session_audit_started=false passed=false\n";
            return result;
        }

        external_nav_output_audit_log.emplace(
            LiveExternalNavOutputAuditLogConfig{config.external_nav_output_session_audit_path, false});
        ExternalNavWriter* external_nav_writer = &disabled_external_nav_writer;
#if VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE
        if (config.external_nav_output_runtime_controls_provided) {
            LiveMavlinkExternalNavWriterConfig writer_config;
            writer_config.device_path = config.telemetry_stream.device_path;
            writer_config.baud_rate = config.telemetry_stream.baud_rate;
            external_nav_output_transport.emplace(writer_config.device_path, writer_config.baud_rate);
            external_nav_output_writer.emplace(writer_config, *external_nav_output_transport);
            external_nav_writer = &*external_nav_output_writer;
        }
#endif
        external_nav_output_session.emplace(
            LiveExternalNavOutputSessionConfig{
                "match_live_route_external_nav",
                LiveMavlinkExternalNavWriter::external_nav_output_available(),
                config.external_nav_output_runtime_enabled,
                config.external_nav_output_operator_confirmed,
                config.emit_external_nav_output_session_audit,
                config.external_nav_output_single_writer_confirmed,
                config.external_nav_output_max_messages,
                config.external_nav_output_max_duration_ms,
            },
            *external_nav_output_audit_log,
            *external_nav_writer);
        result.external_nav_output_session_audit_path =
            config.external_nav_output_session_audit_path.string();
        result.external_nav_output_session_audit_started = external_nav_output_session->start();
        metrics << "live_route_match_external_nav_output_audit_start"
                << " path=" << result.external_nav_output_session_audit_path
                << " started=" << bool_word(result.external_nav_output_session_audit_started) << "\n";
        if (!result.external_nav_output_session_audit_started) {
            source.stop();
            command_sink.stop();
            if (live_output_session) {
                live_output_session->stop("external_nav_output_audit_start_failed");
            }
            if (telemetry_stream) {
                telemetry_stream->stop();
            }
            metrics << "live_route_match_done started=false warmup_frames_dropped=" << result.warmup_frames_dropped
                    << " frames_captured=0 valid_matches=0 progress_regressions=0 empty_polls=" << result.empty_polls
                    << " external_nav_output_session_audit_started=false passed=false\n";
            return result;
        }
    }

    const auto capture_started_at = now();
    const auto capture_timeout_ms = 2000.0 + (static_cast<double>(config.frames_to_capture) * 1000.0
        / static_cast<double>(config.camera.frame_rate_hz));
    double confidence_sum = 0.0;
    std::optional<double> last_valid_progress;
    std::optional<double> last_tracked_progress;
    std::optional<double> previous_valid_command_yaw_rate;
    std::optional<LiveMavlinkOutputSafetySnapshot> last_live_output_gate_snapshot;
    MavlinkTelemetry latest_gate_telemetry;
    std::map<std::string, std::uint64_t> live_output_gate_block_reasons;
    std::map<std::string, std::uint64_t> external_nav_invalid_reasons;
    std::map<std::string, std::uint64_t> external_nav_output_block_reasons;
    double visual_scale_ratio_sum = 0.0;
    double visual_scale_confidence_sum = 0.0;
    std::map<double, std::uint64_t> visual_scale_ratio_histogram;
    std::optional<double> last_tracked_visual_scale_ratio;
    double tracked_visual_scale_ratio_sum = 0.0;
    double external_nav_relative_altitude_sum_m = 0.0;
    double top_match_gap_sum = 0.0;
    double end_zone_gap_sum = 0.0;
    double edge_top_match_gap_sum = 0.0;
    double edge_end_zone_gap_sum = 0.0;
    double focus_roi_confidence_sum = 0.0;
    double focus_roi_top_match_gap_sum = 0.0;
    std::uint64_t focus_roi_top_match_gap_frames = 0;
    std::uint64_t current_external_nav_invalid_streak = 0;
    std::uint64_t current_invalid_command_streak = 0;
    std::optional<Timestamp> endpoint_dwell_started_at;
    std::optional<Timestamp> ambiguous_endpoint_hold_started_at;
    result.endpoint_dwell_required_ms = config.endpoint_dwell_ms;
    result.endpoint_dwell_passed = config.endpoint_dwell_ms <= 0.0;
    result.endpoint_confirmation_required = config.endpoint_require_unambiguous_match;
    result.endpoint_confirmation_passed = !config.endpoint_require_unambiguous_match;
    result.endpoint_confirmation_reason =
        config.endpoint_require_unambiguous_match ? "not_evaluated" : "disabled";
    result.ambiguous_endpoint_hold_required_ms = config.endpoint_ambiguous_hold_dwell_ms;
    result.ambiguous_endpoint_hold_reason =
        config.endpoint_allow_ambiguous_hold ? "not_evaluated" : "disabled";
    while (result.frames_captured < config.frames_to_capture) {
        if (auto frame = source.poll()) {
            const auto processing_started = now();
            const auto processed = preprocessor.process(*frame);
            const auto match = matcher.match(processed);
            const auto top_candidates = matcher.recent_top_candidates();
            const auto zone_candidates = config.zone_probe_diagnostics
                ? matcher.probe_progress_zones(processed)
                : std::vector<RouteMatchZoneCandidate>{};
            const auto edge_diagnostics = (config.edge_match_diagnostics
                || config.endpoint_require_unambiguous_match)
                ? matcher.probe_edge_diagnostics(processed, config.edge_match_top_count)
                : RouteMatchEdgeDiagnostics{};
            std::optional<double> top_match_gap;
            if ((config.top_match_diagnostics || config.endpoint_require_unambiguous_match)
                && top_candidates.size() >= 2) {
                top_match_gap = top_candidates[0].confidence - top_candidates[1].confidence;
                if (config.top_match_diagnostics) {
                    ++result.top_match_diagnostic_frames;
                    top_match_gap_sum += *top_match_gap;
                    result.top_match_gap_min = result.top_match_diagnostic_frames == 1
                        ? *top_match_gap
                        : std::min(result.top_match_gap_min, *top_match_gap);
                }
            }
            std::optional<double> end_zone_gap;
            if (config.zone_probe_diagnostics) {
                if (const auto end_candidate = find_zone_candidate(zone_candidates, "end")) {
                    end_zone_gap = match.confidence - end_candidate->confidence;
                    ++result.zone_probe_diagnostic_frames;
                    end_zone_gap_sum += *end_zone_gap;
                    result.end_zone_gap_min = result.zone_probe_diagnostic_frames == 1
                        ? *end_zone_gap
                        : std::min(result.end_zone_gap_min, *end_zone_gap);
                }
            }
            std::optional<double> edge_top_match_gap;
            if ((config.edge_match_diagnostics || config.endpoint_require_unambiguous_match)
                && edge_diagnostics.top_candidates.size() >= 2) {
                edge_top_match_gap =
                    edge_diagnostics.top_candidates[0].confidence - edge_diagnostics.top_candidates[1].confidence;
                if (config.edge_match_diagnostics) {
                    ++result.edge_match_diagnostic_frames;
                    edge_top_match_gap_sum += *edge_top_match_gap;
                    result.edge_top_match_gap_min = result.edge_match_diagnostic_frames == 1
                        ? *edge_top_match_gap
                        : std::min(result.edge_top_match_gap_min, *edge_top_match_gap);
                }
            }
            std::optional<double> edge_end_zone_gap;
            if (config.edge_match_diagnostics && !edge_diagnostics.top_candidates.empty()) {
                if (const auto edge_end_candidate = find_zone_candidate(edge_diagnostics.zone_candidates, "end")) {
                    edge_end_zone_gap =
                        edge_diagnostics.top_candidates[0].confidence - edge_end_candidate->confidence;
                    edge_end_zone_gap_sum += *edge_end_zone_gap;
                    result.edge_end_zone_gap_min = result.edge_match_diagnostic_frames == 1
                        ? *edge_end_zone_gap
                        : std::min(result.edge_end_zone_gap_min, *edge_end_zone_gap);
                }
            }
            std::optional<RouteMatch> focus_match;
            std::optional<double> focus_top_match_gap;
            if (focus_matcher && focus_roi) {
                const auto focus_frame = crop_gray8_frame(processed, *focus_roi);
                focus_match = focus_matcher->match(focus_frame);
                const auto& focus_top_candidates = focus_matcher->recent_top_candidates();
                if (focus_top_candidates.size() >= 2) {
                    focus_top_match_gap =
                        focus_top_candidates[0].confidence - focus_top_candidates[1].confidence;
                    ++focus_roi_top_match_gap_frames;
                    focus_roi_top_match_gap_sum += *focus_top_match_gap;
                    result.focus_roi_top_match_gap_min = focus_roi_top_match_gap_frames == 1
                        ? *focus_top_match_gap
                        : std::min(result.focus_roi_top_match_gap_min, *focus_top_match_gap);
                }
                ++result.focus_roi_frames;
                focus_roi_confidence_sum += focus_match->confidence;
                if (result.focus_roi_frames == 1) {
                    result.focus_roi_confidence_min = focus_match->confidence;
                    result.focus_roi_first_progress = focus_match->progress;
                    result.focus_roi_min_progress_seen = focus_match->progress;
                    result.focus_roi_max_progress_seen = focus_match->progress;
                } else {
                    result.focus_roi_confidence_min =
                        std::min(result.focus_roi_confidence_min, focus_match->confidence);
                    result.focus_roi_min_progress_seen =
                        std::min(result.focus_roi_min_progress_seen, focus_match->progress);
                    result.focus_roi_max_progress_seen =
                        std::max(result.focus_roi_max_progress_seen, focus_match->progress);
                }
                result.focus_roi_last_progress = focus_match->progress;
                if (focus_match->valid) {
                    ++result.focus_roi_valid_matches;
                }
                if (focus_match->route_index == match.route_index) {
                    ++result.focus_roi_route_index_agreements;
                }
                if (live_route_match_endpoint_reached(config, focus_match->progress)
                    == live_route_match_endpoint_reached(config, match.progress)) {
                    ++result.focus_roi_endpoint_agreements;
                }
            }
            const auto processing_finished = now();
            const auto timing = health.observe_processed_frame(processed, processing_started, processing_finished);
            health.set_route_match_confidence(match.confidence);
            if (telemetry_stream) {
                const auto telemetry = telemetry_stream->snapshot();
                const auto validation = validate_mavlink_telemetry(
                    telemetry.inspection,
                    live_route_match_telemetry_validation_config(config));
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
            if (config.emit_external_nav_estimates) {
                auto external_nav_estimate = make_route_progress_external_nav_estimate(
                    match,
                    route_summary,
                    latest_gate_telemetry,
                    processing_finished,
                    config.external_nav);
                const auto visual_scale_reference_altitude_m = config.visual_scale_diagnostics
                    ? config.visual_scale_reference_altitude_m
                    : config.external_nav.bench_diagnostic_altitude_m;
                if (match.route_index < route.entries.size() && visual_scale_reference_altitude_m > 0.0) {
                    const auto visual_scale = estimate_visual_scale_diagnostic(
                        processed,
                        route.entries[static_cast<std::size_t>(match.route_index)],
                        visual_scale_reference_altitude_m);
                    external_nav_estimate.visual_scale_valid = visual_scale.valid;
                    external_nav_estimate.visual_scale_ratio = visual_scale.scale_ratio;
                    external_nav_estimate.visual_altitude_m = visual_scale.altitude_m;
                    external_nav_estimate.visual_scale_confidence = visual_scale.confidence;
                }
                ++result.external_nav_estimates;
                if (external_nav_estimate.altitude_valid) {
                    ++result.external_nav_altitude_valid_frames;
                } else if (external_nav_estimate.bench_diagnostic_altitude_used) {
                    ++result.external_nav_bench_altitude_frames;
                } else {
                    ++result.external_nav_altitude_invalid_frames;
                }
                if (external_nav_estimate.relative_altitude_seen
                    && std::isfinite(external_nav_estimate.relative_altitude_m)) {
                    ++result.external_nav_relative_altitude_seen_frames;
                    external_nav_relative_altitude_sum_m += external_nav_estimate.relative_altitude_m;
                    if (result.external_nav_relative_altitude_seen_frames == 1) {
                        result.external_nav_relative_altitude_min_m = external_nav_estimate.relative_altitude_m;
                        result.external_nav_relative_altitude_max_m = external_nav_estimate.relative_altitude_m;
                    } else {
                        result.external_nav_relative_altitude_min_m = std::min(
                            result.external_nav_relative_altitude_min_m,
                            external_nav_estimate.relative_altitude_m);
                        result.external_nav_relative_altitude_max_m = std::max(
                            result.external_nav_relative_altitude_max_m,
                            external_nav_estimate.relative_altitude_m);
                    }
                }
                if (external_nav_estimate.visual_scale_valid) {
                    ++result.visual_scale_valid;
                    visual_scale_ratio_sum += external_nav_estimate.visual_scale_ratio;
                    visual_scale_confidence_sum += external_nav_estimate.visual_scale_confidence;
                    ++visual_scale_ratio_histogram[external_nav_estimate.visual_scale_ratio];
                    const auto tracked_visual_scale_ratio = last_tracked_visual_scale_ratio
                        ? live_route_match_next_tracked_visual_scale_ratio(
                            *last_tracked_visual_scale_ratio,
                            external_nav_estimate.visual_scale_ratio)
                        : external_nav_estimate.visual_scale_ratio;
                    last_tracked_visual_scale_ratio = tracked_visual_scale_ratio;
                    tracked_visual_scale_ratio_sum += tracked_visual_scale_ratio;
                    if (result.visual_scale_valid == 1) {
                        result.visual_scale_ratio_min = external_nav_estimate.visual_scale_ratio;
                        result.visual_scale_ratio_max = external_nav_estimate.visual_scale_ratio;
                        result.visual_scale_confidence_min = external_nav_estimate.visual_scale_confidence;
                        result.first_tracked_visual_scale_ratio = tracked_visual_scale_ratio;
                        result.tracked_visual_scale_ratio_min = tracked_visual_scale_ratio;
                        result.tracked_visual_scale_ratio_max = tracked_visual_scale_ratio;
                    } else {
                        result.visual_scale_ratio_min =
                            std::min(result.visual_scale_ratio_min, external_nav_estimate.visual_scale_ratio);
                        result.visual_scale_ratio_max =
                            std::max(result.visual_scale_ratio_max, external_nav_estimate.visual_scale_ratio);
                        result.visual_scale_confidence_min =
                            std::min(result.visual_scale_confidence_min, external_nav_estimate.visual_scale_confidence);
                        result.tracked_visual_scale_ratio_min =
                            std::min(result.tracked_visual_scale_ratio_min, tracked_visual_scale_ratio);
                        result.tracked_visual_scale_ratio_max =
                            std::max(result.tracked_visual_scale_ratio_max, tracked_visual_scale_ratio);
                    }
                    result.last_tracked_visual_scale_ratio = tracked_visual_scale_ratio;
                }
                if (external_nav_estimate.valid_for_fc) {
                    ++result.external_nav_valid_for_fc;
                    current_external_nav_invalid_streak = 0;
                } else {
                    ++external_nav_invalid_reasons[external_nav_estimate.reason];
                    ++current_external_nav_invalid_streak;
                    result.external_nav_max_invalid_streak =
                        std::max(result.external_nav_max_invalid_streak, current_external_nav_invalid_streak);
                }
                metrics << external_nav_estimate_log_line(external_nav_estimate) << "\n";
                if (external_nav_output_session) {
                    const auto output_result = external_nav_output_session->process(
                        LiveExternalNavOutputSnapshot{
                            processing_finished,
                            external_nav_estimate,
                            monotonic_time_usec(processing_finished),
                        });
                    result.final_external_nav_output_reason = output_result.reason;
                    if (output_result.allowed) {
                        ++result.external_nav_output_allowed_frames;
                    } else {
                        ++result.external_nav_output_blocked_frames;
                        ++external_nav_output_block_reasons[output_result.reason];
                    }
                    if (output_result.sent) {
                        ++result.external_nav_output_sent_frames;
                    }
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

            std::optional<double> current_tracked_progress;
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

                const auto tracked_progress = last_tracked_progress
                    ? live_route_match_next_tracked_progress(
                        config.expected_progress,
                        *last_tracked_progress,
                        match.progress)
                    : match.progress;
                if (!last_tracked_progress) {
                    result.first_tracked_progress = tracked_progress;
                    result.min_tracked_progress_seen = tracked_progress;
                    result.max_tracked_progress_seen = tracked_progress;
                } else {
                    if (tracked_progress < *last_tracked_progress) {
                        const auto rollback = *last_tracked_progress - tracked_progress;
                        if (rollback > kTrackedProgressRegressionDeadband) {
                            ++result.tracked_progress_regressions;
                        }
                        result.tracked_progress_rollback += rollback;
                        result.tracked_progress_monotonic = false;
                    }
                    if (tracked_progress > *last_tracked_progress) {
                        const auto rollback = tracked_progress - *last_tracked_progress;
                        if (rollback > kTrackedProgressRegressionDeadband) {
                            ++result.tracked_reverse_progress_regressions;
                        }
                        result.tracked_reverse_progress_rollback += rollback;
                        result.tracked_reverse_progress_monotonic = false;
                    }
                    result.min_tracked_progress_seen =
                        std::min(result.min_tracked_progress_seen, tracked_progress);
                    result.max_tracked_progress_seen =
                        std::max(result.max_tracked_progress_seen, tracked_progress);
                }
                result.last_tracked_progress = tracked_progress;
                last_tracked_progress = tracked_progress;
                current_tracked_progress = tracked_progress;
            }

            metrics << "live_route_match_frame id=" << processed.id
                    << " size=" << processed.width << "x" << processed.height
                    << " bytes=" << processed.data.size()
                    << " route_index=" << match.route_index
                    << " progress=" << match.progress
                    << " tracked_progress=" << (last_tracked_progress ? *last_tracked_progress : 0.0)
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
            if (config.top_match_diagnostics) {
                metrics << " top_matches=" << format_top_match_candidates(top_candidates)
                        << " top_match_gap=" << (top_match_gap ? *top_match_gap : 0.0);
            }
            if (config.zone_probe_diagnostics) {
                metrics << " zone_probes=" << format_zone_probe_candidates(zone_candidates)
                        << " end_zone_gap_vs_best=" << (end_zone_gap ? *end_zone_gap : 0.0);
            }
            if (config.edge_match_diagnostics) {
                metrics << " edge_top_matches=" << format_top_match_candidates(edge_diagnostics.top_candidates)
                        << " edge_top_match_gap=" << (edge_top_match_gap ? *edge_top_match_gap : 0.0)
                        << " edge_zone_probes=" << format_zone_probe_candidates(edge_diagnostics.zone_candidates)
                        << " edge_end_zone_gap_vs_best=" << (edge_end_zone_gap ? *edge_end_zone_gap : 0.0);
            }
            if (focus_match) {
                metrics << " focus_roi_enabled=true"
                        << " focus_roi_route_index=" << focus_match->route_index
                        << " focus_roi_progress=" << focus_match->progress
                        << " focus_roi_confidence=" << focus_match->confidence
                        << " focus_roi_valid=" << bool_word(focus_match->valid)
                        << " focus_roi_top_match_gap=" << (focus_top_match_gap ? *focus_top_match_gap : 0.0)
                        << " focus_roi_agrees_route_index="
                        << bool_word(focus_match->route_index == match.route_index)
                        << " focus_roi_agrees_endpoint="
                        << bool_word(
                            live_route_match_endpoint_reached(config, focus_match->progress)
                            == live_route_match_endpoint_reached(config, match.progress));
            }
            metrics << "\n";

            if (config.stop_at_endpoint_progress && match.valid && current_tracked_progress) {
                if (live_route_match_endpoint_reached(config, *current_tracked_progress)) {
                    bool endpoint_confirmed = true;
                    if (config.endpoint_require_unambiguous_match) {
                        endpoint_confirmed = false;
                        result.endpoint_confirmation_required = true;
                        result.endpoint_confirmation_passed = false;
                        if (!top_match_gap) {
                            result.endpoint_confirmation_reason = "top_match_gap_unavailable";
                        } else if (!edge_top_match_gap) {
                            result.endpoint_confirmation_reason = "edge_top_match_gap_unavailable";
                        } else {
                            result.endpoint_top_match_gap = *top_match_gap;
                            result.endpoint_edge_top_match_gap = *edge_top_match_gap;
                            endpoint_confirmed = live_route_match_endpoint_confirmation_passed(
                                config,
                                *top_match_gap,
                                *edge_top_match_gap);
                            result.endpoint_confirmation_passed = endpoint_confirmed;
                            if (*top_match_gap < config.endpoint_min_top_match_gap) {
                                result.endpoint_confirmation_reason = "top_match_gap_low";
                            } else if (*edge_top_match_gap < config.endpoint_min_edge_top_match_gap) {
                                result.endpoint_confirmation_reason = "edge_top_match_gap_low";
                            } else {
                                result.endpoint_confirmation_reason = "valid";
                            }
                        }
                    }
                    if (endpoint_confirmed) {
                        ambiguous_endpoint_hold_started_at.reset();
                        result.ambiguous_endpoint_hold_dwell_ms = 0.0;
                        result.ambiguous_endpoint_hold_reason =
                            config.endpoint_allow_ambiguous_hold ? "endpoint_confirmed" : "disabled";
                        if (!endpoint_dwell_started_at) {
                            endpoint_dwell_started_at = processing_finished;
                        }
                        result.endpoint_dwell_ms = milliseconds_between(*endpoint_dwell_started_at, processing_finished);
                        result.endpoint_dwell_passed = result.endpoint_dwell_ms >= config.endpoint_dwell_ms;
                    } else {
                        endpoint_dwell_started_at.reset();
                        result.endpoint_dwell_ms = 0.0;
                        result.endpoint_dwell_passed = false;
                        if (config.endpoint_allow_ambiguous_hold) {
                            if (!ambiguous_endpoint_hold_started_at) {
                                ambiguous_endpoint_hold_started_at = processing_finished;
                            }
                            result.ambiguous_endpoint_hold_dwell_ms =
                                milliseconds_between(*ambiguous_endpoint_hold_started_at, processing_finished);
                            result.ambiguous_endpoint_hold_reason = result.endpoint_confirmation_reason;
                            result.ambiguous_endpoint_hold_frame_id = processed.id;
                            result.ambiguous_endpoint_hold_route_index =
                                static_cast<std::uint64_t>(match.route_index);
                            result.ambiguous_endpoint_hold_progress = match.progress;
                            result.ambiguous_endpoint_hold_tracked_progress = *current_tracked_progress;
                            result.ambiguous_endpoint_hold_confidence = match.confidence;
                            if (result.ambiguous_endpoint_hold_dwell_ms
                                >= config.endpoint_ambiguous_hold_dwell_ms) {
                                result.ambiguous_endpoint_hold_triggered = true;
                                result.stop_reason = "ambiguous_endpoint_hold";
                                break;
                            }
                        }
                    }
                    if (endpoint_confirmed && result.endpoint_dwell_passed) {
                        result.endpoint_stop_triggered = true;
                        result.stop_reason = "endpoint_progress_reached";
                        result.endpoint_stop_frame_id = processed.id;
                        result.endpoint_stop_frame_width = processed.width;
                        result.endpoint_stop_frame_height = processed.height;
                        result.endpoint_stop_route_index = static_cast<std::uint64_t>(match.route_index);
                        result.endpoint_stop_progress = match.progress;
                        result.endpoint_stop_tracked_progress = *current_tracked_progress;
                        result.endpoint_stop_confidence = match.confidence;
                        if (config.export_endpoint_stop_frame) {
                            const auto filename = std::string("endpoint-stop-frame-")
                                + wall_time_utc_compact()
                                + "-id-" + std::to_string(processed.id)
                                + "-route-" + std::to_string(match.route_index)
                                + ".pgm";
                            const auto path = config.endpoint_stop_frame_dir / filename;
                            write_gray8_frame_pgm(path, processed);
                            result.endpoint_stop_frame_written = true;
                            result.endpoint_stop_frame_path = path.string();
                            metrics << "live_route_match_endpoint_stop_frame"
                                    << " path=" << result.endpoint_stop_frame_path
                                    << " frame_id=" << result.endpoint_stop_frame_id
                                    << " width=" << result.endpoint_stop_frame_width
                                    << " height=" << result.endpoint_stop_frame_height
                                    << " route_index=" << result.endpoint_stop_route_index
                                    << " progress=" << result.endpoint_stop_progress
                                    << " tracked_progress=" << result.endpoint_stop_tracked_progress
                                    << " confidence=" << result.endpoint_stop_confidence
                                    << "\n";
                        }
                        break;
                    }
                } else {
                    endpoint_dwell_started_at.reset();
                    ambiguous_endpoint_hold_started_at.reset();
                    result.endpoint_dwell_ms = 0.0;
                    result.endpoint_dwell_passed = config.endpoint_dwell_ms <= 0.0;
                    result.endpoint_confirmation_passed = !config.endpoint_require_unambiguous_match;
                    result.endpoint_confirmation_reason =
                        config.endpoint_require_unambiguous_match ? "not_at_endpoint" : "disabled";
                    result.ambiguous_endpoint_hold_dwell_ms = 0.0;
                    result.ambiguous_endpoint_hold_reason =
                        config.endpoint_allow_ambiguous_hold ? "not_at_endpoint" : "disabled";
                }
            }
        } else {
            ++result.empty_polls;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (milliseconds_between(capture_started_at, now()) > capture_timeout_ms) {
            break;
        }
    }

    source.stop();
    if (result.stop_reason == "not_started") {
        result.stop_reason =
            result.frames_captured == static_cast<std::uint64_t>(config.frames_to_capture)
                ? "frame_limit_reached"
                : "capture_timeout";
    }
    if (live_output_session) {
        live_output_session->stop(
            result.endpoint_stop_triggered
                ? "endpoint_progress_reached"
                : (result.ambiguous_endpoint_hold_triggered
                    ? "ambiguous_endpoint_hold"
                    : "match_live_route_complete"));
    }
    if (external_nav_output_session) {
        external_nav_output_session->stop(
            result.endpoint_stop_triggered
                ? "endpoint_progress_reached"
                : (result.ambiguous_endpoint_hold_triggered
                    ? "ambiguous_endpoint_hold"
                    : "match_live_route_complete"));
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
    result.top_match_gap_avg = result.top_match_diagnostic_frames > 0
        ? top_match_gap_sum / static_cast<double>(result.top_match_diagnostic_frames)
        : 0.0;
    result.end_zone_gap_avg = result.zone_probe_diagnostic_frames > 0
        ? end_zone_gap_sum / static_cast<double>(result.zone_probe_diagnostic_frames)
        : 0.0;
    result.edge_top_match_gap_avg = result.edge_match_diagnostic_frames > 0
        ? edge_top_match_gap_sum / static_cast<double>(result.edge_match_diagnostic_frames)
        : 0.0;
    result.edge_end_zone_gap_avg = result.edge_match_diagnostic_frames > 0
        ? edge_end_zone_gap_sum / static_cast<double>(result.edge_match_diagnostic_frames)
        : 0.0;
    if (result.focus_roi_frames > 0) {
        result.focus_roi_valid_fraction =
            static_cast<double>(result.focus_roi_valid_matches) / static_cast<double>(result.focus_roi_frames);
        result.focus_roi_route_index_agreement_fraction =
            static_cast<double>(result.focus_roi_route_index_agreements) / static_cast<double>(result.focus_roi_frames);
        result.focus_roi_endpoint_agreement_fraction =
            static_cast<double>(result.focus_roi_endpoint_agreements) / static_cast<double>(result.focus_roi_frames);
        result.focus_roi_confidence_avg =
            focus_roi_confidence_sum / static_cast<double>(result.focus_roi_frames);
    }
    result.focus_roi_top_match_gap_avg = focus_roi_top_match_gap_frames > 0
        ? focus_roi_top_match_gap_sum / static_cast<double>(focus_roi_top_match_gap_frames)
        : 0.0;
    if (config.expected_progress == "forward") {
        result.directional_progress_passed =
            result.progress_regressions <= config.max_progress_regressions
            && result.progress_rollback <= config.max_progress_rollback;
        result.tracked_directional_progress_passed =
            result.tracked_progress_regressions <= config.max_progress_regressions
            && result.tracked_progress_rollback <= config.max_progress_rollback;
    } else if (config.expected_progress == "reverse") {
        result.directional_progress_passed =
            result.reverse_progress_regressions <= config.max_progress_regressions
            && result.reverse_progress_rollback <= config.max_progress_rollback;
        result.tracked_directional_progress_passed =
            result.tracked_reverse_progress_regressions <= config.max_progress_regressions
            && result.tracked_reverse_progress_rollback <= config.max_progress_rollback;
    } else {
        result.directional_progress_passed = true;
        result.tracked_directional_progress_passed = true;
    }

    result.endpoint_progress_passed = live_route_match_endpoint_progress_passed(config, result);

    result.progress_gate_passed = config.require_endpoint_progress
        ? result.endpoint_progress_passed
        : result.directional_progress_passed;
    if (config.live_output_runtime_controls_provided) {
        result.progress_gate_passed = result.progress_gate_passed && result.directional_progress_passed;
    }

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
    result.external_nav_invalid_reasons = format_reason_counts(external_nav_invalid_reasons);
    result.external_nav_output_block_reasons = format_reason_counts(external_nav_output_block_reasons);
    if (result.external_nav_estimates > 0) {
        result.external_nav_valid_fraction =
            static_cast<double>(result.external_nav_valid_for_fc) / static_cast<double>(result.external_nav_estimates);
        result.visual_scale_valid_fraction =
            static_cast<double>(result.visual_scale_valid) / static_cast<double>(result.external_nav_estimates);
    }
    if (result.external_nav_relative_altitude_seen_frames > 0) {
        result.external_nav_relative_altitude_avg_m = external_nav_relative_altitude_sum_m
            / static_cast<double>(result.external_nav_relative_altitude_seen_frames);
    }
    result.external_nav_expected_relative_altitude_required = config.emit_external_nav_estimates
        && std::isfinite(config.external_nav_expected_relative_altitude_m)
        && config.external_nav_expected_relative_altitude_m > 0.0
        && std::isfinite(config.external_nav_expected_relative_altitude_tolerance_m)
        && config.external_nav_expected_relative_altitude_tolerance_m > 0.0;
    if (result.external_nav_expected_relative_altitude_required) {
        const auto altitude_min_allowed = config.external_nav_expected_relative_altitude_m
            - config.external_nav_expected_relative_altitude_tolerance_m;
        const auto altitude_max_allowed = config.external_nav_expected_relative_altitude_m
            + config.external_nav_expected_relative_altitude_tolerance_m;
        result.external_nav_relative_altitude_window_passed =
            result.external_nav_relative_altitude_seen_frames == result.external_nav_estimates
            && result.external_nav_relative_altitude_min_m >= altitude_min_allowed
            && result.external_nav_relative_altitude_max_m <= altitude_max_allowed;
    }
    if (result.visual_scale_valid > 0) {
        result.visual_scale_ratio_avg = visual_scale_ratio_sum / static_cast<double>(result.visual_scale_valid);
        result.visual_scale_confidence_avg =
            visual_scale_confidence_sum / static_cast<double>(result.visual_scale_valid);
        result.tracked_visual_scale_ratio_avg =
            tracked_visual_scale_ratio_sum / static_cast<double>(result.visual_scale_valid);
    }
    result.visual_scale_ratio_histogram = ratio_histogram_text(visual_scale_ratio_histogram);
    result.external_nav_latest_telemetry_armed = latest_gate_telemetry.armed;
    result.external_nav_latest_telemetry_mode = to_string(latest_gate_telemetry.mode);
    if (result.external_nav_estimates == 0) {
        result.external_nav_altitude_blocker = "not_requested";
    } else if (result.external_nav_altitude_valid_frames > 0) {
        result.external_nav_altitude_blocker = "none";
    } else if (result.external_nav_relative_altitude_seen_frames == 0) {
        result.external_nav_altitude_blocker = "relative_altitude_not_seen";
    } else if (result.external_nav_relative_altitude_max_m <= 0.0) {
        result.external_nav_altitude_blocker = "relative_altitude_non_positive";
    } else if (result.external_nav_bench_altitude_frames > 0) {
        result.external_nav_altitude_blocker = "bench_diagnostic_altitude_used";
    } else {
        result.external_nav_altitude_blocker = "altitude_not_valid";
    }
    result.visual_scale_required = config.emit_external_nav_estimates
        && std::isfinite(config.external_nav.bench_diagnostic_altitude_m)
        && config.external_nav.bench_diagnostic_altitude_m > 0.0;

    result.passed = result.started
        && live_route_match_has_required_frame_count(config, result)
        && result.valid_matches == result.frames_captured
        && result.progress_gate_passed
        && (!config.require_live_telemetry_health || result.live_telemetry_health_passed)
        && (!config.require_dry_run_command_quality || result.dry_run_command_quality_passed);

    if (result.external_nav_estimates == 0) {
        result.external_nav_strict_session_reason = "not_requested";
    } else if (result.ambiguous_endpoint_hold_triggered) {
        result.external_nav_strict_session_reason = "ambiguous_endpoint_hold";
    } else if (!result.passed) {
        result.external_nav_strict_session_reason = "route_session_not_passed";
    } else if (result.external_nav_valid_for_fc != result.external_nav_estimates) {
        result.external_nav_strict_session_reason = "per_frame_external_nav_invalid";
    } else {
        result.external_nav_strict_session_ready = true;
        result.external_nav_strict_session_reason = "valid";
    }
    result.external_nav_session_valid_for_fc = result.external_nav_valid_for_fc;

    constexpr double kExternalNavMinimumValidFraction = 0.95;
    constexpr std::uint64_t kExternalNavMaxInvalidStreak = 3;
    constexpr double kVisualScaleMinimumValidFraction = 0.95;
    constexpr double kVisualScaleMinimumRatio = 0.80;
    constexpr double kVisualScaleMaximumRatio = 1.25;
    if (result.external_nav_estimates == 0) {
        result.external_nav_quality_reason = "not_requested";
    } else if (result.ambiguous_endpoint_hold_triggered) {
        result.external_nav_quality_reason = "ambiguous_endpoint_hold";
    } else if (!result.passed) {
        result.external_nav_quality_reason = "route_session_not_passed";
    } else if (config.require_live_telemetry_health && !result.live_telemetry_health_passed) {
        result.external_nav_quality_reason = "telemetry_health_not_passed";
    } else if (result.external_nav_expected_relative_altitude_required
               && !result.external_nav_relative_altitude_window_passed) {
        result.external_nav_quality_reason = "relative_altitude_out_of_expected_window";
    } else if (result.external_nav_valid_fraction < kExternalNavMinimumValidFraction) {
        result.external_nav_quality_reason = "external_nav_valid_fraction_low";
    } else if (result.external_nav_max_invalid_streak > kExternalNavMaxInvalidStreak) {
        result.external_nav_quality_reason = "external_nav_invalid_streak_high";
    } else if (result.visual_scale_required
               && result.visual_scale_valid_fraction < kVisualScaleMinimumValidFraction) {
        result.external_nav_quality_reason = "visual_scale_valid_fraction_low";
    } else if (result.visual_scale_required
               && (result.visual_scale_ratio_min < kVisualScaleMinimumRatio
                   || result.visual_scale_ratio_max > kVisualScaleMaximumRatio)) {
        result.external_nav_quality_reason = "visual_scale_ratio_out_of_range";
    } else {
        result.external_nav_quality_ready = true;
        result.external_nav_quality_reason = "valid";
    }
    result.external_nav_session_ready = result.external_nav_quality_ready;
    result.external_nav_session_reason = result.external_nav_quality_reason;
    constexpr std::uint64_t kExternalNavOperatorMaxProgressRegressions = 15;
    constexpr double kExternalNavOperatorMaxProgressRollback = 1.0;
    const auto operator_progress_regressions = config.expected_progress == "reverse"
        ? result.tracked_reverse_progress_regressions
        : result.tracked_progress_regressions;
    const auto operator_progress_rollback = config.expected_progress == "reverse"
        ? result.tracked_reverse_progress_rollback
        : result.tracked_progress_rollback;
    const auto operator_directional_progress_soft_passed =
        config.expected_progress == "any"
        || (operator_progress_regressions <= kExternalNavOperatorMaxProgressRegressions
            && operator_progress_rollback <= kExternalNavOperatorMaxProgressRollback);
    if (result.external_nav_estimates == 0) {
        result.external_nav_operator_readiness = "not_requested";
        result.external_nav_operator_reason = "not_requested";
    } else if (result.ambiguous_endpoint_hold_triggered) {
        result.external_nav_operator_readiness = "marginal";
        result.external_nav_operator_reason = "ambiguous_endpoint_hold";
    } else if (!result.external_nav_quality_ready) {
        result.external_nav_operator_readiness = "blocked";
        result.external_nav_operator_reason = result.external_nav_quality_reason;
    } else if (!operator_directional_progress_soft_passed) {
        result.external_nav_operator_readiness = "marginal";
        result.external_nav_operator_reason = "route_tracked_directional_progress_soft_high";
    } else if (!result.external_nav_strict_session_ready) {
        result.external_nav_operator_readiness = "marginal";
        result.external_nav_operator_reason = "external_nav_strict_session_not_ready";
    } else {
        result.external_nav_operator_readiness = "ready";
        result.external_nav_operator_reason = "valid";
    }

    metrics << "live_route_match_done started=true"
            << " warmup_frames_dropped=" << result.warmup_frames_dropped
            << " frames_captured=" << result.frames_captured
            << " valid_matches=" << result.valid_matches
            << " progress_regressions=" << result.progress_regressions
            << " reverse_progress_regressions=" << result.reverse_progress_regressions
            << " progress_rollback=" << result.progress_rollback
            << " reverse_progress_rollback=" << result.reverse_progress_rollback
            << " tracked_progress_regressions=" << result.tracked_progress_regressions
            << " tracked_reverse_progress_regressions=" << result.tracked_reverse_progress_regressions
            << " tracked_progress_rollback=" << result.tracked_progress_rollback
            << " tracked_reverse_progress_rollback=" << result.tracked_reverse_progress_rollback
            << " max_progress_regressions=" << config.max_progress_regressions
            << " max_progress_rollback=" << config.max_progress_rollback
            << " empty_polls=" << result.empty_polls
            << " minimum_confidence_seen=" << result.minimum_confidence_seen
            << " average_confidence=" << result.average_confidence
            << " top_match_diagnostics=" << bool_word(config.top_match_diagnostics)
            << " top_match_frames=" << result.top_match_diagnostic_frames
            << " top_match_gap_min_avg=" << result.top_match_gap_min
            << "/" << result.top_match_gap_avg
            << " zone_probe_diagnostics=" << bool_word(config.zone_probe_diagnostics)
            << " zone_probe_frames=" << result.zone_probe_diagnostic_frames
            << " end_zone_gap_min_avg=" << result.end_zone_gap_min
            << "/" << result.end_zone_gap_avg
            << " edge_match_diagnostics=" << bool_word(config.edge_match_diagnostics)
            << " edge_match_frames=" << result.edge_match_diagnostic_frames
            << " edge_top_match_gap_min_avg=" << result.edge_top_match_gap_min
            << "/" << result.edge_top_match_gap_avg
            << " edge_end_zone_gap_min_avg=" << result.edge_end_zone_gap_min
            << "/" << result.edge_end_zone_gap_avg
            << " focus_roi_diagnostics=" << bool_word(config.focus_roi_diagnostics)
            << " focus_roi_frames=" << result.focus_roi_frames
            << " focus_roi_valid=" << result.focus_roi_valid_matches
            << "/" << result.focus_roi_frames
            << " focus_roi_valid_fraction=" << result.focus_roi_valid_fraction
            << " focus_roi_confidence_min_avg=" << result.focus_roi_confidence_min
            << "/" << result.focus_roi_confidence_avg
            << " focus_roi_progress=" << result.focus_roi_first_progress
            << ".." << result.focus_roi_last_progress
            << " focus_roi_minmax_progress=" << result.focus_roi_min_progress_seen
            << ".." << result.focus_roi_max_progress_seen
            << " focus_roi_route_index_agreement=" << result.focus_roi_route_index_agreements
            << "/" << result.focus_roi_frames
            << " focus_roi_route_index_agreement_fraction="
            << result.focus_roi_route_index_agreement_fraction
            << " focus_roi_endpoint_agreement=" << result.focus_roi_endpoint_agreements
            << "/" << result.focus_roi_frames
            << " focus_roi_endpoint_agreement_fraction=" << result.focus_roi_endpoint_agreement_fraction
            << " focus_roi_top_match_gap_min_avg=" << result.focus_roi_top_match_gap_min
            << "/" << result.focus_roi_top_match_gap_avg
            << " first_progress=" << result.first_progress
            << " last_progress=" << result.last_progress
            << " min_progress_seen=" << result.min_progress_seen
            << " max_progress_seen=" << result.max_progress_seen
            << " first_tracked_progress=" << result.first_tracked_progress
            << " last_tracked_progress=" << result.last_tracked_progress
            << " min_tracked_progress_seen=" << result.min_tracked_progress_seen
            << " max_tracked_progress_seen=" << result.max_tracked_progress_seen
            << " progress_monotonic=" << (result.progress_monotonic ? "true" : "false")
            << " reverse_progress_monotonic=" << (result.reverse_progress_monotonic ? "true" : "false")
            << " tracked_progress_monotonic=" << bool_word(result.tracked_progress_monotonic)
            << " tracked_reverse_progress_monotonic=" << bool_word(result.tracked_reverse_progress_monotonic)
            << " expected_progress=" << config.expected_progress
            << " directional_progress_passed=" << (result.directional_progress_passed ? "true" : "false")
            << " tracked_directional_progress_passed=" << bool_word(result.tracked_directional_progress_passed)
            << " endpoint_progress_passed=" << (result.endpoint_progress_passed ? "true" : "false")
            << " progress_gate_passed=" << (result.progress_gate_passed ? "true" : "false")
            << " endpoint_stop_triggered=" << (result.endpoint_stop_triggered ? "true" : "false")
            << " endpoint_dwell_ms=" << result.endpoint_dwell_ms
            << " endpoint_dwell_required_ms=" << result.endpoint_dwell_required_ms
            << " endpoint_dwell_passed=" << bool_word(result.endpoint_dwell_passed)
            << " endpoint_confirmation_required=" << bool_word(result.endpoint_confirmation_required)
            << " endpoint_confirmation_passed=" << bool_word(result.endpoint_confirmation_passed)
            << " endpoint_confirmation_reason=" << result.endpoint_confirmation_reason
            << " endpoint_top_match_gap=" << result.endpoint_top_match_gap
            << " endpoint_edge_top_match_gap=" << result.endpoint_edge_top_match_gap
            << " ambiguous_endpoint_hold_triggered=" << bool_word(result.ambiguous_endpoint_hold_triggered)
            << " ambiguous_endpoint_hold_dwell_ms=" << result.ambiguous_endpoint_hold_dwell_ms
            << " ambiguous_endpoint_hold_required_ms=" << result.ambiguous_endpoint_hold_required_ms
            << " ambiguous_endpoint_hold_reason=" << result.ambiguous_endpoint_hold_reason
            << " ambiguous_endpoint_hold_frame_id=" << result.ambiguous_endpoint_hold_frame_id
            << " ambiguous_endpoint_hold_route_index=" << result.ambiguous_endpoint_hold_route_index
            << " ambiguous_endpoint_hold_progress=" << result.ambiguous_endpoint_hold_progress
            << " ambiguous_endpoint_hold_tracked_progress=" << result.ambiguous_endpoint_hold_tracked_progress
            << " ambiguous_endpoint_hold_confidence=" << result.ambiguous_endpoint_hold_confidence
            << " endpoint_stop_frame_written=" << bool_word(result.endpoint_stop_frame_written)
            << " endpoint_stop_frame_path="
            << (result.endpoint_stop_frame_path.empty() ? "none" : result.endpoint_stop_frame_path)
            << " endpoint_stop_frame_id=" << result.endpoint_stop_frame_id
            << " endpoint_stop_frame_size=" << result.endpoint_stop_frame_width
            << "x" << result.endpoint_stop_frame_height
            << " endpoint_stop_route_index=" << result.endpoint_stop_route_index
            << " endpoint_stop_progress=" << result.endpoint_stop_progress
            << " endpoint_stop_tracked_progress=" << result.endpoint_stop_tracked_progress
            << " endpoint_stop_confidence=" << result.endpoint_stop_confidence
            << " stop_reason=" << result.stop_reason
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
            << " telemetry_altitude_messages=" << result.telemetry_altitude_messages
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
            << " external_nav_estimates=" << result.external_nav_estimates
            << " external_nav_valid_for_fc=" << result.external_nav_valid_for_fc
            << " external_nav_valid_fraction=" << result.external_nav_valid_fraction
            << " external_nav_max_invalid_streak=" << result.external_nav_max_invalid_streak
            << " external_nav_invalid_reasons=" << result.external_nav_invalid_reasons
            << " external_nav_altitude_valid_frames=" << result.external_nav_altitude_valid_frames
            << " external_nav_bench_altitude_frames=" << result.external_nav_bench_altitude_frames
            << " external_nav_altitude_invalid_frames=" << result.external_nav_altitude_invalid_frames
            << " external_nav_relative_altitude_seen_frames=" << result.external_nav_relative_altitude_seen_frames
            << " external_nav_relative_altitude_min_avg_max_m=" << result.external_nav_relative_altitude_min_m
            << "/" << result.external_nav_relative_altitude_avg_m
            << "/" << result.external_nav_relative_altitude_max_m
            << " external_nav_expected_relative_altitude_required="
            << bool_word(result.external_nav_expected_relative_altitude_required)
            << " external_nav_expected_relative_altitude_m="
            << config.external_nav_expected_relative_altitude_m
            << " external_nav_expected_relative_altitude_tolerance_m="
            << config.external_nav_expected_relative_altitude_tolerance_m
            << " external_nav_relative_altitude_window_passed="
            << bool_word(result.external_nav_relative_altitude_window_passed)
            << " external_nav_latest_telemetry_mode=" << result.external_nav_latest_telemetry_mode
            << " external_nav_latest_telemetry_armed=" << bool_word(result.external_nav_latest_telemetry_armed)
            << " external_nav_altitude_blocker=" << result.external_nav_altitude_blocker
            << " external_nav_session_ready=" << bool_word(result.external_nav_session_ready)
            << " external_nav_session_valid_for_fc=" << result.external_nav_session_valid_for_fc
            << "/" << result.external_nav_estimates
            << " external_nav_session_reason=" << result.external_nav_session_reason
            << " external_nav_strict_session_ready=" << bool_word(result.external_nav_strict_session_ready)
            << " external_nav_strict_session_reason=" << result.external_nav_strict_session_reason
            << " external_nav_quality_ready=" << bool_word(result.external_nav_quality_ready)
            << " external_nav_quality_reason=" << result.external_nav_quality_reason
            << " external_nav_operator_readiness=" << result.external_nav_operator_readiness
            << " external_nav_operator_reason=" << result.external_nav_operator_reason
            << " visual_scale_required=" << bool_word(result.visual_scale_required)
            << " visual_scale_valid=" << result.visual_scale_valid << "/" << result.external_nav_estimates
            << " visual_scale_valid_fraction=" << result.visual_scale_valid_fraction
            << " visual_scale_ratio_min_avg_max=" << result.visual_scale_ratio_min
            << "/" << result.visual_scale_ratio_avg
            << "/" << result.visual_scale_ratio_max
            << " visual_scale_confidence_min_avg=" << result.visual_scale_confidence_min
            << "/" << result.visual_scale_confidence_avg
            << " visual_scale_ratio_histogram=" << result.visual_scale_ratio_histogram
            << " tracked_visual_scale_ratio=" << result.first_tracked_visual_scale_ratio
            << ".." << result.last_tracked_visual_scale_ratio
            << " tracked_visual_scale_ratio_min_avg_max=" << result.tracked_visual_scale_ratio_min
            << "/" << result.tracked_visual_scale_ratio_avg
            << "/" << result.tracked_visual_scale_ratio_max
            << " live_output_gate_allowed_frames=" << result.live_output_gate_allowed_frames
            << " live_output_gate_blocked_frames=" << result.live_output_gate_blocked_frames
            << " live_output_gate_block_reasons=" << result.live_output_gate_block_reasons
            << " final_live_output_gate_allowed=" << (result.final_live_output_gate_allowed ? "true" : "false")
            << " final_live_output_gate_reason=" << result.final_live_output_gate_reason
            << " live_output_session_audit_started=" << (result.live_output_session_audit_started ? "true" : "false")
            << " live_output_session_audit_path=" << result.live_output_session_audit_path
            << " external_nav_output_allowed_frames=" << result.external_nav_output_allowed_frames
            << " external_nav_output_sent_frames=" << result.external_nav_output_sent_frames
            << " external_nav_output_blocked_frames=" << result.external_nav_output_blocked_frames
            << " external_nav_output_block_reasons=" << result.external_nav_output_block_reasons
            << " final_external_nav_output_reason=" << result.final_external_nav_output_reason
            << " external_nav_output_session_audit_started="
            << (result.external_nav_output_session_audit_started ? "true" : "false")
            << " external_nav_output_session_audit_path=" << result.external_nav_output_session_audit_path
            << " passed=" << (result.passed ? "true" : "false") << "\n";

    metrics << "live_route_match_compact"
            << " passed=" << bool_word(result.passed)
            << " frames=" << result.frames_captured << "/" << config.frames_to_capture
            << " valid_matches=" << result.valid_matches
            << " progress=" << result.first_progress << ".." << result.last_progress
            << " minmax_progress=" << result.min_progress_seen << ".." << result.max_progress_seen
            << " tracked_progress=" << result.first_tracked_progress << ".." << result.last_tracked_progress
            << " tracked_minmax_progress=" << result.min_tracked_progress_seen
            << ".." << result.max_tracked_progress_seen
            << " tracked_directional_progress=" << bool_word(result.tracked_directional_progress_passed)
            << " tracked_regressions=" << result.tracked_progress_regressions
            << "/" << result.tracked_reverse_progress_regressions
            << " tracked_rollback=" << result.tracked_progress_rollback
            << "/" << result.tracked_reverse_progress_rollback
            << " endpoint_passed=" << bool_word(result.endpoint_progress_passed)
            << " progress_gate_passed=" << bool_word(result.progress_gate_passed)
            << " endpoint_stop=" << bool_word(result.endpoint_stop_triggered)
            << " endpoint_dwell_ms=" << result.endpoint_dwell_ms
            << " endpoint_dwell_required_ms=" << result.endpoint_dwell_required_ms
            << " endpoint_dwell_passed=" << bool_word(result.endpoint_dwell_passed)
            << " endpoint_confirmation_required=" << bool_word(result.endpoint_confirmation_required)
            << " endpoint_confirmation_passed=" << bool_word(result.endpoint_confirmation_passed)
            << " endpoint_confirmation_reason=" << result.endpoint_confirmation_reason
            << " endpoint_top_match_gap=" << result.endpoint_top_match_gap
            << " endpoint_edge_top_match_gap=" << result.endpoint_edge_top_match_gap
            << " ambiguous_endpoint_hold=" << bool_word(result.ambiguous_endpoint_hold_triggered)
            << " ambiguous_endpoint_hold_dwell_ms=" << result.ambiguous_endpoint_hold_dwell_ms
            << " ambiguous_endpoint_hold_required_ms=" << result.ambiguous_endpoint_hold_required_ms
            << " ambiguous_endpoint_hold_reason=" << result.ambiguous_endpoint_hold_reason
            << " ambiguous_endpoint_hold_route_index=" << result.ambiguous_endpoint_hold_route_index
            << " ambiguous_endpoint_hold_progress=" << result.ambiguous_endpoint_hold_progress
            << " ambiguous_endpoint_hold_tracked_progress=" << result.ambiguous_endpoint_hold_tracked_progress
            << " ambiguous_endpoint_hold_confidence=" << result.ambiguous_endpoint_hold_confidence
            << " endpoint_stop_frame_written=" << bool_word(result.endpoint_stop_frame_written)
            << " endpoint_stop_frame_path="
            << (result.endpoint_stop_frame_path.empty() ? "none" : result.endpoint_stop_frame_path)
            << " endpoint_stop_frame_id=" << result.endpoint_stop_frame_id
            << " endpoint_stop_frame_size=" << result.endpoint_stop_frame_width
            << "x" << result.endpoint_stop_frame_height
            << " endpoint_stop_route_index=" << result.endpoint_stop_route_index
            << " endpoint_stop_progress=" << result.endpoint_stop_progress
            << " endpoint_stop_tracked_progress=" << result.endpoint_stop_tracked_progress
            << " endpoint_stop_confidence=" << result.endpoint_stop_confidence
            << " stop_reason=" << result.stop_reason
            << " confidence_min_avg=" << result.minimum_confidence_seen << "/" << result.average_confidence
            << " top_match_diagnostics=" << bool_word(config.top_match_diagnostics)
            << " top_match_frames=" << result.top_match_diagnostic_frames
            << " top_match_gap_min_avg=" << result.top_match_gap_min
            << "/" << result.top_match_gap_avg
            << " zone_probe_diagnostics=" << bool_word(config.zone_probe_diagnostics)
            << " zone_probe_frames=" << result.zone_probe_diagnostic_frames
            << " end_zone_gap_min_avg=" << result.end_zone_gap_min
            << "/" << result.end_zone_gap_avg
            << " edge_match_diagnostics=" << bool_word(config.edge_match_diagnostics)
            << " edge_match_frames=" << result.edge_match_diagnostic_frames
            << " edge_top_match_gap_min_avg=" << result.edge_top_match_gap_min
            << "/" << result.edge_top_match_gap_avg
            << " edge_end_zone_gap_min_avg=" << result.edge_end_zone_gap_min
            << "/" << result.edge_end_zone_gap_avg
            << " focus_roi_diagnostics=" << bool_word(config.focus_roi_diagnostics)
            << " focus_roi_valid=" << result.focus_roi_valid_matches
            << "/" << result.focus_roi_frames
            << " focus_roi_valid_fraction=" << result.focus_roi_valid_fraction
            << " focus_roi_confidence_min_avg=" << result.focus_roi_confidence_min
            << "/" << result.focus_roi_confidence_avg
            << " focus_roi_progress=" << result.focus_roi_first_progress
            << ".." << result.focus_roi_last_progress
            << " focus_roi_minmax_progress=" << result.focus_roi_min_progress_seen
            << ".." << result.focus_roi_max_progress_seen
            << " focus_roi_route_index_agreement=" << result.focus_roi_route_index_agreements
            << "/" << result.focus_roi_frames
            << " focus_roi_route_index_agreement_fraction="
            << result.focus_roi_route_index_agreement_fraction
            << " focus_roi_endpoint_agreement=" << result.focus_roi_endpoint_agreements
            << "/" << result.focus_roi_frames
            << " focus_roi_endpoint_agreement_fraction=" << result.focus_roi_endpoint_agreement_fraction
            << " focus_roi_top_match_gap_min_avg=" << result.focus_roi_top_match_gap_min
            << "/" << result.focus_roi_top_match_gap_avg
            << " telemetry_health=" << bool_word(result.live_telemetry_health_passed)
            << " telemetry_dropped=" << result.telemetry_bytes_dropped
            << " dry_run_quality=" << bool_word(result.dry_run_command_quality_passed)
            << " dry_run_valid=" << result.valid_dry_run_commands << "/" << result.dry_run_commands
            << " external_nav_valid=" << result.external_nav_valid_for_fc << "/" << result.external_nav_estimates
            << " external_nav_valid_fraction=" << result.external_nav_valid_fraction
            << " external_nav_max_invalid_streak=" << result.external_nav_max_invalid_streak
            << " external_nav_invalid_reasons=" << result.external_nav_invalid_reasons
            << " external_nav_altitude_valid=" << result.external_nav_altitude_valid_frames
            << "/" << result.external_nav_estimates
            << " external_nav_bench_altitude=" << result.external_nav_bench_altitude_frames
            << "/" << result.external_nav_estimates
            << " external_nav_altitude_invalid=" << result.external_nav_altitude_invalid_frames
            << "/" << result.external_nav_estimates
            << " external_nav_relative_altitude_seen=" << result.external_nav_relative_altitude_seen_frames
            << "/" << result.external_nav_estimates
            << " external_nav_relative_altitude_min_avg_max_m=" << result.external_nav_relative_altitude_min_m
            << "/" << result.external_nav_relative_altitude_avg_m
            << "/" << result.external_nav_relative_altitude_max_m
            << " external_nav_expected_relative_altitude_required="
            << bool_word(result.external_nav_expected_relative_altitude_required)
            << " external_nav_expected_relative_altitude_m="
            << config.external_nav_expected_relative_altitude_m
            << " external_nav_expected_relative_altitude_tolerance_m="
            << config.external_nav_expected_relative_altitude_tolerance_m
            << " external_nav_relative_altitude_window_passed="
            << bool_word(result.external_nav_relative_altitude_window_passed)
            << " external_nav_latest_telemetry_mode=" << result.external_nav_latest_telemetry_mode
            << " external_nav_latest_telemetry_armed=" << bool_word(result.external_nav_latest_telemetry_armed)
            << " external_nav_altitude_blocker=" << result.external_nav_altitude_blocker
            << " external_nav_session_ready=" << bool_word(result.external_nav_session_ready)
            << " external_nav_session_valid=" << result.external_nav_session_valid_for_fc
            << "/" << result.external_nav_estimates
            << " external_nav_session_reason=" << result.external_nav_session_reason
            << " external_nav_strict_session_ready=" << bool_word(result.external_nav_strict_session_ready)
            << " external_nav_strict_session_reason=" << result.external_nav_strict_session_reason
            << " external_nav_quality_ready=" << bool_word(result.external_nav_quality_ready)
            << " external_nav_quality_reason=" << result.external_nav_quality_reason
            << " external_nav_operator_readiness=" << result.external_nav_operator_readiness
            << " external_nav_operator_reason=" << result.external_nav_operator_reason
            << " visual_scale_required=" << bool_word(result.visual_scale_required)
            << " visual_scale_valid=" << result.visual_scale_valid << "/" << result.external_nav_estimates
            << " visual_scale_valid_fraction=" << result.visual_scale_valid_fraction
            << " visual_scale_ratio_min_avg_max=" << result.visual_scale_ratio_min
            << "/" << result.visual_scale_ratio_avg
            << "/" << result.visual_scale_ratio_max
            << " visual_scale_confidence_min_avg=" << result.visual_scale_confidence_min
            << "/" << result.visual_scale_confidence_avg
            << " visual_scale_ratio_histogram=" << result.visual_scale_ratio_histogram
            << " tracked_visual_scale_ratio=" << result.first_tracked_visual_scale_ratio
            << ".." << result.last_tracked_visual_scale_ratio
            << " tracked_visual_scale_ratio_min_avg_max=" << result.tracked_visual_scale_ratio_min
            << "/" << result.tracked_visual_scale_ratio_avg
            << "/" << result.tracked_visual_scale_ratio_max
            << " live_output_gate_allowed=" << result.live_output_gate_allowed_frames
            << " live_output_gate_blocked=" << result.live_output_gate_blocked_frames
            << " live_output_gate_block_reasons=" << result.live_output_gate_block_reasons
            << " final_live_output_gate_reason=" << result.final_live_output_gate_reason
            << " live_output_session_audit=" << bool_word(result.live_output_session_audit_started)
            << " external_nav_output_allowed=" << result.external_nav_output_allowed_frames
            << " external_nav_output_sent=" << result.external_nav_output_sent_frames
            << " external_nav_output_blocked=" << result.external_nav_output_blocked_frames
            << " external_nav_output_block_reasons=" << result.external_nav_output_block_reasons
            << " final_external_nav_output_reason=" << result.final_external_nav_output_reason
            << " external_nav_output_session_audit="
            << bool_word(result.external_nav_output_session_audit_started)
            << "\n";
    return result;
}

} // namespace vh
