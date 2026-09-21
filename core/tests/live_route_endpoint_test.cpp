#include <cassert>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <tuple>

#include "visual_homing/camera_smoke.hpp"

namespace {

vh::Timestamp at_ms(int value) {
    return vh::Timestamp{} + std::chrono::milliseconds(value);
}

vh::LiveRouteMatchingConfig endpoint_config(const std::string& direction = "forward") {
    vh::LiveRouteMatchingConfig config;
    config.expected_progress = direction;
    config.stop_at_endpoint_progress = true;
    config.endpoint_start_progress = 0.25;
    config.endpoint_end_progress = 0.75;
    config.endpoint_dwell_ms = 100.0;
    config.endpoint_require_unambiguous_match = true;
    config.endpoint_min_top_match_gap = 0.125;
    config.endpoint_min_edge_top_match_gap = 0.0625;
    config.endpoint_ambiguous_hold_dwell_ms = 200.0;
    return config;
}

struct Run {
    vh::LiveRouteMatchingConfig config;
    vh::LiveRouteMatchEndpointState state;
    vh::LiveRouteMatchingResult result;
    vh::Frame frame;
    vh::RouteMatch match;

    explicit Run(vh::LiveRouteMatchingConfig settings = endpoint_config()) : config(settings) {
        // Same initial diagnostics as the camera loop, before its first frame.
        result.endpoint_dwell_required_ms = config.endpoint_dwell_ms;
        result.endpoint_dwell_passed = config.endpoint_dwell_ms <= 0.0;
        result.endpoint_confirmation_required = config.endpoint_require_unambiguous_match;
        result.endpoint_confirmation_passed = !config.endpoint_require_unambiguous_match;
        result.endpoint_confirmation_reason = config.endpoint_require_unambiguous_match ? "not_evaluated" : "disabled";
        result.ambiguous_endpoint_hold_required_ms = config.endpoint_ambiguous_hold_dwell_ms;
        result.ambiguous_endpoint_hold_reason = config.endpoint_allow_ambiguous_hold ? "not_evaluated" : "disabled";
        frame.id = 42;
        frame.width = 160;
        frame.height = 100;
        frame.timestamp = at_ms(-10000); // Dwell uses processing time, not frame time.
        match.valid = true;
        match.route_index = 7;
        match.progress = 0.91; // Raw evidence differs from the tracked decision input.
        match.confidence = 0.875;
    }

