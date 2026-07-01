#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>

#include "visual_homing/mavlink.hpp"
#include "visual_homing/mavlink_telemetry_stream.hpp"
#include "visual_homing/pi_camera_source.hpp"
#include "visual_homing/bounded_navigator.hpp"
#include "visual_homing/external_nav_estimator.hpp"

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
    std::size_t warmup_frames = 0;
    double altitude_m = 0.0;
    double heading_hint_rad = 0.0;
    bool use_telemetry_snapshot = false;
    MavlinkTelemetry telemetry_snapshot{};
    bool use_live_telemetry_stream = false;
    MavlinkTelemetryStreamConfig telemetry_stream{};
    std::uint64_t telemetry_warmup_timeout_ms = 1500;
    bool operator_cue_enabled = false;
    std::size_t operator_cue_seconds = 0;
    bool operator_cue_bell = true;
};

struct LiveRouteRecordingResult {
    bool started = false;
    bool route_written = false;
    std::uint64_t warmup_frames_dropped = 0;
    std::uint64_t frames_captured = 0;
    std::uint64_t route_entries = 0;
    std::uint64_t empty_polls = 0;
    double last_frame_age_ms = 0.0;
    double last_processing_latency_ms = 0.0;
    double elapsed_ms = 0.0;
    double effective_fps = 0.0;
    bool used_telemetry_snapshot = false;
    bool used_live_telemetry_stream = false;
    bool telemetry_warmup_passed = false;
    double telemetry_warmup_elapsed_ms = 0.0;
    std::uint64_t telemetry_bytes_captured = 0;
    std::uint64_t telemetry_bytes_retained = 0;
    std::uint64_t telemetry_bytes_dropped = 0;
    std::uint64_t telemetry_frames_seen = 0;
    std::uint64_t telemetry_heartbeat_messages = 0;
    std::uint64_t telemetry_attitude_messages = 0;
    std::uint64_t telemetry_global_position_int_messages = 0;
    std::uint64_t telemetry_altitude_messages = 0;
};

struct LiveRouteMatchingConfig {
    PiCameraConfig camera;
    std::filesystem::path route_path;
    int target_width = 32;
    int target_height = 24;
    std::size_t frames_to_capture = 120;
    std::size_t warmup_frames = 0;
    std::size_t window_radius = 12;
    double minimum_confidence = 0.80;
    int max_direction_shift_px = 0;
    double radians_per_pixel = 0.0;
    std::string expected_progress = "any";
    std::uint64_t max_progress_regressions = 5;
    double max_progress_rollback = 0.25;
    bool require_endpoint_progress = false;
    double endpoint_start_progress = 0.15;
    double endpoint_end_progress = 0.85;
    bool stop_at_endpoint_progress = false;
    bool operator_cue_enabled = false;
    std::size_t operator_cue_seconds = 0;
    bool operator_cue_bell = true;
    bool emit_dry_run_commands = false;
    BoundedNavigatorConfig navigator{};
    bool use_live_telemetry_stream = false;
    MavlinkTelemetryStreamConfig telemetry_stream{};
    std::uint64_t telemetry_warmup_timeout_ms = 1500;
    double telemetry_max_age_ms = 500.0;
    bool require_live_telemetry_health = false;
    bool emit_external_nav_estimates = false;
    ExternalNavEstimatorConfig external_nav{};
    bool visual_scale_diagnostics = false;
    double visual_scale_reference_altitude_m = 0.0;
    bool scale_refinement_enabled = false;
    std::size_t scale_refinement_radius = 1;
    bool top_match_diagnostics = false;
    std::size_t top_match_count = 5;
    bool zone_probe_diagnostics = false;
    bool edge_match_diagnostics = false;
    std::size_t edge_match_top_count = 5;
    double external_nav_expected_relative_altitude_m = 0.0;
    double external_nav_expected_relative_altitude_tolerance_m = 0.0;
    bool require_dry_run_command_quality = false;
    double minimum_valid_dry_run_command_fraction = 0.95;
    std::uint64_t max_invalid_dry_run_command_streak = 3;
    double max_abs_dry_run_yaw_rate_radps = 0.35;
    std::uint64_t max_dry_run_yaw_rate_sign_flips = 20;
    double max_dry_run_yaw_rate_delta_radps = 0.15;
    bool emit_live_output_session_audit = false;
    std::filesystem::path live_output_session_audit_path;
    bool live_output_runtime_controls_provided = false;
    bool live_output_runtime_enabled = false;
    bool live_output_operator_confirmed = false;
    std::uint64_t live_output_max_commands = 0;
    double live_output_max_duration_ms = 0.0;
};

