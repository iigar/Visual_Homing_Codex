#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "visual_homing/camera_smoke.hpp"

namespace {

struct Fixture {
    vh::LiveRouteMatchingConfig config;
    vh::LiveRouteMatchingResult result;

    Fixture() {
        config.frames_to_capture = 100;
        config.expected_progress = "forward";
        config.require_endpoint_progress = true;
        config.stop_at_endpoint_progress = true;
        config.emit_dry_run_commands = true;
        config.require_dry_run_command_quality = true;
        config.use_live_telemetry_stream = true;
        config.require_live_telemetry_health = true;
        config.emit_external_nav_estimates = true;
        config.external_nav_expected_relative_altitude_m = 1.0;
        config.external_nav_expected_relative_altitude_tolerance_m = 0.25;
        result.started = true;
        result.frames_captured = 100;
        result.valid_matches = 100;
        result.first_tracked_progress = 0.0;
        result.min_tracked_progress_seen = 0.0;
        result.max_tracked_progress_seen = 1.0;
        result.dry_run_commands = 100;
        result.valid_dry_run_commands = 100;
        result.telemetry_warmup_passed = true;
        result.telemetry_health_ready_frames = 100;
        result.external_nav_estimates = 100;
        result.external_nav_valid_for_fc = 100;
        result.external_nav_altitude_valid_frames = 100;
        result.external_nav_relative_altitude_seen_frames = 100;
        result.external_nav_relative_altitude_min_m = 0.75;
        result.external_nav_relative_altitude_max_m = 1.25;
        result.visual_scale_valid = 100;
        result.visual_scale_ratio_min = 1.0;
        result.visual_scale_ratio_max = 1.0;
        result.final_live_output_gate_reason = "vehicle_not_armed";
        result.live_output_gate_blocked_frames = 100;
        result.live_output_gate_block_reasons = "vehicle_not_armed:100";
    }

