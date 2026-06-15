#pragma once

#include <cstdint>
#include <string>

#include "visual_homing/interfaces.hpp"
#include "visual_homing/mavlink.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

struct ExternalNavEstimatorConfig {
    double nominal_route_length_m = 0.0;
    double minimum_match_confidence = 0.9;
    double maximum_altitude_age_ms = 500.0;
    double bench_diagnostic_altitude_m = 0.0;
    std::string source_tag = "visual_route_progress";
};

struct ExternalNavEstimate {
    Timestamp timestamp{};
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    double yaw_rad = 0.0;
    double confidence = 0.0;
    double route_progress = 0.0;
    std::uint64_t route_index = 0;
    std::uint64_t route_entries = 0;
    bool relative_altitude_seen = false;
    double relative_altitude_m = 0.0;
    bool route_match_valid = false;
    bool telemetry_fresh = false;
    bool altitude_valid = false;
    bool bench_diagnostic_altitude_used = false;
    bool scale_known = false;
    bool valid_for_fc = false;
    std::string source_tag;
    std::string reason;
};

ExternalNavEstimate make_route_progress_external_nav_estimate(
    const RouteMatch& match,
    const RouteSignatureSummary& route,
    const MavlinkTelemetry& telemetry,
    Timestamp now,
    const ExternalNavEstimatorConfig& config);

std::string external_nav_estimate_log_line(const ExternalNavEstimate& estimate);

} // namespace vh
