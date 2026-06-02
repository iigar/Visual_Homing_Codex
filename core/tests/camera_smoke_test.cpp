#include <cassert>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/route_signature.hpp"

int main() {
    std::ostringstream metrics;
    vh::CameraSmokeConfig config;
    config.camera.width = 160;
    config.camera.height = 120;
    config.camera.frame_rate_hz = 10;
    config.target_width = 16;
    config.target_height = 12;
    config.frames_to_capture = 3;

    const auto result = vh::run_pi_camera_smoke(config, metrics);
    assert(!result.started);
    assert(result.frames_captured == 0);
    assert(result.empty_polls == 0);

    const auto output = metrics.str();
    assert(output.find("camera_smoke_start width=160 height=120 fps=10 target=16x12 requested_frames=3") != std::string::npos);
    assert(output.find("camera_smoke_unavailable error=") != std::string::npos);
    assert(output.find("camera_smoke_done started=false frames_captured=0 empty_polls=0") != std::string::npos);

    bool rejected = false;
    try {
        vh::CameraSmokeConfig invalid;
        invalid.frames_to_capture = 0;
        std::ostringstream ignored;
        (void)vh::run_pi_camera_smoke(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    bool rejected_target = false;
    try {
        vh::CameraSmokeConfig invalid;
        invalid.target_width = 0;
        std::ostringstream ignored;
        (void)vh::run_pi_camera_smoke(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_target = true;
    }
    assert(rejected_target);

    const auto live_route_path = std::filesystem::temp_directory_path() / "visual_homing_live_route_recording_unavailable_test.vhrs";
    std::filesystem::remove(live_route_path);

    std::ostringstream route_metrics;
    vh::LiveRouteRecordingConfig route_config;
    route_config.camera.width = 160;
    route_config.camera.height = 120;
    route_config.camera.frame_rate_hz = 10;
    route_config.frames_to_capture = 3;
    route_config.warmup_frames = 3;
    route_config.route_output_path = live_route_path;
    route_config.target_width = 16;
    route_config.target_height = 12;
    route_config.altitude_m = 42.0;
    route_config.heading_hint_rad = 0.1;

    const auto route_result = vh::record_live_camera_route(route_config, route_metrics);
    assert(!route_result.started);
    assert(!route_result.route_written);
    assert(route_result.warmup_frames_dropped == 0);
    assert(route_result.frames_captured == 0);
    assert(route_result.route_entries == 0);
    assert(!std::filesystem::exists(live_route_path));

    const auto route_output = route_metrics.str();
    assert(route_output.find("live_route_record_start width=160 height=120 fps=10 target=16x12 requested_frames=3 warmup_frames=3") != std::string::npos);
    assert(route_output.find("telemetry_warmup_timeout_ms=1500") != std::string::npos);
    assert(route_output.find("live_route_unavailable error=") != std::string::npos);
    assert(route_output.find("live_route_record_done started=false warmup_frames_dropped=0 frames_captured=0 entries=0 empty_polls=0 route_written=false") != std::string::npos);

    bool rejected_route_frames = false;
    try {
        vh::LiveRouteRecordingConfig invalid;
        invalid.frames_to_capture = 0;
        invalid.route_output_path = live_route_path;
        std::ostringstream ignored;
        (void)vh::record_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_route_frames = true;
    }
    assert(rejected_route_frames);

    bool rejected_route_output = false;
    try {
        vh::LiveRouteRecordingConfig invalid;
        std::ostringstream ignored;
        (void)vh::record_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_route_output = true;
    }
    assert(rejected_route_output);

    const auto match_route_path = std::filesystem::temp_directory_path() / "visual_homing_live_route_match_unavailable_test.vhrs";
    std::filesystem::remove(match_route_path);
    vh::RouteSignatureFile match_route;
    vh::RouteSignatureEntry match_entry;
    match_entry.frame_id = 1;
    match_entry.timestamp_ns = 100;
    match_entry.width = 16;
    match_entry.height = 12;
    match_entry.format = vh::PixelFormat::Gray8;
    match_entry.payload.assign(16 * 12, 42);
    match_route.entries.push_back(match_entry);
    vh::write_route_signature_file(match_route_path, match_route);

    std::ostringstream match_metrics;
    vh::LiveRouteMatchingConfig match_config;
    match_config.camera.width = 160;
    match_config.camera.height = 120;
    match_config.camera.frame_rate_hz = 10;
    match_config.frames_to_capture = 3;
    match_config.warmup_frames = 3;
    match_config.route_path = match_route_path;
    match_config.target_width = 16;
    match_config.target_height = 12;
    match_config.window_radius = 4;
    match_config.minimum_confidence = 0.75;
    match_config.max_direction_shift_px = 2;
    match_config.radians_per_pixel = 0.01;
    match_config.expected_progress = "reverse";
    match_config.max_progress_regressions = 7;
    match_config.max_progress_rollback = 0.125;

    const auto match_result = vh::match_live_camera_route(match_config, match_metrics);
    assert(!match_result.started);
    assert(!match_result.passed);
    assert(match_result.frames_captured == 0);
    assert(match_result.valid_matches == 0);

    const auto match_output = match_metrics.str();
    assert(match_output.find("live_route_match_start width=160 height=120 fps=10 target=16x12 requested_frames=3 warmup_frames=3") != std::string::npos);
    assert(match_output.find("route_entries=1 window_radius=4 minimum_confidence=0.75 max_direction_shift_px=2 radians_per_pixel=0.01 expected_progress=reverse max_progress_regressions=7 max_progress_rollback=0.125 require_endpoint_progress=false endpoint_start_progress=0.15 endpoint_end_progress=0.85") != std::string::npos);
    assert(match_output.find("live_route_match_unavailable error=") != std::string::npos);
    assert(match_output.find("live_route_match_done started=false warmup_frames_dropped=0 frames_captured=0 valid_matches=0 progress_regressions=0 empty_polls=0 passed=false") != std::string::npos);

    bool rejected_match_frames = false;
    try {
        vh::LiveRouteMatchingConfig invalid;
        invalid.frames_to_capture = 0;
        invalid.route_path = match_route_path;
        std::ostringstream ignored;
        (void)vh::match_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_match_frames = true;
    }
    assert(rejected_match_frames);

    bool rejected_match_route = false;
    try {
        vh::LiveRouteMatchingConfig invalid;
        std::ostringstream ignored;
        (void)vh::match_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_match_route = true;
    }
    assert(rejected_match_route);

    bool rejected_match_progress = false;
    try {
        vh::LiveRouteMatchingConfig invalid;
        invalid.route_path = match_route_path;
        invalid.expected_progress = "sideways";
        std::ostringstream ignored;
        (void)vh::match_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_match_progress = true;
    }
    assert(rejected_match_progress);

    bool rejected_match_rollback = false;
    try {
        vh::LiveRouteMatchingConfig invalid;
        invalid.route_path = match_route_path;
        invalid.max_progress_rollback = -0.1;
        std::ostringstream ignored;
        (void)vh::match_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_match_rollback = true;
    }
    assert(rejected_match_rollback);

    bool rejected_match_endpoint_start = false;
    try {
        vh::LiveRouteMatchingConfig invalid;
        invalid.route_path = match_route_path;
        invalid.endpoint_start_progress = 1.2;
        std::ostringstream ignored;
        (void)vh::match_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_match_endpoint_start = true;
    }
    assert(rejected_match_endpoint_start);

    bool rejected_match_endpoint_order = false;
    try {
        vh::LiveRouteMatchingConfig invalid;
        invalid.route_path = match_route_path;
        invalid.endpoint_start_progress = 0.9;
        invalid.endpoint_end_progress = 0.8;
        std::ostringstream ignored;
        (void)vh::match_live_camera_route(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_match_endpoint_order = true;
    }
    assert(rejected_match_endpoint_order);

    return 0;
}
