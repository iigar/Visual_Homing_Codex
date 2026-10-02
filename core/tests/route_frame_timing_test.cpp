#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <numeric>
#include <sstream>

#include "visual_homing/bounded_navigator.hpp"
#include "visual_homing/health_monitor.hpp"
#include "visual_homing/live_route_verification.hpp"
#include "visual_homing/mavlink_telemetry_adapter.hpp"
#include "visual_homing/route_frame_timing.hpp"

namespace {
using namespace std::chrono_literals;
vh::Timestamp at(int ms) { return vh::Timestamp{} + std::chrono::milliseconds(ms); }

void complete(vh::RouteFrameTiming& trace, const std::array<int, 8>& boundaries) {
    for (std::size_t i = 0; i < boundaries.size(); ++i) trace.complete(static_cast<vh::RouteFrameStage>(i), at(boundaries[i]));
}

void check_intervals() {
    vh::RouteFrameTiming trace(at(1000), at(1010));
    assert(!trace.summary().valid);
    complete(trace, {1012, 1015, 1020, 1027, 1038, 1051, 1068, 1087});
    const auto summary = trace.summary();
    const std::array<double, 8> expected{2, 3, 5, 7, 11, 13, 17, 19};
    assert(summary.valid && summary.source_timestamp_present && summary.stage_ms == expected);
    assert(summary.frame_work_ms == 77 && summary.source_age_at_start_ms == 10 && summary.source_age_at_finish_ms == 87);
    assert(std::accumulate(summary.stage_ms.begin(), summary.stage_ms.end(), 0.0) == summary.frame_work_ms);
    std::ostringstream first, second;
    vh::log_route_frame_timing(first, "test", 42, summary);
    vh::log_route_frame_timing(second, "test", 42, trace.summary());
    assert(first.str() == second.str());
    assert(first.str().find("scale_ms=11 external_nav_ms=13 verification_ms=17 reporting_endpoint_ms=19") != std::string::npos);

    vh::RouteFrameTiming skipped(at(1100), at(1000));
    complete(skipped, {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000});
    assert(skipped.summary().valid && skipped.summary().frame_work_ms == 0);
    assert(skipped.summary().source_age_at_finish_ms == -100); // Do not hide a future frame.
    vh::RouteFrameTiming absent({}, at(1000));
    complete(absent, {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000});
    assert(!absent.summary().source_timestamp_present);
    for (std::size_t stage = 0; stage < 8; ++stage) {
        vh::RouteFrameTiming backwards(at(1000), at(1010));
        std::array<int, 8> boundaries{1010, 1010, 1010, 1010, 1010, 1010, 1010, 1010};
        boundaries[stage] = 1009;
        complete(backwards, boundaries);
        assert(!backwards.summary().valid && backwards.summary().stage_ms[stage] == -1);
    }
    trace.complete(vh::RouteFrameStage::Reporting, at(1088)); // Duplicate/end overflow.
    assert(!trace.summary().valid);
    vh::RouteFrameTiming out_of_order(at(1000), at(1010));
    out_of_order.complete(vh::RouteFrameStage::Count, at(1010));
    complete(out_of_order, {1010, 1010, 1010, 1010, 1010, 1010, 1010, 1010});
    assert(!out_of_order.summary().valid);
}

class Processor : public vh::VerificationPublicationProcessor {
public:
    explicit Processor(std::atomic<unsigned>& count) : count_(count) {}
    vh::LiveVerificationFrameResult process(const vh::Frame&, const vh::LiveVerificationFrameContext& context) override {
        assert(context.health_ready && !context.has_local_pose);
        ++count_;
        return {};
    }
private:
    std::atomic<unsigned>& count_;
};

void check_late_consumers() {
    std::atomic<unsigned> processed{0};
    vh::BoundedVerificationPublisher publisher({}, std::make_unique<Processor>(processed));
    vh::LiveRouteVerificationProducerConfig producer_config;
    producer_config.maximum_frame_context_age_ms = 200;
    producer_config.maximum_scalar_age_ms = 200;
    producer_config.require_mavlink_health = true;
    vh::LiveRouteVerificationProducer producer(producer_config, publisher);
    assert(publisher.start());
    vh::Frame frame{.id = 1, .timestamp = at(1000), .width = 2, .height = 2, .data = {1, 2, 3, 4}};
    vh::RouteMatch match;
    match.timestamp = frame.timestamp;
    match.valid = true;
    match.confidence = 1;
    match.direction_observation_valid = true;
    vh::HealthMonitor monitor(at(1));
    monitor.observe_processed_frame(frame, at(1050), at(1100));
    monitor.set_links(true, true, true);
    monitor.set_route_match_confidence(1);
    const auto early = monitor.snapshot(at(1100));
    vh::MavlinkTelemetryAdapter telemetry({.max_telemetry_age_ms = 200});
    telemetry.observe({.timestamp = at(1000), .heartbeat_seen = true, .armed = true,
        .mode = vh::FlightMode::Guided}, at(1000));
    vh::BoundedNavigator navigator({.max_match_age_ms = 200});
    const auto check = [&](vh::Timestamp late_time, vh::Timestamp altitude_time,
                           const vh::MavlinkTelemetryAdapter* adapter, const char* reason) {
        const auto late = vh::refresh_route_frame_health(early, adapter, late_time);
        assert(late.frames_seen == 1 && late.frames_dropped == 0);
        assert(late.frame_age_ms == 50 && late.processing_latency_ms == 50);
        const auto observation = vh::make_progress_only_live_route_verification_observation(
            match, 0.5, late, {true, altitude_time, 1.0}, {true, frame.timestamp, 1.0});
        const auto before = publisher.metrics().submissions;
        const auto submitted = producer.submit(frame, observation);
        if (reason) {
            assert(submitted.status == vh::LiveRouteVerificationStatus::Rejected && submitted.reason == reason);
            assert(publisher.metrics().submissions == before);
        } else {
            assert(submitted.status == vh::LiveRouteVerificationStatus::Accepted);
            assert(publisher.wait_until_idle(1000ms));
            assert(publisher.metrics().submissions == before + 1);
        }
        assert(observation.match.timestamp == at(1000) && observation.scale_ratio.timestamp == at(1000));
        return late;
    };
    assert(navigator.update(match, check(at(1200), at(1000), &telemetry, nullptr)).valid);
    const auto stale_frame = check(at(1200) + 1ns, at(1000), &telemetry, "health_frame_context_stale");
    assert(!navigator.update(match, stale_frame).valid);
    // Fresh frame, but altitude expires during the late work.
    check(at(1100), at(900), &telemetry, nullptr);
    check(at(1100) + 1ns, at(900), &telemetry, "altitude_observation_invalid");
    // Telemetry can expire even while the frame/scalars remain fresh.
    vh::MavlinkTelemetryAdapter short_telemetry({.max_telemetry_age_ms = 100});
    short_telemetry.observe({.heartbeat_seen = true}, at(1000));
    check(at(1100), at(1000), &short_telemetry, nullptr);
    const auto lost = check(at(1100) + 1ns, at(1000), &short_telemetry, "health_not_ready");
    assert(!lost.mavlink_ok && lost.state == vh::HealthState::Degraded);
    short_telemetry.observe({.heartbeat_seen = true}, at(1101));
    const auto retained_failure = vh::refresh_route_frame_health(lost, &short_telemetry, at(1101));
    assert(!retained_failure.mavlink_ok); // A new frame/observation must establish recovery.
    check(at(1101), at(1000), &short_telemetry, nullptr);
    check(at(1100), at(1000), nullptr, nullptr); // Caller without telemetry contract.
    for (auto when : {vh::Timestamp{}, at(1099)}) {
        const auto invalid = vh::refresh_route_frame_health(early, &telemetry, when);
        assert(!invalid.camera_ok && !invalid.mavlink_ok && !invalid.navigation_ok);
    }
    auto failed = early;
    failed.camera_ok = failed.navigation_ok = false;
    failed.state = vh::HealthState::Failsafe;
    const auto refreshed_failure = vh::refresh_route_frame_health(failed, nullptr, at(1150));
    assert(!refreshed_failure.camera_ok && !refreshed_failure.navigation_ok && refreshed_failure.state == vh::HealthState::Failsafe);
    assert(monitor.snapshot(at(1200)).frames_seen == 1); // Refresh never double-counts.
    publisher.stop(true);
    assert(processed == 5);
}
} // namespace

int main() {
    check_intervals();
    check_late_consumers();
}
