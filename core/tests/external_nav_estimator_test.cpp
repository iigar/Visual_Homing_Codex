#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "visual_homing/external_nav_estimator.hpp"

namespace {

vh::Timestamp at_ms(int milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(std::chrono::milliseconds(milliseconds)));
}

vh::RouteMatch good_match() {
    vh::RouteMatch match;
    match.timestamp = at_ms(100);
    match.route_index = 50;
    match.progress = 0.5;
    match.direction_error_rad = 0.1;
    match.direction_observation_valid = true;
    match.confidence = 0.95;
    match.valid = true;
    return match;
}

vh::RouteSignatureSummary route_summary() {
    vh::RouteSignatureSummary summary;
    summary.entry_count = 101;
    return summary;
}

vh::MavlinkTelemetry fresh_telemetry() {
    vh::MavlinkTelemetry telemetry;
    telemetry.timestamp = at_ms(100);
    telemetry.heartbeat_seen = true;
    telemetry.yaw_rad = 0.25;
    telemetry.relative_altitude_seen = true;
    telemetry.relative_altitude_m = 12.0;
    return telemetry;
}

} // namespace

int main() {
    vh::ExternalNavEstimatorConfig config;
    config.nominal_route_length_m = 20.0;
    config.minimum_match_confidence = 0.9;
    config.maximum_altitude_age_ms = 250.0;
    config.route_frame_alignment_known = true;
    config.altitude_origin_aligned = true;

    const auto estimate = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        fresh_telemetry(),
        at_ms(150),
        config);
    assert(estimate.valid_for_fc);
    assert(estimate.reason == "valid");
    assert(estimate.route_match_valid);
    assert(estimate.telemetry_fresh);
    assert(estimate.altitude_valid);
    assert(!estimate.bench_diagnostic_altitude_used);
    assert(estimate.scale_known);
    assert(estimate.pose_frame == vh::LocalCoordinateFrame::local_ned);
    assert(estimate.frame_alignment_known);
    assert(estimate.altitude_origin_aligned);
    assert(estimate.x_m == 10.0);
    assert(estimate.y_m == 0.0);
    assert(estimate.z_m == -12.0);
    assert(std::abs(estimate.yaw_rad - 0.1) < 1e-12);
    assert(estimate.telemetry_yaw_rad == 0.25);
    assert(estimate.yaw_direction_error_rad == 0.1);
    assert(estimate.yaw_source_independent);
    assert(estimate.route_entries == 101);
    assert(estimate.route_index == 50);

    const auto log_line = vh::external_nav_estimate_log_line(estimate);
    assert(log_line.find("external_nav_estimate ") == 0);
    assert(log_line.find("valid_for_fc=true") != std::string::npos);
    assert(log_line.find("reason=valid") != std::string::npos);
    assert(log_line.find("telemetry_yaw_rad=0.25") != std::string::npos);
    assert(log_line.find("yaw_direction_error_rad=0.1") != std::string::npos);
    assert(log_line.find("yaw_source_independent=true") != std::string::npos);
    assert(log_line.find("relative_altitude_seen=true") != std::string::npos);
    assert(log_line.find("relative_altitude_m=12") != std::string::npos);
    assert(log_line.find("bench_diagnostic_altitude_used=false") != std::string::npos);
    assert(log_line.find("scale_known=true") != std::string::npos);
    assert(log_line.find("visual_scale_valid=false") != std::string::npos);
    assert(log_line.find("visual_scale_ratio=0") != std::string::npos);
    assert(log_line.find("pose_frame=local_ned") != std::string::npos);
    assert(log_line.find("frame_alignment_known=true") != std::string::npos);
    assert(log_line.find("route_origin_ned_m=0/0/0") != std::string::npos);
    assert(log_line.find("altitude_origin_aligned=true") != std::string::npos);

    auto no_alignment_config = config;
    no_alignment_config.route_frame_alignment_known = false;
    const auto no_alignment = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        fresh_telemetry(),
        at_ms(150),
        no_alignment_config);
    assert(!no_alignment.valid_for_fc);
    assert(no_alignment.reason == "frame_alignment_not_known");
    assert(no_alignment.pose_frame == vh::LocalCoordinateFrame::route_frd);
    assert(no_alignment.x_m == 10.0);
    assert(no_alignment.y_m == 0.0);
    assert(no_alignment.z_m == -12.0);

    auto no_altitude_origin_config = config;
    no_altitude_origin_config.altitude_origin_aligned = false;
    const auto no_altitude_origin = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        fresh_telemetry(),
        at_ms(150),
        no_altitude_origin_config);
    assert(!no_altitude_origin.valid_for_fc);
    assert(no_altitude_origin.reason == "altitude_origin_not_aligned");
    assert(no_altitude_origin.pose_frame == vh::LocalCoordinateFrame::local_ned);

    auto rotated_config = config;
    rotated_config.route_alignment.origin_ned_m = {100.0, 200.0, 5.0};
    rotated_config.route_alignment.heading_ned_rad = std::acos(-1.0) / 2.0;
    const auto rotated = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        fresh_telemetry(),
        at_ms(150),
        rotated_config);
    assert(rotated.valid_for_fc);
    assert(rotated.reason == "valid");
    assert(std::abs(rotated.x_m - 100.0) < 1e-12);
    assert(std::abs(rotated.y_m - 210.0) < 1e-12);
    assert(std::abs(rotated.z_m - -7.0) < 1e-12);
    assert(std::abs(rotated.yaw_rad - (std::acos(-1.0) / 2.0 + 0.1)) < 1e-12);

    auto saturated_direction_match = good_match();
    saturated_direction_match.direction_observation_valid = false;
    const auto saturated_direction = vh::make_route_progress_external_nav_estimate(
        saturated_direction_match,
        route_summary(),
        fresh_telemetry(),
        at_ms(150),
        config);
    assert(!saturated_direction.valid_for_fc);
    assert(saturated_direction.reason == "yaw_source_not_independent");
    assert(!saturated_direction.yaw_source_independent);
    assert(saturated_direction.yaw_rad == 0.0);
    assert(saturated_direction.telemetry_yaw_rad == 0.25);

    auto low_confidence_match = good_match();
    low_confidence_match.confidence = 0.5;
    const auto low_confidence = vh::make_route_progress_external_nav_estimate(
        low_confidence_match,
        route_summary(),
        fresh_telemetry(),
        at_ms(150),
        config);
    assert(!low_confidence.valid_for_fc);
    assert(low_confidence.reason == "route_match_not_valid");

    auto stale = fresh_telemetry();
    stale.timestamp = at_ms(0);
    const auto stale_estimate = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        stale,
        at_ms(500),
        config);
    assert(!stale_estimate.valid_for_fc);
    assert(stale_estimate.reason == "telemetry_not_fresh");

    auto no_altitude = fresh_telemetry();
    no_altitude.relative_altitude_m = 0.0;
    const auto no_altitude_estimate = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        no_altitude,
        at_ms(150),
        config);
    assert(!no_altitude_estimate.valid_for_fc);
    assert(no_altitude_estimate.reason == "altitude_not_valid");

    auto bench_altitude_config = config;
    bench_altitude_config.bench_diagnostic_altitude_m = 0.5;
    const auto bench_altitude = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        no_altitude,
        at_ms(150),
        bench_altitude_config);
    assert(!bench_altitude.valid_for_fc);
    assert(bench_altitude.reason == "bench_diagnostic_altitude_not_fc_ready");
    assert(!bench_altitude.altitude_valid);
    assert(bench_altitude.bench_diagnostic_altitude_used);
    assert(bench_altitude.scale_known);
    assert(bench_altitude.x_m == 10.0);
    assert(bench_altitude.z_m == -0.5);
    const auto bench_log_line = vh::external_nav_estimate_log_line(bench_altitude);
    assert(bench_log_line.find("bench_diagnostic_altitude_used=true") != std::string::npos);
    assert(bench_log_line.find("valid_for_fc=false") != std::string::npos);

    auto no_scale_config = config;
    no_scale_config.nominal_route_length_m = 0.0;
    const auto no_scale = vh::make_route_progress_external_nav_estimate(
        good_match(),
        route_summary(),
        fresh_telemetry(),
        at_ms(150),
        no_scale_config);
    assert(!no_scale.valid_for_fc);
    assert(no_scale.reason == "scale_not_known");
    assert(no_scale.altitude_valid);

    auto empty_route = route_summary();
    empty_route.entry_count = 0;
    const auto empty_route_estimate = vh::make_route_progress_external_nav_estimate(
        good_match(),
        empty_route,
        fresh_telemetry(),
        at_ms(150),
        config);
    assert(!empty_route_estimate.valid_for_fc);
    assert(empty_route_estimate.reason == "route_empty");

    bool rejected_bad_config = false;
    try {
        auto bad_config = config;
        bad_config.minimum_match_confidence = 2.0;
        (void)vh::make_route_progress_external_nav_estimate(
            good_match(),
            route_summary(),
            fresh_telemetry(),
            at_ms(150),
            bad_config);
    } catch (const std::invalid_argument&) {
        rejected_bad_config = true;
    }
    assert(rejected_bad_config);

    rejected_bad_config = false;
    try {
        auto bad_config = config;
        bad_config.bench_diagnostic_altitude_m = -1.0;
        (void)vh::make_route_progress_external_nav_estimate(
            good_match(),
            route_summary(),
            fresh_telemetry(),
            at_ms(150),
            bad_config);
    } catch (const std::invalid_argument&) {
        rejected_bad_config = true;
    }
    assert(rejected_bad_config);

    rejected_bad_config = false;
    try {
        auto bad_config = config;
        bad_config.route_alignment.heading_ned_rad = std::numeric_limits<double>::quiet_NaN();
        (void)vh::make_route_progress_external_nav_estimate(
            good_match(),
            route_summary(),
            fresh_telemetry(),
            at_ms(150),
            bad_config);
    } catch (const std::invalid_argument&) {
        rejected_bad_config = true;
    }
    assert(rejected_bad_config);

    return 0;
}