    void evaluate() {
        vh::live_route_match_evaluate_route_quality(config, result);
        vh::live_route_match_evaluate_session_readiness(config, result);
        assert(result.external_nav_session_ready == result.external_nav_quality_ready);
        assert(result.external_nav_session_reason == result.external_nav_quality_reason);
        assert(result.external_nav_session_valid_for_fc == result.external_nav_valid_for_fc);
        // Reporting readiness must not turn into output permission or new sends.
        assert(!result.final_live_output_gate_allowed);
        assert(result.final_live_output_gate_reason == "vehicle_not_armed");
        assert(result.live_output_gate_blocked_frames == 100);
        assert(result.live_output_gate_block_reasons == "vehicle_not_armed:100");
        assert(result.external_nav_output_sent_frames == 0);
    }
};

void expect_ready(const Fixture& f) {
    assert(f.result.passed);
    assert(f.result.external_nav_strict_session_ready);
    assert(f.result.external_nav_strict_session_reason == "valid");
    assert(f.result.external_nav_quality_ready);
    assert(f.result.external_nav_quality_reason == "valid");
    assert(f.result.external_nav_operator_readiness == "ready");
    assert(f.result.external_nav_operator_reason == "valid");
}

void expect_quality_blocked(const Fixture& f, const std::string& reason) {
    assert(!f.result.external_nav_quality_ready);
    assert(f.result.external_nav_quality_reason == reason);
    assert(f.result.external_nav_operator_readiness == "blocked");
    assert(f.result.external_nav_operator_reason == reason);
}

void check_baseline_and_no_estimates() {
    Fixture ready;
    ready.evaluate();
    expect_ready(ready);
    assert(ready.result.external_nav_valid_fraction == 1.0);
    assert(ready.result.visual_scale_valid_fraction == 1.0);
    assert(ready.result.external_nav_altitude_blocker == "none");
    assert(ready.result.external_nav_expected_relative_altitude_required);
    assert(ready.result.external_nav_relative_altitude_window_passed);
    assert(!ready.result.visual_scale_required);

    Fixture none;
    none.config.emit_external_nav_estimates = false;
    none.result.external_nav_estimates = 0;
    none.result.external_nav_valid_for_fc = 0;
    none.result.visual_scale_valid = 0;
    none.result.external_nav_relative_altitude_seen_frames = 0;
    none.evaluate();
    assert(none.result.passed);
    assert(!none.result.external_nav_strict_session_ready);
    assert(!none.result.external_nav_quality_ready);
    assert(none.result.external_nav_strict_session_reason == "not_requested");
    assert(none.result.external_nav_quality_reason == "not_requested");
    assert(none.result.external_nav_operator_readiness == "not_requested");
    assert(none.result.external_nav_operator_reason == "not_requested");
    assert(none.result.external_nav_altitude_blocker == "not_requested");
    assert(none.result.external_nav_valid_fraction == 0.0);
    assert(none.result.visual_scale_valid_fraction == 0.0);
    assert(!none.result.external_nav_expected_relative_altitude_required);
}

void check_directional_and_endpoint_gates() {
    for (const std::string direction : {"forward", "reverse"}) {
        for (int failing = 0; failing < 3; ++failing) {
            Fixture f;
            f.config.expected_progress = direction;
            f.config.require_endpoint_progress = false;
            const bool reverse = direction == "reverse";
            auto& count = reverse ? f.result.reverse_progress_regressions : f.result.progress_regressions;
            auto& rollback = reverse ? f.result.reverse_progress_rollback : f.result.progress_rollback;
            count = f.config.max_progress_regressions + (failing == 1 ? 1 : 0);
            rollback = failing == 2 ? std::nextafter(f.config.max_progress_rollback, 1.0)
                                    : f.config.max_progress_rollback;
            // Opposite-direction/tracked diagnostics do not replace raw gates.
            (reverse ? f.result.progress_regressions : f.result.reverse_progress_regressions) = 1000;
            (reverse ? f.result.tracked_reverse_progress_regressions : f.result.tracked_progress_regressions) = 6;
            f.evaluate();
            assert(f.result.directional_progress_passed == (failing == 0));
            assert(!f.result.tracked_directional_progress_passed);
            assert(f.result.progress_gate_passed == (failing == 0));
            assert(f.result.passed == (failing == 0));
        }
        for (const bool runtime_controls : {false, true}) {
            Fixture f;
            f.config.expected_progress = direction;
            f.result.first_tracked_progress = direction == "reverse" ? 1.0 : 0.0;
            f.config.live_output_runtime_controls_provided = runtime_controls;
            f.result.progress_regressions = 100;
            f.result.reverse_progress_regressions = 100;
            f.evaluate();
            assert(f.result.endpoint_progress_passed);
            assert(!f.result.directional_progress_passed);
            assert(f.result.progress_gate_passed == !runtime_controls);
            assert(f.result.passed == !runtime_controls);
        }
    }
    Fixture incomplete;
    incomplete.result.first_tracked_progress = 0.5;
    incomplete.evaluate();
    assert(!incomplete.result.endpoint_progress_passed);
    assert(!incomplete.result.passed);
    Fixture any;
    any.config.expected_progress = "any";
    any.config.stop_at_endpoint_progress = false;
    any.result.progress_regressions = 1000;
    any.result.tracked_progress_regressions = 1000;
    any.evaluate();
    assert(any.result.directional_progress_passed && any.result.tracked_directional_progress_passed);
    expect_ready(any);
}

void check_command_quality_boundaries() {
    for (int failing = 0; failing <= 6; ++failing) {
        Fixture f;
        f.result.valid_dry_run_commands = failing == 1 ? 94 : 95;
        f.result.max_invalid_dry_run_command_streak = failing == 2 ? 4 : 3;
        f.result.max_abs_dry_run_yaw_rate_radps = failing == 3 ? std::nextafter(0.35, 1.0) : 0.35;
        f.result.dry_run_yaw_rate_sign_flips = failing == 4 ? 21 : 20;
        f.result.max_dry_run_yaw_rate_delta_radps = failing == 5 ? std::nextafter(0.15, 1.0) : 0.15;
        if (failing == 6) {
            f.result.dry_run_commands = 99;
        }
        f.evaluate();
        assert(f.result.dry_run_command_quality_passed == (failing == 0));
        if (failing == 0) {
            assert(f.result.valid_dry_run_command_fraction == 0.95);
            expect_ready(f);
        } else {
            assert(!f.result.passed);
            expect_quality_blocked(f, "route_session_not_passed");
        }
    }
    Fixture optional;
    optional.config.require_dry_run_command_quality = false;
    optional.result.dry_run_commands = 0;
    optional.result.valid_dry_run_commands = 0;
    optional.evaluate();
    assert(optional.result.valid_dry_run_command_fraction == 0.0);
    assert(!optional.result.dry_run_command_quality_passed);
    expect_ready(optional);
    Fixture disabled;
    disabled.config.emit_dry_run_commands = false;
    disabled.config.require_dry_run_command_quality = false;
    disabled.result.dry_run_commands = 0;
    disabled.evaluate();
    assert(disabled.result.dry_run_command_quality_passed);
    expect_ready(disabled);
}

void check_telemetry_and_verification_gates() {
    for (int failing = 0; failing < 3; ++failing) {
        Fixture f;
        if (failing == 0) f.result.telemetry_warmup_passed = false;
        if (failing == 1) f.result.telemetry_health_degraded_frames = 1;
        if (failing == 2) f.result.telemetry_health_ready_frames = 99;
        f.evaluate();
        assert(!f.result.live_telemetry_health_passed && !f.result.passed);
        // Overall route failure takes precedence over the later telemetry reason.
        expect_quality_blocked(f, "route_session_not_passed");
    }
    Fixture optional;
    optional.config.require_live_telemetry_health = false;
    optional.result.telemetry_warmup_passed = false;
    optional.evaluate();
    assert(!optional.result.live_telemetry_health_passed);
    expect_ready(optional);
    Fixture disabled;
    disabled.config.use_live_telemetry_stream = false;
    disabled.config.require_live_telemetry_health = false;
    disabled.result.telemetry_warmup_passed = false;
    disabled.evaluate();
    assert(disabled.result.live_telemetry_health_passed);
    expect_ready(disabled);
    for (const bool requested : {false, true}) {
        Fixture f;
        f.config.publish_progress_only_route_verification = requested;
        f.result.route_verification_passed = false;
        f.evaluate();
        assert(f.result.passed == !requested);
    }
}

void check_session_frames_and_ambiguity_precedence() {
    for (int failing = 0; failing < 3; ++failing) {
        Fixture f;
        if (failing == 0) f.result.started = false;
        if (failing == 1) f.result.frames_captured = 99;
        if (failing == 2) f.result.valid_matches = 99;
        f.evaluate();
        assert(!f.result.passed);
        assert(f.result.external_nav_strict_session_reason == "route_session_not_passed");
        expect_quality_blocked(f, "route_session_not_passed");
    }
    for (const bool confirmed_endpoint : {false, true}) {
        Fixture f;
        f.result.frames_captured = f.result.valid_matches = 80;
        f.result.dry_run_commands = f.result.valid_dry_run_commands = 80;
        f.result.telemetry_health_ready_frames = 80;
        f.result.endpoint_stop_triggered = confirmed_endpoint;
        f.result.ambiguous_endpoint_hold_triggered = !confirmed_endpoint;
        f.evaluate();
        if (confirmed_endpoint) {
            expect_ready(f);
        } else {
            assert(!f.result.passed);
            assert(!f.result.external_nav_quality_ready);
            assert(f.result.external_nav_strict_session_reason == "ambiguous_endpoint_hold");
            assert(f.result.external_nav_quality_reason == "ambiguous_endpoint_hold");
            assert(f.result.external_nav_operator_readiness == "marginal");
            assert(f.result.external_nav_operator_reason == "ambiguous_endpoint_hold");
        }
    }
    Fixture empty;
    empty.result.frames_captured = empty.result.valid_matches = 0;
    empty.result.endpoint_stop_triggered = true;
    empty.evaluate();
    assert(!empty.result.passed);
}

void check_strict_quality_and_operator_thresholds() {
    for (const std::uint64_t valid : {94U, 95U, 100U}) {
        Fixture f;
        f.result.external_nav_valid_for_fc = valid;
        f.result.external_nav_max_invalid_streak = 3;
        f.evaluate();
        assert(f.result.passed);
        assert(f.result.external_nav_strict_session_ready == (valid == 100));
        assert(f.result.external_nav_quality_ready == (valid >= 95));
        if (valid == 94) {
            expect_quality_blocked(f, "external_nav_valid_fraction_low");
        } else if (valid == 95) {
            assert(f.result.external_nav_strict_session_reason == "per_frame_external_nav_invalid");
            assert(f.result.external_nav_operator_readiness == "marginal");
            assert(f.result.external_nav_operator_reason == "external_nav_strict_session_not_ready");
        } else {
            expect_ready(f);
        }
    }
    Fixture streak;
    streak.result.external_nav_max_invalid_streak = 4;
    streak.evaluate();
    expect_quality_blocked(streak, "external_nav_invalid_streak_high");
    for (const std::string direction : {"forward", "reverse", "any"}) {
        for (int failing = 0; failing < 3; ++failing) {
            Fixture f;
            f.config.expected_progress = direction;
            f.config.stop_at_endpoint_progress = direction != "any";
            f.result.first_tracked_progress = direction == "reverse" ? 1.0 : 0.0;
            auto& count = direction == "reverse" ? f.result.tracked_reverse_progress_regressions
                                                : f.result.tracked_progress_regressions;
            auto& rollback = direction == "reverse" ? f.result.tracked_reverse_progress_rollback
                                                   : f.result.tracked_progress_rollback;
            count = failing == 1 ? 16 : 15;
            rollback = failing == 2 ? std::nextafter(1.0, 2.0) : 1.0;
            f.evaluate();
            if (failing == 0 || direction == "any") {
                expect_ready(f);
            } else {
                assert(f.result.external_nav_quality_ready);
                assert(f.result.external_nav_operator_readiness == "marginal");
                assert(f.result.external_nav_operator_reason == "route_tracked_directional_progress_soft_high");
            }
        }
    }
    Fixture priority;
    priority.result.tracked_progress_regressions = 16;
    priority.result.external_nav_valid_for_fc = 95;
    priority.evaluate();
    assert(priority.result.external_nav_operator_reason == "route_tracked_directional_progress_soft_high");
}

void check_altitude_window_and_diagnostic_blockers() {
    for (int failing = 0; failing < 3; ++failing) {
        Fixture f;
        if (failing == 0) f.result.external_nav_relative_altitude_seen_frames = 99;
        if (failing == 1) f.result.external_nav_relative_altitude_min_m = std::nextafter(0.75, 0.0);
        if (failing == 2) f.result.external_nav_relative_altitude_max_m = std::nextafter(1.25, 2.0);
        // Window failure precedes both fraction and streak failures.
        f.result.external_nav_valid_for_fc = 94;
        f.result.external_nav_max_invalid_streak = 4;
        f.evaluate();
        assert(!f.result.external_nav_relative_altitude_window_passed);
        expect_quality_blocked(f, "relative_altitude_out_of_expected_window");
    }
    for (const double value : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::quiet_NaN()}) {
        for (const bool tolerance : {false, true}) {
            Fixture f;
            (tolerance ? f.config.external_nav_expected_relative_altitude_tolerance_m
                       : f.config.external_nav_expected_relative_altitude_m) = value;
            f.result.external_nav_relative_altitude_seen_frames = 0;
            f.evaluate();
            assert(!f.result.external_nav_expected_relative_altitude_required);
            assert(f.result.external_nav_relative_altitude_window_passed);
            expect_ready(f);
        }
    }
    const char* reasons[] = {"none", "relative_altitude_not_seen", "relative_altitude_non_positive",
                             "bench_diagnostic_altitude_used", "altitude_not_valid"};
    for (int kind = 0; kind < 5; ++kind) {
        Fixture f;
        f.config.external_nav_expected_relative_altitude_m = 0.0;
        f.result.external_nav_altitude_valid_frames = kind == 0 ? 1 : 0;
        if (kind <= 1) f.result.external_nav_relative_altitude_seen_frames = 0;
        if (kind <= 2) f.result.external_nav_relative_altitude_max_m = 0.0;
        f.result.external_nav_bench_altitude_frames = kind <= 3 ? 1 : 0;
        f.evaluate();
        assert(f.result.external_nav_altitude_blocker == reasons[kind]);
        // This diagnostic string is not itself an extra readiness gate.
        expect_ready(f);
    }
}

