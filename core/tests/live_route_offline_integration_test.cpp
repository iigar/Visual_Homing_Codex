#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/gray8_route_matcher.hpp"
#include "visual_homing/health_monitor.hpp"

namespace {

constexpr int width = 16, height = 12;
constexpr std::size_t last_index = 16;
std::size_t scenarios = 0;

vh::Timestamp at_ms(int ms) {
    return vh::Timestamp{} + std::chrono::milliseconds(ms);
}

enum class Scene { Distinct, DuplicateEndpoint, FlatEdges };

vh::RouteSignatureFile make_route(Scene scene, bool reverse) {
    vh::RouteSignatureFile route;
    std::uint32_t seed = 0x17382945;
    for (std::size_t i = 0; i <= last_index; ++i) {
        vh::RouteSignatureEntry entry;
        entry.frame_id = i;
        entry.timestamp_ns = i * 100000000;
        entry.width = width;
        entry.height = height;
        for (int pixel = 0; pixel < width * height; ++pixel) {
            seed = seed * 1664525U + 1013904223U;
            entry.payload.push_back(scene == Scene::FlatEdges
                ? static_cast<std::uint8_t>(i * 15)
                : static_cast<std::uint8_t>(seed >> 24));
        }
        route.entries.push_back(std::move(entry));
    }
    if (scene == Scene::DuplicateEndpoint) {
        route.entries[reverse ? 1 : last_index].payload =
            route.entries[reverse ? 0 : last_index - 1].payload;
    }
    return route;
}

vh::LiveRouteMatchingConfig configuration(bool reverse, bool allow_hold) {
    vh::LiveRouteMatchingConfig config;
    config.frames_to_capture = 64;
    config.expected_progress = reverse ? "reverse" : "forward";
    config.minimum_confidence = 0.99;
    config.require_endpoint_progress = true;
    config.stop_at_endpoint_progress = true;
    config.endpoint_dwell_ms = 100.0;
    config.endpoint_require_unambiguous_match = true;
    config.endpoint_min_top_match_gap = 0.01;
    config.endpoint_min_edge_top_match_gap = 0.01;
    config.endpoint_allow_ambiguous_hold = allow_hold;
    config.endpoint_ambiguous_hold_dwell_ms = 200.0;
    return config;
}

struct Observation {
    vh::Frame frame;
    vh::RouteMatch match;
    std::optional<double> tracked, top_gap, edge_gap;
    bool stop = false;
};

// Test-only composition of existing public APIs, in camera-loop order.
// Pixels and both clocks are scripted. No capture, wall clock, sleep, output,
// estimator or invented telemetry evidence is involved.
struct Run {
    bool reverse;
    vh::LiveRouteMatchingConfig config;
    vh::RouteSignatureFile route;
    vh::Gray8RouteMatcher matcher;
    vh::HealthMonitor health{at_ms(0)};
    vh::LiveRouteMatchProgressState progress_state;
    vh::LiveRouteMatchEndpointState endpoint_state;
    vh::LiveRouteMatchingResult result;

    explicit Run(bool backwards, Scene scene = Scene::Distinct, bool allow_hold = false,
                 std::size_t window = 0, bool scale = false)
        : reverse(backwards), config(configuration(backwards, allow_hold)),
          route(make_route(scene, backwards)),
          matcher(route, {.window_radius = window, .minimum_confidence = config.minimum_confidence,
              .enable_scale_refinement = scale, .top_candidate_count = 2}) {
        result.started = true;
        result.endpoint_dwell_required_ms = config.endpoint_dwell_ms;
        result.endpoint_confirmation_required = true;
        result.endpoint_confirmation_passed = false;
        result.endpoint_confirmation_reason = "not_evaluated";
        result.ambiguous_endpoint_hold_required_ms = config.endpoint_ambiguous_hold_dwell_ms;
        result.ambiguous_endpoint_hold_reason = allow_hold ? "not_evaluated" : "disabled";
    }

    std::size_t index(std::size_t step) const { return reverse ? last_index - step : step; }