struct LiveRouteMatchingResult {
    bool started = false;
    std::uint64_t warmup_frames_dropped = 0;
    std::uint64_t frames_captured = 0;
    std::uint64_t valid_matches = 0;
    std::uint64_t progress_regressions = 0;
    std::uint64_t reverse_progress_regressions = 0;
    std::uint64_t empty_polls = 0;
    double progress_rollback = 0.0;
    double reverse_progress_rollback = 0.0;
    std::uint64_t tracked_progress_regressions = 0;
    std::uint64_t tracked_reverse_progress_regressions = 0;
    double tracked_progress_rollback = 0.0;
    double tracked_reverse_progress_rollback = 0.0;
    double minimum_confidence_seen = 0.0;
    double average_confidence = 0.0;
    std::uint64_t top_match_diagnostic_frames = 0;
    double top_match_gap_min = 0.0;
    double top_match_gap_avg = 0.0;
    std::uint64_t zone_probe_diagnostic_frames = 0;
    double end_zone_gap_min = 0.0;
    double end_zone_gap_avg = 0.0;
    std::uint64_t edge_match_diagnostic_frames = 0;
    double edge_top_match_gap_min = 0.0;
    double edge_top_match_gap_avg = 0.0;
    double edge_end_zone_gap_min = 0.0;
    double edge_end_zone_gap_avg = 0.0;
    double first_progress = 0.0;
    double last_progress = 0.0;
    double min_progress_seen = 0.0;
    double max_progress_seen = 0.0;
    double first_tracked_progress = 0.0;
    double last_tracked_progress = 0.0;
    double min_tracked_progress_seen = 0.0;
    double max_tracked_progress_seen = 0.0;
    bool progress_monotonic = true;
    bool reverse_progress_monotonic = true;
    bool tracked_progress_monotonic = true;
    bool tracked_reverse_progress_monotonic = true;
    bool directional_progress_passed = true;
    bool tracked_directional_progress_passed = true;
    bool endpoint_progress_passed = true;
    bool progress_gate_passed = true;
    bool endpoint_stop_triggered = false;
    std::string stop_reason = "not_started";
    bool used_live_telemetry_stream = false;
    bool telemetry_warmup_passed = false;
    double telemetry_warmup_elapsed_ms = 0.0;
    std::uint64_t telemetry_bytes_captured = 0;
    std::uint64_t telemetry_bytes_retained = 0;
    std::uint64_t telemetry_bytes_dropped = 0;
    std::uint64_t telemetry_frames_seen = 0;
    std::uint64_t telemetry_heartbeat_messages = 0;
    std::uint64_t telemetry_attitude_messages = 0;
    std::uint64_t telemetry_global_position_int_messages = 0;
    std::uint64_t telemetry_altitude_messages = 0;
    std::uint64_t telemetry_health_ready_frames = 0;
    std::uint64_t telemetry_health_degraded_frames = 0;
    bool live_telemetry_health_passed = true;
    double last_frame_age_ms = 0.0;
    double last_processing_latency_ms = 0.0;
    double elapsed_ms = 0.0;
    double effective_fps = 0.0;
    std::uint64_t dry_run_commands = 0;
    std::uint64_t valid_dry_run_commands = 0;
    std::uint64_t max_invalid_dry_run_command_streak = 0;
    double valid_dry_run_command_fraction = 0.0;
    double max_abs_dry_run_yaw_rate_radps = 0.0;
    std::uint64_t dry_run_yaw_rate_sign_flips = 0;
    double max_dry_run_yaw_rate_delta_radps = 0.0;
    bool dry_run_command_quality_passed = true;
    std::uint64_t external_nav_estimates = 0;
    std::uint64_t external_nav_valid_for_fc = 0;
    double external_nav_valid_fraction = 0.0;
    std::uint64_t external_nav_max_invalid_streak = 0;
    std::string external_nav_invalid_reasons;
    std::uint64_t external_nav_altitude_valid_frames = 0;
    std::uint64_t external_nav_bench_altitude_frames = 0;
    std::uint64_t external_nav_altitude_invalid_frames = 0;
    std::uint64_t external_nav_relative_altitude_seen_frames = 0;
    double external_nav_relative_altitude_min_m = 0.0;
    double external_nav_relative_altitude_avg_m = 0.0;
    double external_nav_relative_altitude_max_m = 0.0;
    bool external_nav_expected_relative_altitude_required = false;
    bool external_nav_relative_altitude_window_passed = true;
    bool external_nav_latest_telemetry_armed = false;
    std::string external_nav_latest_telemetry_mode = "Unknown";
    std::string external_nav_altitude_blocker = "not_requested";
    bool external_nav_session_ready = false;
    std::uint64_t external_nav_session_valid_for_fc = 0;
    std::string external_nav_session_reason = "not_requested";
    bool external_nav_strict_session_ready = false;
    std::string external_nav_strict_session_reason = "not_requested";
    bool external_nav_quality_ready = false;
    std::string external_nav_quality_reason = "not_requested";
    std::string external_nav_operator_readiness = "not_requested";
    std::string external_nav_operator_reason = "not_requested";
    bool visual_scale_required = false;
    std::uint64_t visual_scale_valid = 0;
    double visual_scale_valid_fraction = 0.0;
    double visual_scale_ratio_min = 0.0;
    double visual_scale_ratio_avg = 0.0;
    double visual_scale_ratio_max = 0.0;
    double visual_scale_confidence_min = 0.0;
    double visual_scale_confidence_avg = 0.0;
    std::string visual_scale_ratio_histogram;
    double first_tracked_visual_scale_ratio = 0.0;
    double last_tracked_visual_scale_ratio = 0.0;
    double tracked_visual_scale_ratio_min = 0.0;
    double tracked_visual_scale_ratio_avg = 0.0;
    double tracked_visual_scale_ratio_max = 0.0;
    std::uint64_t live_output_gate_allowed_frames = 0;
    std::uint64_t live_output_gate_blocked_frames = 0;
    bool final_live_output_gate_allowed = false;
    std::string final_live_output_gate_reason;
    std::string live_output_gate_block_reasons;
    bool live_output_session_audit_started = false;
    std::string live_output_session_audit_path;
    bool passed = false;
};

CameraSmokeResult run_pi_camera_smoke(const CameraSmokeConfig& config, std::ostream& metrics);
LiveRouteRecordingResult record_live_camera_route(const LiveRouteRecordingConfig& config, std::ostream& metrics);
bool live_route_match_endpoint_reached(const LiveRouteMatchingConfig& config, double progress);
bool live_route_match_has_required_frame_count(const LiveRouteMatchingConfig& config,
                                               const LiveRouteMatchingResult& result);
double live_route_match_next_tracked_progress(const std::string& expected_progress,
                                              double previous_tracked_progress,
                                              double raw_progress);
double live_route_match_next_tracked_visual_scale_ratio(double previous_tracked_scale_ratio,
                                                        double raw_scale_ratio);
LiveRouteMatchingResult match_live_camera_route(const LiveRouteMatchingConfig& config, std::ostream& metrics);

} // namespace vh
