#include <cassert>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/live_external_nav_output_session.hpp"

namespace {

class FakeAuditSink final : public vh::LiveExternalNavOutputAuditSink {
public:
    bool start_result = true;
    bool ready_state = true;
    bool throw_on_record = false;
    std::vector<std::string> records;

    bool start(const std::string& run_id) override {
        records.push_back("start:" + run_id);
        return start_result;
    }

    void stop(const std::string& reason) override {
        records.push_back("stop:" + reason);
        ready_state = false;
    }

    void record_estimate(
        const vh::LiveExternalNavOutputSnapshot&,
        const vh::LiveExternalNavOutputResult& result) override {
        if (throw_on_record) {
            throw std::runtime_error("fake external-nav audit failure");
        }
        records.push_back("estimate:" + result.reason);
    }

    bool ready() const override {
        return ready_state;
    }
};

class FakeExternalNavWriter final : public vh::ExternalNavWriter {
public:
    int starts = 0;
    int stops = 0;
    int sends = 0;
    bool running_state = false;
    bool start_result = true;
    bool throw_on_send = false;
    std::vector<std::uint64_t> time_usecs;

    bool start() override {
        ++starts;
        running_state = start_result;
        return start_result;
    }

    void stop() override {
        ++stops;
        running_state = false;
    }

    void send_vision_position_estimate(const vh::ExternalNavEstimate&, std::uint64_t time_usec) override {
        if (!running_state) {
            throw std::runtime_error("fake external-nav writer send while stopped");
        }
        if (throw_on_send) {
            throw std::runtime_error("fake external-nav writer send failure");
        }
        ++sends;
        time_usecs.push_back(time_usec);
    }

    bool running() const override {
        return running_state;
    }

    std::string unavailable_reason() const override {
        return {};
    }
};

vh::ExternalNavEstimate ready_estimate() {
    vh::ExternalNavEstimate estimate;
    estimate.pose_frame = vh::LocalCoordinateFrame::local_ned;
    estimate.frame_alignment_known = true;
    estimate.altitude_origin_aligned = true;
    estimate.timestamp = vh::now();
    estimate.x_m = 0.5;
    estimate.y_m = 0.0;
    estimate.z_m = -0.82;
    estimate.yaw_rad = -1.6;
    estimate.confidence = 0.88;
    estimate.route_progress = 0.05;
    estimate.route_index = 18;
    estimate.route_entries = 360;
    estimate.relative_altitude_seen = true;
    estimate.relative_altitude_m = 0.82;
    estimate.route_match_valid = true;
    estimate.telemetry_fresh = true;
    estimate.altitude_valid = true;
    estimate.scale_known = true;
    estimate.valid_for_fc = true;
    estimate.source_tag = "visual_route_progress";
    estimate.reason = "valid";
    return estimate;
}

vh::LiveExternalNavOutputSessionConfig passing_config() {
    vh::LiveExternalNavOutputSessionConfig config;
    config.run_id = "external_nav";
    config.output_available = true;
    config.runtime_enabled = true;
    config.operator_confirmed = true;
    config.audit_log_enabled = true;
    config.single_writer = true;
    return config;
}

vh::LiveExternalNavOutputSnapshot passing_snapshot() {
    vh::LiveExternalNavOutputSnapshot snapshot;
    snapshot.now = vh::now();
    snapshot.estimate = ready_estimate();
    snapshot.time_usec = 123456789ULL;
    return snapshot;
}

} // namespace