    Observation step(std::optional<std::size_t> route_index, int processing_ms,
                     std::optional<int> frame_ms = std::nullopt) {
        assert(!result.endpoint_stop_triggered && !result.ambiguous_endpoint_hold_triggered);
        Observation observation;
        observation.frame = {.id = 100 + result.frames_captured,
            .timestamp = at_ms(frame_ms.value_or(processing_ms - 3)),
            .width = width, .height = height,
            .data = route_index ? route.entries.at(*route_index).payload
                : std::vector<std::uint8_t>(width * height, 128)};
        observation.match = matcher.match(observation.frame);
        const auto& top = matcher.recent_top_candidates();
        assert(top.size() == 2);
        observation.top_gap = top[0].confidence - top[1].confidence;
        const auto edge = matcher.probe_edge_diagnostics(observation.frame, 2);
        assert(edge.top_candidates.size() == 2);
        observation.edge_gap = edge.top_candidates[0].confidence - edge.top_candidates[1].confidence;
        const auto timing = health.observe_processed_frame(
            observation.frame, at_ms(processing_ms - 2), at_ms(processing_ms));
        health.set_route_match_confidence(observation.match.confidence);
        observation.tracked = vh::live_route_match_record_progress(config.expected_progress,
            observation.match.progress, observation.match.valid, progress_state, result);
        result.last_frame_age_ms = timing.frame_age_ms;
        result.last_processing_latency_ms = timing.processing_latency_ms;
        observation.stop = vh::live_route_match_update_endpoint(config, observation.frame,
            observation.match, observation.tracked, observation.top_gap, observation.edge_gap,
            at_ms(processing_ms), endpoint_state, result);
        assert(observation.match.timestamp == observation.frame.timestamp);
        return observation;
    }

    Observation traverse(std::optional<int> fixed_frame_ms = std::nullopt) {
        Observation observation;
        for (std::size_t i = 0; i <= last_index; ++i) {
            observation = step(index(i), 1000 + static_cast<int>(i) * 100, fixed_frame_ms);
            assert(observation.match.valid && observation.match.confidence == 1.0);
            assert(observation.tracked && !observation.stop);
        }
        return observation;
    }

    void finish(bool passed) {
        vh::live_route_match_evaluate_route_quality(config, result);
        vh::live_route_match_evaluate_session_readiness(config, result);
        assert(result.passed == passed);
        assert(result.external_nav_estimates == 0);
        assert(!result.external_nav_quality_ready && !result.external_nav_strict_session_ready);
        assert(result.external_nav_quality_reason == "not_requested");
        assert(result.external_nav_strict_session_reason == "not_requested");
        assert(result.external_nav_operator_readiness == "not_requested");
        assert(!result.final_live_output_gate_allowed && result.external_nav_output_sent_frames == 0);
        assert(!result.endpoint_stop_frame_written && result.endpoint_stop_frame_path.empty());
        ++scenarios;
    }
};

void check_complete(bool reverse, std::size_t window, bool scale) {
    Run run(reverse, Scene::Distinct, false, window, scale);
    const auto arrival = run.traverse();
    assert(arrival.match.route_index == run.index(last_index));
    assert(run.endpoint_state.endpoint_dwell_started_at == at_ms(2600));
    assert(!run.result.endpoint_dwell_passed);
    assert(*arrival.top_gap >= run.config.endpoint_min_top_match_gap);
    assert(*arrival.edge_gap >= run.config.endpoint_min_edge_top_match_gap);
    assert(!run.step(run.index(last_index), 2699).stop);
    assert(run.result.endpoint_dwell_ms == 99.0);
    const auto stop = run.step(run.index(last_index), 2700);
    assert(stop.stop && run.result.endpoint_stop_triggered);
    assert(run.result.endpoint_dwell_ms == 100.0);
    assert(run.result.stop_reason == "endpoint_progress_reached");
    assert(run.result.endpoint_stop_frame_id == stop.frame.id);
    assert(run.result.endpoint_stop_route_index == stop.match.route_index);
    assert(run.result.endpoint_stop_progress == stop.match.progress);
    assert(run.result.endpoint_stop_tracked_progress == *stop.tracked);
    assert(run.result.endpoint_stop_confidence == stop.match.confidence);
    assert(run.result.frames_captured == 19 && run.result.valid_matches == 19);
    assert(run.result.last_frame_age_ms == 1.0 && run.result.last_processing_latency_ms == 2.0);
    run.finish(true); // Confirmed endpoint permits fewer than 64 requested frames.
    assert(run.result.endpoint_progress_passed && run.result.progress_gate_passed);
}

void check_raw_endpoint_is_not_tracked_endpoint(bool reverse) {
    Run run(reverse);
    assert(!run.step(run.index(0), 1000).stop);
    const auto jump = run.step(run.index(last_index), 2000);
    assert(jump.match.valid && jump.match.route_index == run.index(last_index));
    assert(vh::live_route_match_endpoint_reached(run.config, jump.match.progress));
    assert(!vh::live_route_match_endpoint_reached(run.config, *jump.tracked));
    assert(!jump.stop && !run.endpoint_state.endpoint_dwell_started_at);
    assert(run.result.endpoint_confirmation_reason == "not_at_endpoint");
    run.finish(false);
}

void check_invalid_gap(bool reverse) {
    Run run(reverse);
    run.traverse();
    const auto tracked = run.result.last_tracked_progress;
    const auto gap = run.step(std::nullopt, 2650);
    assert(!gap.match.valid && !gap.tracked && !gap.stop);
    assert(run.result.frames_captured == 18 && run.result.valid_matches == 17);
    assert(run.result.last_tracked_progress == tracked);
    assert(run.endpoint_state.endpoint_dwell_started_at == at_ms(2600));
    const auto resumed = run.step(run.index(last_index), 2800);
    // Existing policy counts elapsed time across an invalid-match gap.
    assert(resumed.stop && run.result.endpoint_dwell_ms == 200.0);
    assert(run.result.valid_matches == 18 && run.result.frames_captured == 19);
    run.finish(false); // Endpoint stop does not erase the invalid frame.
    assert(run.result.endpoint_progress_passed && run.result.progress_gate_passed);
}

void check_ambiguity(bool reverse, Scene scene, bool allow_hold) {
    Run run(reverse, scene, allow_hold);
    run.traverse();
    // Duplicate endpoint may need more smoothing before its tracked threshold.
    for (int ms = 2700; ms <= 2800; ms += 100) {
        assert(!run.step(run.index(last_index), ms).stop);
        if (run.endpoint_state.ambiguous_endpoint_hold_started_at) break;
    }
    const auto reason = scene == Scene::DuplicateEndpoint ? "top_match_gap_low" : "edge_top_match_gap_low";
    assert(!run.result.endpoint_confirmation_passed);
    assert(run.result.endpoint_confirmation_reason == reason);
    assert(!run.endpoint_state.endpoint_dwell_started_at);
    if (scene == Scene::DuplicateEndpoint) {
        assert(run.result.endpoint_top_match_gap == 0.0);
    } else {
        assert(run.result.endpoint_top_match_gap > run.config.endpoint_min_top_match_gap);
        assert(run.result.endpoint_edge_top_match_gap == 0.0);
    }
    if (allow_hold) {
        assert(run.endpoint_state.ambiguous_endpoint_hold_started_at);
        const auto started = *run.endpoint_state.ambiguous_endpoint_hold_started_at;
        const auto started_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            started.time_since_epoch()).count());
        assert(!run.step(run.index(last_index), started_ms + 199).stop);
        const auto held = run.step(run.index(last_index), started_ms + 200);
        assert(held.stop && run.result.ambiguous_endpoint_hold_triggered);
        assert(run.result.ambiguous_endpoint_hold_frame_id == held.frame.id);
        assert(run.result.stop_reason == "ambiguous_endpoint_hold");
        assert(run.result.ambiguous_endpoint_hold_dwell_ms == 200.0);
    } else {
        assert(!run.step(run.index(last_index), 10000).stop);
        assert(!run.result.ambiguous_endpoint_hold_triggered);
    }
    assert(!run.result.endpoint_stop_triggered);
    run.finish(false); // Ambiguous hold does not qualify as a confirmed early stop.
}