void check_visual_scale_policy() {
    for (int failing = 0; failing < 4; ++failing) {
        Fixture f;
        f.config.external_nav.bench_diagnostic_altitude_m = 0.5;
        f.result.visual_scale_valid = failing == 1 ? 94 : 95;
        f.result.visual_scale_ratio_min = failing == 2 ? std::nextafter(0.80, 0.0) : 0.80;
        f.result.visual_scale_ratio_max = failing == 3 ? std::nextafter(1.25, 2.0) : 1.25;
        f.evaluate();
        assert(f.result.visual_scale_required);
        if (failing == 0) {
            expect_ready(f);
        } else {
            expect_quality_blocked(f, failing == 1 ? "visual_scale_valid_fraction_low"
                                                   : "visual_scale_ratio_out_of_range");
        }
    }
    for (const double altitude : {0.0, -1.0, std::numeric_limits<double>::infinity()}) {
        Fixture optional;
        optional.config.external_nav.bench_diagnostic_altitude_m = altitude;
        optional.result.visual_scale_valid = 0;
        optional.result.visual_scale_ratio_min = 0.1;
        optional.result.visual_scale_ratio_max = 10.0;
        optional.evaluate();
        assert(!optional.result.visual_scale_required);
        expect_ready(optional);
    }
    Fixture priority;
    priority.config.external_nav.bench_diagnostic_altitude_m = 0.5;
    priority.result.visual_scale_valid = 0;
    priority.result.visual_scale_ratio_min = 0.1;
    priority.result.external_nav_valid_for_fc = 94;
    priority.result.external_nav_max_invalid_streak = 4;
    priority.evaluate();
    expect_quality_blocked(priority, "external_nav_valid_fraction_low");
}

} // namespace

int main() {
    check_baseline_and_no_estimates();
    check_directional_and_endpoint_gates();
    check_command_quality_boundaries();
    check_telemetry_and_verification_gates();
    check_session_frames_and_ambiguity_precedence();
    check_strict_quality_and_operator_thresholds();
    check_altitude_window_and_diagnostic_blockers();
    check_visual_scale_policy();
}
