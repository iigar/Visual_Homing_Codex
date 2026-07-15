#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "visual_homing/live_external_nav_output_audit_log.hpp"

namespace {

std::string read_all(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

vh::ExternalNavEstimate ready_estimate() {
    vh::ExternalNavEstimate estimate;
    estimate.pose_frame = vh::LocalCoordinateFrame::local_ned;
    estimate.frame_alignment_known = true;
    estimate.altitude_origin_aligned = true;
    estimate.x_m = 0.306407;
    estimate.y_m = 0.0;
    estimate.z_m = -0.82;
    estimate.yaw_rad = -1.72;
    estimate.telemetry_yaw_rad = -1.70;
    estimate.yaw_direction_error_rad = -0.02;
    estimate.yaw_source_independent = true;
    estimate.confidence = 0.87;
    estimate.route_progress = 0.0306407;
    estimate.route_index = 11;
    estimate.route_entries = 360;
    estimate.relative_altitude_seen = true;
    estimate.relative_altitude_m = 0.82;
    estimate.route_match_valid = true;
    estimate.telemetry_fresh = true;
    estimate.altitude_valid = true;
    estimate.scale_known = true;
    estimate.visual_scale_valid = false;
    estimate.visual_scale_ratio = 0.0;
    estimate.valid_for_fc = true;
    estimate.source_tag = "visual_route_progress";
    estimate.reason = "valid";
    return estimate;
}

vh::LiveExternalNavOutputSnapshot snapshot_for(vh::ExternalNavEstimate estimate) {
    vh::LiveExternalNavOutputSnapshot snapshot;
    snapshot.now = vh::now();
    snapshot.estimate = std::move(estimate);
    snapshot.time_usec = 123456789ULL;
    return snapshot;
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "visual_homing_external_nav_output_audit_log_test.log";
    std::filesystem::remove(path);

    vh::LiveExternalNavOutputAuditLog audit({path, false});
    assert(!audit.ready());
    assert(audit.path() == path);

    bool rejected = false;
    try {
        audit.record_estimate(snapshot_for(ready_estimate()), {false, false, "not_started"});
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    assert(audit.start("external_nav_audit_unit"));
    assert(audit.ready());

    auto blocked_estimate = ready_estimate();
    blocked_estimate.valid_for_fc = false;
    blocked_estimate.reason = "scale_not_known";
    audit.record_estimate(snapshot_for(blocked_estimate), {false, false, "external_nav_estimate_not_ready"});
    audit.record_estimate(snapshot_for(ready_estimate()), {true, true, "allowed"});
    audit.stop("external_nav_audit_unit_complete");
    assert(!audit.ready());

    const auto contents = read_all(path);
    assert(contents.find("external_nav_output_audit event=start run_id=external_nav_audit_unit") != std::string::npos);
    assert(contents.find("external_nav_output_audit event=estimate allowed=false sent=false decision=blocked reason=external_nav_estimate_not_ready") != std::string::npos);
    assert(contents.find("external_nav_output_audit event=estimate allowed=true sent=true decision=allowed reason=allowed") != std::string::npos);
    assert(contents.find("time_usec=123456789") != std::string::npos);
    assert(contents.find("valid_for_fc=false") != std::string::npos);
    assert(contents.find("estimate_reason=scale_not_known") != std::string::npos);
    assert(contents.find("valid_for_fc=true") != std::string::npos);
    assert(contents.find("source=visual_route_progress") != std::string::npos);
    assert(contents.find("x_m=0.306407") != std::string::npos);
    assert(contents.find("z_m=-0.82") != std::string::npos);
    assert(contents.find("yaw_rad=-1.72") != std::string::npos);
    assert(contents.find("telemetry_yaw_rad=-1.7") != std::string::npos);
    assert(contents.find("yaw_direction_error_rad=-0.02") != std::string::npos);
    assert(contents.find("yaw_source_independent=true") != std::string::npos);
    assert(contents.find("pose_frame=local_ned") != std::string::npos);
    assert(contents.find("frame_alignment_known=true") != std::string::npos);
    assert(contents.find("route_origin_ned_m=0/0/0") != std::string::npos);
    assert(contents.find("route_heading_ned_rad=0") != std::string::npos);
    assert(contents.find("altitude_origin_aligned=true") != std::string::npos);
    assert(contents.find("progress=0.0306407") != std::string::npos);
    assert(contents.find("route_index=11") != std::string::npos);
    assert(contents.find("relative_altitude_seen=true") != std::string::npos);
    assert(contents.find("relative_altitude_m=0.82") != std::string::npos);
    assert(contents.find("route_match_valid=true") != std::string::npos);
    assert(contents.find("telemetry_fresh=true") != std::string::npos);
    assert(contents.find("altitude_valid=true") != std::string::npos);
    assert(contents.find("scale_known=true") != std::string::npos);
    assert(contents.find("external_nav_output_audit event=stop reason=external_nav_audit_unit_complete") != std::string::npos);

    vh::LiveExternalNavOutputAuditLog missing_path({{}, false});
    assert(!missing_path.start("missing_path"));
    assert(!missing_path.ready());

    std::filesystem::remove(path);
    return 0;
}
