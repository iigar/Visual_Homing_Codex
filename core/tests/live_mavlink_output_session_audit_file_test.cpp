#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "visual_homing/dry_run_command_sink.hpp"
#include "visual_homing/live_mavlink_output_audit_log.hpp"
#include "visual_homing/live_mavlink_output_session.hpp"

namespace {

std::string read_all(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

vh::LiveMavlinkOutputSafetyConfig passing_config() {
    vh::LiveMavlinkOutputSafetyConfig config;
    config.live_output_available = true;
    config.runtime_enabled = true;
    config.operator_confirmed = true;
    config.dry_run_quality_passed = true;
    config.audit_log_enabled = true;
    config.audit_log_ready = false;
    config.single_writer = true;
    config.max_telemetry_age_ms = 500.0;
    config.min_match_confidence = 0.75;
    config.max_match_age_ms = 250.0;
    config.max_abs_yaw_rate_radps = 0.35;
    config.max_abs_forward_speed_mps = 0.5;
    config.require_zero_forward_speed = true;
    return config;
}

vh::LiveMavlinkOutputSafetySnapshot passing_snapshot() {
    const auto now = vh::now();
    vh::LiveMavlinkOutputSafetySnapshot snapshot;
    snapshot.now = now;
    snapshot.telemetry.timestamp = now - std::chrono::milliseconds(50);
    snapshot.telemetry.heartbeat_seen = true;
    snapshot.telemetry.armed = true;
    snapshot.telemetry.mode = vh::FlightMode::Guided;
    snapshot.match.timestamp = now - std::chrono::milliseconds(20);
    snapshot.match.valid = true;
    snapshot.match.progress = 0.5;
    snapshot.match.confidence = 0.9;
    snapshot.command.timestamp = now;
    snapshot.command.valid = true;
    snapshot.command.vx_mps = 0.0;
    snapshot.command.vy_mps = 0.0;
    snapshot.command.yaw_rate_radps = 0.1;
    snapshot.command.confidence = 0.9;
    return snapshot;
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "visual_homing_live_output_session_audit_file_test.log";
    std::filesystem::remove(path);

    vh::LiveMavlinkOutputAuditLog audit({path, false});
    vh::DryRunCommandSink bridge(nullptr);
    vh::LiveMavlinkOutputSession session({ "audit_file_smoke", passing_config() }, audit, bridge);

    assert(session.start());
    assert(session.running());
    assert(audit.ready());
    assert(bridge.running());

    auto blocked_snapshot = passing_snapshot();
    blocked_snapshot.telemetry.armed = false;
    const auto blocked = session.process(blocked_snapshot);
    assert(!blocked.sent);
    assert(!blocked.safety.allowed);
    assert(blocked.safety.reason == "vehicle_not_armed");
    assert(bridge.commands_sent() == 0);

    const auto allowed = session.process(passing_snapshot());
    assert(allowed.sent);
    assert(allowed.safety.allowed);
    assert(allowed.safety.reason == "allowed");
    assert(bridge.commands_sent() == 1);
    assert(bridge.commands().size() == 1);
    assert(bridge.commands()[0].yaw_rate_radps == 0.1);

    session.stop("audit_file_smoke_complete");
    assert(!session.running());
    assert(!audit.ready());
    assert(!bridge.running());

    const auto contents = read_all(path);
    assert(contents.find("live_output_audit event=start run_id=audit_file_smoke") != std::string::npos);
    assert(contents.find("live_output_audit event=command allowed=false") != std::string::npos);
    assert(contents.find("live_output_audit event=command allowed=true") != std::string::npos);
    assert(contents.find("reason=vehicle_not_armed") != std::string::npos);
    assert(contents.find("reason=allowed") != std::string::npos);
    assert(contents.find("decision=blocked") != std::string::npos);
    assert(contents.find("decision=allowed") != std::string::npos);
    assert(contents.find("telemetry_heartbeat_seen=true") != std::string::npos);
    assert(contents.find("telemetry_mode=Guided") != std::string::npos);
    assert(contents.find("route_match_valid=true") != std::string::npos);
    assert(contents.find("route_match_confidence=0.9") != std::string::npos);
    assert(contents.find("route_match_progress=0.5") != std::string::npos);
    assert(contents.find("vx_mps=0") != std::string::npos);
    assert(contents.find("yaw_rate_radps=0.1") != std::string::npos);
    assert(contents.find("live_output_audit event=stop reason=audit_file_smoke_complete") != std::string::npos);

    std::filesystem::remove(path);
    return 0;
}
