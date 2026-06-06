#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "visual_homing/live_mavlink_output_audit_log.hpp"

namespace {

std::string read_all(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

vh::LiveMavlinkOutputSafetySnapshot snapshot_for(const vh::NavigationCommand& command) {
    const auto now = vh::now();
    vh::LiveMavlinkOutputSafetySnapshot snapshot;
    snapshot.now = now;
    snapshot.telemetry.timestamp = now - std::chrono::milliseconds(40);
    snapshot.telemetry.heartbeat_seen = true;
    snapshot.telemetry.armed = false;
    snapshot.telemetry.mode = vh::FlightMode::AltHold;
    snapshot.match.timestamp = now - std::chrono::milliseconds(20);
    snapshot.match.valid = true;
    snapshot.match.progress = 0.42;
    snapshot.match.confidence = 0.88;
    snapshot.command = command;
    return snapshot;
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "visual_homing_live_output_audit_log_test.log";
    std::filesystem::remove(path);

    vh::LiveMavlinkOutputAuditLog audit({path, false});
    assert(!audit.ready());
    assert(audit.path() == path);

    vh::NavigationCommand command;
    command.valid = true;
    command.vx_mps = 0.0;
    command.vy_mps = 0.0;
    command.yaw_rate_radps = 0.12;
    command.confidence = 0.91;

    bool rejected = false;
    try {
        audit.record_command(snapshot_for(command), {false, "not_started"});
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    assert(audit.start("unit_test"));
    assert(audit.ready());
    audit.record_command(snapshot_for(command), {false, "vehicle_not_armed"});
    audit.stop("unit_test_complete");
    assert(!audit.ready());

    const auto contents = read_all(path);
    assert(contents.find("live_output_audit event=start run_id=unit_test") != std::string::npos);
    assert(contents.find("live_output_audit event=command") != std::string::npos);
    assert(contents.find("allowed=false") != std::string::npos);
    assert(contents.find("decision=blocked") != std::string::npos);
    assert(contents.find("reason=vehicle_not_armed") != std::string::npos);
    assert(contents.find("vx_mps=0") != std::string::npos);
    assert(contents.find("yaw_rate_radps=0.12") != std::string::npos);
    assert(contents.find("telemetry_heartbeat_seen=true") != std::string::npos);
    assert(contents.find("telemetry_armed=false") != std::string::npos);
    assert(contents.find("telemetry_mode=AltHold") != std::string::npos);
    assert(contents.find("route_match_valid=true") != std::string::npos);
    assert(contents.find("route_match_confidence=0.88") != std::string::npos);
    assert(contents.find("route_match_progress=0.42") != std::string::npos);
    assert(contents.find("live_output_audit event=stop reason=unit_test_complete") != std::string::npos);

    vh::LiveMavlinkOutputAuditLog missing_path({{}, false});
    assert(!missing_path.start("missing_path"));
    assert(!missing_path.ready());

    std::filesystem::remove(path);
    return 0;
}