int main() {
    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);

        bool rejected = false;
        try {
            (void)session.process(passing_snapshot());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        assert(!session.running());
        assert(writer.starts == 0);
        assert(writer.sends == 0);
    }

    {
        FakeAuditSink audit;
        audit.start_result = false;
        FakeExternalNavWriter writer;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);

        assert(!session.start());
        assert(!session.running());
        assert(writer.starts == 0);
        assert(audit.records.size() == 1);
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        auto config = passing_config();
        config.output_available = false;
        vh::LiveExternalNavOutputSession session(config, audit, writer);
        assert(session.start());

        const auto result = session.process(passing_snapshot());
        assert(!result.allowed);
        assert(!result.sent);
        assert(result.reason == "external_nav_output_unavailable");
        assert(writer.starts == 0);
        assert(writer.sends == 0);
        assert(audit.records.size() == 2);
        assert(audit.records[1] == "estimate:external_nav_output_unavailable");
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        auto config = passing_config();
        config.operator_confirmed = false;
        vh::LiveExternalNavOutputSession session(config, audit, writer);
        assert(session.start());

        const auto result = session.process(passing_snapshot());
        assert(!result.allowed);
        assert(result.reason == "operator_not_confirmed");
        assert(writer.starts == 0);
        assert(audit.records[1] == "estimate:operator_not_confirmed");
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);
        assert(session.start());

        auto snapshot = passing_snapshot();
        snapshot.estimate.valid_for_fc = false;
        snapshot.estimate.reason = "scale_not_known";
        const auto result = session.process(snapshot);
        assert(!result.allowed);
        assert(result.reason == "external_nav_estimate_not_ready");
        assert(writer.starts == 0);
        assert(writer.sends == 0);
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);
        assert(session.start());

        auto snapshot = passing_snapshot();
        snapshot.estimate.frame_alignment_known = false;
        const auto result = session.process(snapshot);
        assert(!result.allowed);
        assert(result.reason == "external_nav_estimate_not_ready");
        assert(writer.starts == 0);
        assert(writer.sends == 0);
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);
        assert(session.start());

        auto snapshot = passing_snapshot();
        snapshot.estimate.x_m = std::numeric_limits<double>::quiet_NaN();
        const auto result = session.process(snapshot);
        assert(!result.allowed);
        assert(result.reason == "external_nav_estimate_not_ready");
        assert(writer.starts == 0);
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);
        assert(session.start());

        const auto result = session.process(passing_snapshot());
        assert(result.allowed);
        assert(result.sent);
        assert(result.reason == "allowed");
        assert(writer.starts == 1);
        assert(writer.sends == 1);
        assert(writer.time_usecs.at(0) == 123456789ULL);
        assert(audit.records.size() == 2);
        assert(audit.records[1] == "estimate:allowed");

        session.stop("done");
        assert(writer.stops == 1);
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        auto config = passing_config();
        config.max_messages = 1;
        vh::LiveExternalNavOutputSession session(config, audit, writer);
        assert(session.start());

        const auto first = session.process(passing_snapshot());
        assert(first.sent);
        const auto second = session.process(passing_snapshot());
        assert(!second.sent);
        assert(second.reason == "max_message_count_reached");
        assert(!session.running());
        assert(!writer.running());
        assert(writer.sends == 1);
        assert(audit.records[2] == "estimate:max_message_count_reached");
        assert(audit.records[3] == "stop:max_message_count_reached");

        const auto after_stop = session.process(passing_snapshot());
        assert(!after_stop.allowed);
        assert(!after_stop.sent);
        assert(after_stop.reason == "external_nav_output_session_stopped");
        assert(writer.sends == 1);
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        auto config = passing_config();
        config.max_duration_ms = 10.0;
        vh::LiveExternalNavOutputSession session(config, audit, writer);
        assert(session.start());

        auto snapshot = passing_snapshot();
        snapshot.now = vh::now() + std::chrono::milliseconds(1000);
        const auto result = session.process(snapshot);
        assert(!result.sent);
        assert(result.reason == "max_duration_reached");
        assert(!session.running());
        assert(writer.sends == 0);
    }

    {
        bool rejected = false;
        try {
            FakeAuditSink audit;
            FakeExternalNavWriter writer;
            auto config = passing_config();
            config.max_duration_ms = -1.0;
            vh::LiveExternalNavOutputSession session(config, audit, writer);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        writer.start_result = false;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);
        assert(session.start());

        const auto result = session.process(passing_snapshot());
        assert(!result.sent);
        assert(result.reason == "writer_start_failed");
        assert(!session.running());
        assert(writer.starts == 1);
        assert(writer.sends == 0);
        assert(audit.records[1] == "estimate:writer_start_failed");
        assert(audit.records[2] == "stop:writer_start_failed");
    }

    {
        FakeAuditSink audit;
        FakeExternalNavWriter writer;
        writer.throw_on_send = true;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);
        assert(session.start());

        const auto result = session.process(passing_snapshot());
        assert(!result.sent);
        assert(result.reason == "send_failed");
        assert(!session.running());
        assert(!writer.running());
        assert(writer.starts == 1);
        assert(writer.stops == 1);
        assert(audit.records[1] == "estimate:send_failed");
        assert(audit.records[2] == "stop:send_failed");
    }

    {
        FakeAuditSink audit;
        audit.throw_on_record = true;
        FakeExternalNavWriter writer;
        vh::LiveExternalNavOutputSession session(passing_config(), audit, writer);
        assert(session.start());

        bool threw = false;
        try {
            (void)session.process(passing_snapshot());
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
        assert(!session.running());
        assert(!writer.running());
        assert(writer.starts == 1);
        assert(writer.sends == 1);
        assert(writer.stops == 1);
        assert(audit.records[1] == "stop:audit_record_failed");
    }

    return 0;
}
