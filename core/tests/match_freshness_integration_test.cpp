#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "visual_homing/bounded_navigator.hpp"
#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/gray8_route_matcher.hpp"
#include "visual_homing/health_monitor.hpp"

namespace {
using namespace std::chrono_literals;
std::size_t cases = 0;

vh::Timestamp at_ms(int value) { return vh::Timestamp{} + std::chrono::milliseconds(value); }

vh::RouteSignatureFile make_route() {
    vh::RouteSignatureFile route;
    std::uint32_t seed = 0x48127365;
    for (std::uint64_t i = 0; i < 3; ++i) {
        vh::RouteSignatureEntry entry;
        entry.frame_id = i;
        entry.width = 16;
        entry.height = 12;
        for (int pixel = 0; pixel < 16 * 12; ++pixel) {
            seed = seed * 1664525U + 1013904223U;
            entry.payload.push_back(static_cast<std::uint8_t>(seed >> 24));
        }
        route.entries.push_back(std::move(entry));
    }
    return route;
}

// Real queue/worker admission, with an in-memory processor in place of storage.
class CountingProcessor final : public vh::VerificationPublicationProcessor {
public:
    explicit CountingProcessor(std::atomic<std::uint64_t>& calls) : calls_(calls) {}
    vh::LiveVerificationFrameResult process(
        const vh::Frame&, const vh::LiveVerificationFrameContext& context) override {
        assert(context.health_ready && !context.has_local_pose);
        ++calls_;
        vh::LiveVerificationFrameResult result;
        result.decision.valid = true;
        return result;
    }
private:
    std::atomic<std::uint64_t>& calls_;
};

struct Sample {
    vh::Frame frame;
    vh::LiveRouteVerificationObservation observation;
};

struct Run {
    std::atomic<std::uint64_t> processed{0};
    vh::BoundedVerificationPublisher publisher{{}, std::make_unique<CountingProcessor>(processed)};
    vh::LiveRouteVerificationProducer producer{{.minimum_match_confidence = 0.95,
        .maximum_frame_context_age_ms = 200.0, .maximum_scalar_age_ms = 200.0,
        .require_mavlink_health = true, .local_frame_id = {},
        .local_frame_revision = {}, .local_frame_convention = {}}, publisher};
    vh::RouteSignatureFile route = make_route();
    vh::Gray8RouteMatcher matcher;
    vh::BoundedNavigator navigator{{.minimum_confidence = 0.95, .max_match_age_ms = 200.0,
        .forward_speed_mps = 0.25}};
    vh::HealthMonitor health{at_ms(1)};
    vh::LiveRouteMatchProgressState progress_state;
    vh::LiveRouteMatchingResult progress_result;

    explicit Run(bool direction_enabled = true)
        : matcher(route, {.minimum_confidence = 0.99,
            .max_direction_shift_px = direction_enabled ? 2 : 0, .radians_per_pixel = 0.03}) {
        assert(publisher.start());
    }
    ~Run() { publisher.stop(true); }

    Sample sample(std::optional<std::size_t> index = 0,
                  vh::Timestamp frame_time = at_ms(1000),
                  vh::Timestamp evaluation_time = at_ms(1100), unsigned links = 7) {
        Sample value;
        value.frame = {.id = 100 + progress_result.frames_captured, .timestamp = frame_time,
            .width = 16, .height = 12,
            .data = index ? route.entries.at(*index).payload : std::vector<std::uint8_t>(16 * 12, 128)};
        const auto match = matcher.match(value.frame);
        assert(match.timestamp == frame_time);
        health.set_links((links & 1) != 0, (links & 2) != 0, (links & 4) != 0);
        health.observe_processed_frame(value.frame, evaluation_time, evaluation_time);
        health.set_route_match_confidence(match.confidence);
        const auto progress = vh::live_route_match_record_progress(
            "forward", match.progress, match.valid, progress_state, progress_result);
        // Scalar values and link status are scripted test inputs, not telemetry evidence.
        value.observation = vh::make_progress_only_live_route_verification_observation(
            match, progress, health.snapshot(evaluation_time),
            {true, evaluation_time, 0.6}, {true, evaluation_time, 1.0});
        assert(!value.observation.local_pose);
        return value;
    }

