#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/dry_run_command_sink.hpp"
#include "visual_homing/live_mavlink_bridge.hpp"
#include "visual_homing/live_mavlink_output_session.hpp"

namespace {

class FakeAuditSink final : public vh::LiveMavlinkOutputAuditSink {
public:
    bool start_result = true;
    bool ready_state = true;
    std::vector<std::string> records;

    bool start(const std::string& run_id) override {
        records.push_back("start:" + run_id);
        return start_result;
    }

    void stop(const std::string& reason) override {
        records.push_back("stop:" + reason);
        ready_state = false;
    }

    void record_command(
        const vh::NavigationCommand&,
        const vh::LiveMavlinkOutputSafetyResult& safety_result) override {
        records.push_back("command:" + safety_result.reason);
    }

    bool ready() const override {
        return ready_state;
    }
};

class FakeLiveWriter final : public vh::LiveMavlinkCommandWriter {
public:
    int starts = 0;
    int stops = 0;
    int sends = 0;
    bool running_state = false;

    bool start() override {
        ++starts;
        running_state = true;
        return true;
    }

    void stop() override {
        ++stops;
        running_state = false;
    }

    void send(const vh::NavigationCommand&) override {
        if (!running_state) {
            throw std::runtime_error("fake live writer send called while stopped");
        }
        ++sends;
    }

    bool running() const override {
        return running_state;
    }

    std::string unavailable_reason() const override {
        return {};
    }
};

vh::LiveMavlinkOutputSafetyConfig passing_config() {
    vh::LiveMavlinkOutputSafetyConfig config;
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
    {
        FakeAuditSink audit;
        vh::DryRunCommandSink bridge(nullptr);
        vh::LiveMavlinkOutputSession session({ "stopped", passing_config() }, audit, bridge);

        bool rejected = false;
        try {
            (void)session.process(passing_snapshot());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        assert(!session.running());
        assert(bridge.commands_sent() == 0);
    }

    {
        FakeAuditSink audit;
        audit.start_result = false;
        vh::DryRunCommandSink bridge(nullptr);
        vh::LiveMavlinkOutputSession session({ "no_audit", passing_config() }, audit, bridge);

        assert(!session.start());
        assert(!session.running());
        assert(!bridge.running());
        assert(audit.records.size() == 1);
        assert(audit.records[0] == "start:no_audit");
    }

    {
        FakeAuditSink audit;
        audit.ready_state = false;
        vh::DryRunCommandSink bridge(nullptr);
        vh::LiveMavlinkOutputSession session({ "audit_not_ready", passing_config() }, audit, bridge);

        assert(!session.start());
        assert(!session.running());
        assert(!bridge.running());
        assert(audit.records.size() == 1);
        assert(audit.records[0] == "start:audit_not_ready");
    }

    {
        FakeAuditSink audit;
        vh::DryRunCommandSink bridge(nullptr);
        vh::LiveMavlinkOutputSession session({ "blocked", passing_config() }, audit, bridge);
        assert(session.start());

        auto snapshot = passing_snapshot();
        snapshot.telemetry.armed = false;
        const auto result = session.process(snapshot);
        assert(!result.sent);
        assert(!result.safety.allowed);
        assert(result.safety.reason == "vehicle_not_armed");
        assert(bridge.commands_sent() == 0);
        assert(audit.records.size() == 2);
        assert(audit.records[1] == "command:vehicle_not_armed");

        session.stop("done");
        assert(!session.running());
        assert(!bridge.running());
    }

    {
        FakeAuditSink audit;
        vh::DryRunCommandSink bridge(nullptr);
        vh::LiveMavlinkOutputSession session({ "allowed", passing_config() }, audit, bridge);
        assert(session.start());

        const auto result = session.process(passing_snapshot());
        assert(result.sent);
        assert(result.safety.allowed);
        assert(result.safety.reason == "allowed");
        assert(bridge.commands_sent() == 1);
        assert(audit.records.size() == 2);
        assert(audit.records[1] == "command:allowed");

        session.stop("done");
    }

    {
        FakeAuditSink audit;
        vh::DryRunCommandSink bridge(nullptr);
        vh::LiveMavlinkOutputSession session({ "max_commands", passing_config(), 1, 0.0 }, audit, bridge);
        assert(session.start());

        const auto first = session.process(passing_snapshot());
        assert(first.sent);
        assert(bridge.commands_sent() == 1);

        const auto second = session.process(passing_snapshot());
        assert(!second.sent);
        assert(!second.safety.allowed);
        assert(second.safety.reason == "max_command_count_reached");
        assert(!session.running());
        assert(!bridge.running());
        assert(bridge.commands_sent() == 1);
        assert(audit.records.size() == 4);
        assert(audit.records[2] == "command:max_command_count_reached");
        assert(audit.records[3] == "stop:max_command_count_reached");
    }

    {
        FakeAuditSink audit;
        vh::DryRunCommandSink bridge(nullptr);
        vh::LiveMavlinkOutputSession session({ "max_duration", passing_config(), 0, 10.0 }, audit, bridge);
        assert(session.start());

        auto snapshot = passing_snapshot();
        snapshot.now = vh::now() + std::chrono::milliseconds(1000);
        const auto result = session.process(snapshot);
        assert(!result.sent);
        assert(!result.safety.allowed);
        assert(result.safety.reason == "max_duration_reached");
        assert(!session.running());
        assert(!bridge.running());
        assert(bridge.commands_sent() == 0);
        assert(audit.records.size() == 3);
        assert(audit.records[1] == "command:max_duration_reached");
        assert(audit.records[2] == "stop:max_duration_reached");
    }

    {
        bool rejected = false;
        try {
            FakeAuditSink audit;
            vh::DryRunCommandSink bridge(nullptr);
            vh::LiveMavlinkOutputSession session({ "bad_duration", passing_config(), 0, -1.0 }, audit, bridge);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    {
        FakeAuditSink audit;
        FakeLiveWriter writer;
        vh::LiveMavlinkBridge bridge(writer);
        vh::LiveMavlinkOutputSession session({ "blocked_live_writer", passing_config() }, audit, bridge);
        assert(session.start());

        auto snapshot = passing_snapshot();
        snapshot.match.confidence = 0.1;
        const auto result = session.process(snapshot);
        assert(!result.sent);
        assert(!result.safety.allowed);
        assert(result.safety.reason == "route_match_confidence_low");
        assert(writer.starts == 1);
        assert(writer.sends == 0);
        assert(audit.records.size() == 2);
        assert(audit.records[1] == "command:route_match_confidence_low");

        session.stop("done");
        assert(writer.stops == 1);
    }

    {
        FakeAuditSink audit;
        vh::LiveMavlinkBridge bridge;
        vh::LiveMavlinkOutputSession session({ "compiled_out_bridge", passing_config() }, audit, bridge);

        assert(!session.start());
        assert(!session.running());
        assert(!bridge.running());
        assert(audit.records.size() == 2);
        assert(audit.records[0] == "start:compiled_out_bridge");
        assert(audit.records[1] == "stop:bridge_start_failed");
    }

    return 0;
}
