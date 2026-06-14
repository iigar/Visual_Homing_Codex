#include <cassert>
#include <chrono>
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
    telemetry.relative_altitude_m = 12.0;
    return telemetry;
}

} // namespace

int main() {
    vh::ExternalNavEstimatorConfig config;
    config.nominal_route_length_m = 20.0;
    config.minimum_match_confidence = 0.9;
    config.maximum_altitude_age_ms = 250.0;

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
    assert(estimate.scale_known);
    assert(estimate.x_m == 10.0);
    assert(estimate.y_m == 0.0);
    assert(estimate.z_m == -12.0);
    assert(estimate.yaw_rad == 0.25);
    assert(estimate.route_entries == 101);
    assert(estimate.route_index == 50);

    const auto log_line = vh::external_nav_estimate_log_line(estimate);
    assert(log_line.find("external_nav_estimate ") == 0);
    assert(log_line.find("valid_for_fc=true") != std::string::npos);
    assert(log_line.find("reason=valid") != std::string::npos);
    assert(log_line.find("scale_known=true") != std::string::npos);

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

    return 0;
}