    void check(const Sample& value, bool navigation_valid, const char* rejection = nullptr) {
        const auto submissions_before = publisher.metrics().submissions;
        const auto processed_before = processed.load();
        // Compare independent consumers; no command sink or live session is started.
        const auto command = navigator.update(value.observation.match, value.observation.health);
        assert(command.valid == navigation_valid);
        assert(command.timestamp == value.observation.health.timestamp);
        if (!navigation_valid) {
            assert(command.vx_mps == 0.0 && command.vy_mps == 0.0 && command.yaw_rate_radps == 0.0);
        }
        const auto result = producer.submit(value.frame, value.observation);
        if (rejection) {
            assert(result.status == vh::LiveRouteVerificationStatus::Rejected);
            assert(result.reason == rejection);
            assert(!result.context && !result.submission);
            assert(publisher.metrics().submissions == submissions_before);
            assert(processed.load() == processed_before);
        } else {
            assert(result.status == vh::LiveRouteVerificationStatus::Accepted);
            assert(result.context && result.submission);
            assert(result.context->route_progress == *value.observation.tracked_route_progress);
            assert(result.context->yaw_rad == value.observation.match.direction_error_rad);
            assert(!result.context->has_local_pose);
            assert(publisher.wait_until_idle(1000ms));
            assert(publisher.metrics().submissions == submissions_before + 1);
            assert(processed.load() == processed_before + 1);
        }
        assert(publisher.metrics().outstanding_jobs == 0);
        ++cases;
    }
};

void check_frame_age_and_recovery() {
    Run run;
    const auto tick = vh::Clock::duration{1};
    run.check(run.sample(0, at_ms(1000), at_ms(1000)), true);
    run.check(run.sample(0, at_ms(1000), at_ms(1200)), true); // Inclusive age limit.
    run.check(run.sample(0, at_ms(1000), at_ms(1200) + tick), false, "health_frame_context_stale");
    run.check(run.sample(0, at_ms(1300) + tick, at_ms(1300)), false, "health_frame_context_stale");
    run.check(run.sample(0, at_ms(1), at_ms(2700)), false, "health_frame_context_stale");
    const auto invalid = run.sample(std::nullopt, at_ms(2800), at_ms(2800));
    assert(!invalid.observation.match.valid && !invalid.observation.tracked_route_progress);
    run.check(invalid, false, "route_match_invalid");
    const auto recovered = run.sample(2, at_ms(2900), at_ms(2910));
    assert(recovered.observation.match.valid && recovered.observation.match.progress == 1.0);
    assert(*recovered.observation.tracked_route_progress < recovered.observation.match.progress);
    run.check(recovered, true);
    assert(run.producer.metrics().accepted == 3 && run.producer.metrics().rejected == 4);
    assert(run.publisher.metrics().completed == 3);
}

void check_health_and_binding() {
    Run run;
    for (const unsigned links : {6U, 5U, 3U}) {
        run.check(run.sample(0, at_ms(1000), at_ms(1100), links), false, "health_not_ready");
    }
    auto mismatch = run.sample();
    mismatch.observation.health.route_match_confidence = 0.999;
    run.check(mismatch, true, "health_match_confidence_mismatch");
    mismatch = run.sample();
    mismatch.frame.timestamp += vh::Clock::duration{1};
    run.check(mismatch, true, "route_match_frame_mismatch");
    run.check(run.sample(), true);
}

void check_scalar_age() {
    Run run;
    const auto tick = vh::Clock::duration{1};
    auto value = run.sample(0, at_ms(1200), at_ms(1200));
    value.observation.altitude.timestamp = at_ms(1000);
    value.observation.scale_ratio.timestamp = at_ms(1000);
    run.check(value, true); // Both scalar ages exactly at their inclusive limit.
    value.observation.altitude.timestamp -= tick;
    run.check(value, true, "altitude_observation_invalid");
    value.observation.altitude.timestamp = at_ms(1200);
    value.observation.scale_ratio.timestamp -= tick;
    run.check(value, true, "scale_observation_invalid");
    value.observation.scale_ratio.timestamp = at_ms(1200);
    value.observation.altitude.timestamp = at_ms(1200) + tick;
    run.check(value, true, "altitude_observation_invalid");
    value.observation.altitude.timestamp = at_ms(1200);
    value.observation.scale_ratio.timestamp = at_ms(1200) + tick;
    run.check(value, true, "scale_observation_invalid");
    run.check(run.sample(0, at_ms(1300), at_ms(1310)), true);
}

void check_different_consumer_contracts() {
    Run run;
    // Navigator checks relative age; producer additionally rejects a zero timestamp.
    run.check(run.sample(0, at_ms(0), at_ms(100)), true, "health_frame_context_stale");
    Run no_direction(false);
    const auto value = no_direction.sample();
    assert(value.observation.match.valid && !value.observation.match.direction_observation_valid);
    assert(!value.observation.yaw.valid);
    // Valid route match and valid image-derived yaw are separate requirements.
    no_direction.check(value, true, "yaw_observation_invalid");
}

} // namespace

int main() {
    check_frame_age_and_recovery();
    check_health_and_binding();
    check_scalar_age();
    check_different_consumer_contracts();
    std::cout << "Matcher downstream freshness cases: " << cases << '\n';
}