void check_incomplete_capture(bool reverse) {
    Run empty(reverse);
    empty.finish(false);
    Run partial(reverse);
    for (std::size_t i = 0; i < 8; ++i) assert(!partial.step(partial.index(i), 1000 + static_cast<int>(i) * 100).stop);
    assert(partial.result.valid_matches == partial.result.frames_captured);
    partial.finish(false);
    Run endpoint_only(reverse);
    assert(!endpoint_only.step(endpoint_only.index(last_index), 1000).stop);
    assert(endpoint_only.step(endpoint_only.index(last_index), 1100).stop);
    endpoint_only.finish(false);
    assert(!endpoint_only.result.endpoint_progress_passed); // No observed route start.
}

void check_stale_frame_timestamps(bool reverse) {
    Run run(reverse);
    run.traverse(0);
    assert(!run.step(run.index(last_index), 2699, 0).stop);
    assert(run.result.endpoint_dwell_ms == 99.0);
    assert(run.step(run.index(last_index), 2700, 0).stop);
    assert(run.result.last_frame_age_ms == 2698.0);
    // Route-only readiness has no age gate. This characterizes its limit;
    // it is not evidence of readiness for navigation or output.
    run.finish(true);
}

} // namespace

int main() {
    for (const bool reverse : {false, true}) {
        check_complete(reverse, 0, false);
        check_complete(reverse, 3, false);
        check_complete(reverse, 3, true);
        check_raw_endpoint_is_not_tracked_endpoint(reverse);
        check_invalid_gap(reverse);
        check_ambiguity(reverse, Scene::DuplicateEndpoint, false);
        check_ambiguity(reverse, Scene::DuplicateEndpoint, true);
        check_ambiguity(reverse, Scene::FlatEdges, false);
        check_incomplete_capture(reverse);
        check_stale_frame_timestamps(reverse);
    }
    std::cout << "Offline matcher/progress/endpoint/readiness scenarios: " << scenarios << '\n';
}