    bool step(int ms, std::optional<double> progress = 0.75,
              std::optional<double> top = 0.125, std::optional<double> edge = 0.0625) {
        return vh::live_route_match_update_endpoint(
            config, frame, match, progress, top, edge, at_ms(ms), state, result);
    }
};

auto diagnostics(const Run& run) {
    const auto& r = run.result;
    return std::make_tuple(
        run.state.endpoint_dwell_started_at, run.state.ambiguous_endpoint_hold_started_at,
        r.endpoint_dwell_ms, r.endpoint_dwell_required_ms, r.endpoint_dwell_passed,
        r.endpoint_confirmation_required, r.endpoint_confirmation_passed, r.endpoint_confirmation_reason,
        r.endpoint_top_match_gap, r.endpoint_edge_top_match_gap,
        r.ambiguous_endpoint_hold_dwell_ms, r.ambiguous_endpoint_hold_required_ms,
        r.ambiguous_endpoint_hold_reason, r.ambiguous_endpoint_hold_frame_id,
        r.ambiguous_endpoint_hold_route_index, r.ambiguous_endpoint_hold_progress,
        r.ambiguous_endpoint_hold_tracked_progress, r.ambiguous_endpoint_hold_confidence,
        r.endpoint_stop_triggered, r.ambiguous_endpoint_hold_triggered, r.stop_reason);
}

void check_confirmed_dwell_and_evidence() {
    for (const std::string direction : {"forward", "reverse"}) {
        Run run(endpoint_config(direction));
        const double boundary = direction == "forward" ? 0.75 : 0.25;
        const double outside = std::nextafter(boundary, 0.5);
        assert(!run.step(0, outside));
        assert(!run.state.endpoint_dwell_started_at);
        assert(run.result.endpoint_confirmation_reason == "not_at_endpoint");
        assert(!run.step(10, boundary));
        assert(run.state.endpoint_dwell_started_at == at_ms(10));
        assert(run.result.endpoint_confirmation_passed);
        assert(run.result.endpoint_confirmation_reason == "valid");
        assert(run.result.endpoint_dwell_ms == 0.0);
        assert(!run.step(109, boundary));
        assert(run.result.endpoint_dwell_ms == 99.0);
        assert(!run.result.endpoint_dwell_passed);
        run.frame.id = 99;
        run.match.route_index = 12;
        assert(run.step(110, boundary));
        const auto& r = run.result;
        assert(r.endpoint_dwell_ms == 100.0 && r.endpoint_dwell_passed);
        assert(r.endpoint_stop_triggered && !r.ambiguous_endpoint_hold_triggered);
        assert(r.stop_reason == "endpoint_progress_reached");
        assert(r.endpoint_stop_frame_id == 99);
        assert(r.endpoint_stop_frame_width == 160 && r.endpoint_stop_frame_height == 100);
        assert(r.endpoint_stop_route_index == 12);
        assert(r.endpoint_stop_progress == run.match.progress);
        assert(r.endpoint_stop_tracked_progress == boundary);
        assert(r.endpoint_stop_confidence == run.match.confidence);
        assert(!r.endpoint_stop_frame_written && r.endpoint_stop_frame_path.empty());
    }
}

void check_invalid_or_absent_progress_preserves_timers() {
    for (const bool ambiguous : {false, true}) {
        auto config = endpoint_config();
        config.endpoint_allow_ambiguous_hold = ambiguous;
        Run run(config);
        const double top = ambiguous ? 0.0 : 0.125;
        assert(!run.step(10, 0.75, top));
        const auto before = diagnostics(run);
        run.match.valid = false;
        run.frame.id = 500;
        // Even a stale endpoint value must not trigger/reset on an invalid frame.
        assert(!run.step(1000, 0.99, 0.0, std::nullopt));
        assert(diagnostics(run) == before);
        run.match.valid = true;
        assert(!run.step(2000, std::nullopt, std::nullopt, std::nullopt));
        assert(diagnostics(run) == before);
        // Characterization: elapsed time across the invalid gap counts on return.
        assert(run.step(2010, 0.75, top));
        if (ambiguous) {
            assert(run.result.ambiguous_endpoint_hold_dwell_ms == 2000.0);
            assert(run.result.ambiguous_endpoint_hold_triggered);
            assert(!run.result.endpoint_stop_triggered);
            assert(run.result.ambiguous_endpoint_hold_frame_id == 500);
        } else {
            assert(run.result.endpoint_dwell_ms == 2000.0);
            assert(run.result.endpoint_stop_triggered);
            assert(!run.result.ambiguous_endpoint_hold_triggered);
        }
    }
    Run fresh;
    const auto initial = diagnostics(fresh);
    fresh.match.valid = false;
    assert(!fresh.step(0));
    assert(diagnostics(fresh) == initial);
}

void check_leaving_endpoint_resets_both_timers() {
    for (const bool ambiguous : {false, true}) {
        auto config = endpoint_config();
        config.endpoint_allow_ambiguous_hold = true;
        Run run(config);
        const double top = ambiguous ? 0.0 : 0.125;
        const int dwell = ambiguous ? 200 : 100;
        assert(!run.step(10, 0.75, top));
        assert(!run.step(50, 0.75, top));
        assert(!run.step(60, 0.5, top));
        assert(!run.state.endpoint_dwell_started_at);
        assert(!run.state.ambiguous_endpoint_hold_started_at);
        assert(run.result.endpoint_dwell_ms == 0.0);
        assert(run.result.ambiguous_endpoint_hold_dwell_ms == 0.0);
        assert(!run.result.endpoint_dwell_passed);
        assert(!run.result.endpoint_confirmation_passed);
        assert(run.result.endpoint_confirmation_reason == "not_at_endpoint");
        assert(run.result.ambiguous_endpoint_hold_reason == "not_at_endpoint");
        assert(!run.step(1000, 0.75, top));
        assert(!run.step(1000 + dwell - 1, 0.75, top));
        assert(run.step(1000 + dwell, 0.75, top));
    }
}

void check_confirmation_reasons_and_boundaries() {
    struct Case { std::optional<double> top, edge; const char* reason; };
    const Case cases[] = {
        {std::nullopt, std::nullopt, "top_match_gap_unavailable"},
        {0.0, std::nullopt, "edge_top_match_gap_unavailable"},
        {0.0, 0.0, "top_match_gap_low"},
        {std::nextafter(0.125, 0.0), 0.0625, "top_match_gap_low"},
        {0.125, std::nextafter(0.0625, 0.0), "edge_top_match_gap_low"},
    };
    for (const auto& c : cases) {
        Run run;
        assert(!run.step(10));
        assert(!run.step(2000, 0.75, c.top, c.edge));
        assert(!run.result.endpoint_confirmation_passed);
        assert(run.result.endpoint_confirmation_reason == c.reason);
        assert(run.result.endpoint_dwell_ms == 0.0 && !run.result.endpoint_dwell_passed);
        assert(!run.state.endpoint_dwell_started_at);
        assert(!run.state.ambiguous_endpoint_hold_started_at);
        assert(run.result.ambiguous_endpoint_hold_reason == "disabled");
        // Missing gaps retain the previous measured diagnostic values.
        assert(run.result.endpoint_top_match_gap == (c.top && c.edge ? *c.top : 0.125));
        assert(run.result.endpoint_edge_top_match_gap == (c.top && c.edge ? *c.edge : 0.0625));
        assert(!run.step(3000));
        assert(run.step(3100));
    }
}

void check_ambiguous_hold_and_switching_confirmation() {
    auto config = endpoint_config();
    config.endpoint_allow_ambiguous_hold = true;
    Run run(config);
    assert(!run.step(0, 0.75, 0.0));
    assert(run.result.ambiguous_endpoint_hold_reason == "top_match_gap_low");
    assert(run.result.ambiguous_endpoint_hold_frame_id == run.frame.id);
    assert(run.result.ambiguous_endpoint_hold_route_index == run.match.route_index);
    assert(run.result.ambiguous_endpoint_hold_progress == run.match.progress);
    assert(run.result.ambiguous_endpoint_hold_tracked_progress == 0.75);
    assert(run.result.ambiguous_endpoint_hold_confidence == run.match.confidence);
    assert(!run.step(100, 0.75, std::nullopt));
    assert(run.result.ambiguous_endpoint_hold_dwell_ms == 100.0);
    assert(run.result.ambiguous_endpoint_hold_reason == "top_match_gap_unavailable");
    assert(!run.step(199, 0.75, 0.125, 0.0));
    assert(run.result.ambiguous_endpoint_hold_reason == "edge_top_match_gap_low");
    assert(run.step(200, 0.75, 0.125, std::nullopt));
    assert(run.result.ambiguous_endpoint_hold_triggered && !run.result.endpoint_stop_triggered);
    assert(run.result.ambiguous_endpoint_hold_dwell_ms == 200.0);
    assert(run.result.stop_reason == "ambiguous_endpoint_hold");
    assert(!run.result.endpoint_stop_frame_written && run.result.endpoint_stop_frame_id == 0);

    Run alternating(config);
    assert(!alternating.step(0, 0.75, 0.0));
    assert(!alternating.step(100));
    assert(!alternating.state.ambiguous_endpoint_hold_started_at);
    assert(alternating.result.ambiguous_endpoint_hold_dwell_ms == 0.0);
    assert(alternating.result.ambiguous_endpoint_hold_reason == "endpoint_confirmed");
    assert(alternating.state.endpoint_dwell_started_at == at_ms(100));
    assert(!alternating.step(199));
    assert(!alternating.step(200, 0.75, 0.0));
    assert(!alternating.state.endpoint_dwell_started_at);
    assert(alternating.state.ambiguous_endpoint_hold_started_at == at_ms(200));
    assert(!alternating.step(399, 0.75, 0.0));
    assert(alternating.step(400, 0.75, 0.0));
}

void check_disabled_and_zero_dwell() {
    auto config = endpoint_config();
    config.stop_at_endpoint_progress = false;
    Run disabled(config);
    const auto before = diagnostics(disabled);
    assert(!disabled.step(500));
    assert(diagnostics(disabled) == before);

    config = endpoint_config("any"); // Runtime rejects stop+any; helper reaches neither endpoint.
    Run any(config);
    assert(!any.step(0, 0.0) && !any.step(1000, 1.0));
    assert(!any.state.endpoint_dwell_started_at);

    config = endpoint_config();
    config.endpoint_require_unambiguous_match = false;
    config.endpoint_dwell_ms = 0.0;
    config.export_endpoint_stop_frame = true;
    config.endpoint_stop_frame_dir = "unused-by-decision-helper";
    Run immediate(config);
    assert(!immediate.step(0, 0.5, std::nullopt, std::nullopt));
    assert(immediate.result.endpoint_dwell_passed);
    assert(immediate.step(1, 0.75, std::nullopt, std::nullopt));
    assert(immediate.result.endpoint_confirmation_reason == "disabled");
    assert(immediate.result.endpoint_dwell_ms == 0.0);
    assert(!immediate.result.endpoint_stop_frame_written);
    assert(immediate.result.endpoint_stop_frame_path.empty());

    config = endpoint_config();
    config.endpoint_dwell_ms = 0.0;
    Run unconfirmed(config);
    assert(!unconfirmed.step(0, 0.75, std::nullopt));
    assert(!unconfirmed.result.endpoint_dwell_passed);
    config.endpoint_allow_ambiguous_hold = true;
    config.endpoint_ambiguous_hold_dwell_ms = 0.0;
    Run immediate_hold(config);
    assert(immediate_hold.step(0, 0.75, std::nullopt));
    assert(immediate_hold.result.stop_reason == "ambiguous_endpoint_hold");
}

void check_repeated_and_regressing_processing_time() {
    Run run;
    assert(!run.step(100));
    assert(!run.step(100));
    assert(run.result.endpoint_dwell_ms == 0.0);
    assert(!run.step(50));
    assert(run.result.endpoint_dwell_ms == -50.0);
    assert(!run.result.endpoint_dwell_passed);
    assert(run.state.endpoint_dwell_started_at == at_ms(100));
    assert(run.step(200));
}

void check_progress_tracker_composition() {
    for (const std::string direction : {"forward", "reverse"}) {
        Run run(endpoint_config(direction));
        vh::LiveRouteMatchProgressState progress_state;
        const double start = direction == "forward" ? 0.0 : 1.0;
        const double end = 1.0 - start;
        auto progress = vh::live_route_match_record_progress(direction, start, true, progress_state, run.result);
        assert(!run.step(0, progress));
        run.match.progress = end;
        progress = vh::live_route_match_record_progress(direction, end, true, progress_state, run.result);
        assert(progress && !vh::live_route_match_endpoint_reached(run.config, *progress));
        assert(!run.step(10, progress)); // Raw endpoint alone cannot start dwell.
        assert(!run.state.endpoint_dwell_started_at);
        for (int i = 0; i < 30 && !run.state.endpoint_dwell_started_at; ++i) {
            progress = vh::live_route_match_record_progress(direction, end, true, progress_state, run.result);
            assert(!run.step(20 + i, progress));
        }
        assert(run.state.endpoint_dwell_started_at);
        const auto before = diagnostics(run);
        run.match.valid = false;
        progress = vh::live_route_match_record_progress(direction, end, false, progress_state, run.result);
        assert(!progress && !run.step(1000, progress));
        assert(diagnostics(run) == before);
        run.match.valid = true;
        progress = vh::live_route_match_record_progress(direction, end, true, progress_state, run.result);
        assert(run.step(1010, progress));
        assert(run.result.endpoint_stop_triggered);
    }
}

} // namespace

int main() {
    check_confirmed_dwell_and_evidence();
    check_invalid_or_absent_progress_preserves_timers();
    check_leaving_endpoint_resets_both_timers();
    check_confirmation_reasons_and_boundaries();
    check_ambiguous_hold_and_switching_confirmation();
    check_disabled_and_zero_dwell();
    check_repeated_and_regressing_processing_time();
    check_progress_tracker_composition();
}
