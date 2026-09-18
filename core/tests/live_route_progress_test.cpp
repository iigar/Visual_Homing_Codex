#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

#include "visual_homing/camera_smoke.hpp"

namespace {

void assert_near(double actual, double expected) {
    assert(std::abs(actual - expected) < 1.0e-12);
}

void check_trace(const std::string& direction,
                 const std::array<std::optional<double>, 8>& expected,
                 std::uint64_t tracked_regressions,
                 double tracked_rollback,
                 std::uint64_t tracked_reverse_regressions,
                 double tracked_reverse_rollback,
                 double tracked_min,
                 double tracked_max) {
    vh::LiveRouteMatchProgressState state;
    vh::LiveRouteMatchingResult result;
    constexpr std::array<double, 8> raw{1.0, 0.2, 0.0, 0.8, 0.24, 0.0, 0.0, 0.99};
    constexpr std::array<bool, 8> valid{false, true, false, true, true, true, true, false};
    constexpr std::array<std::uint64_t, 8> valid_counts{0, 1, 1, 2, 3, 4, 5, 5};

    for (std::size_t i = 0; i < raw.size(); ++i) {
        const auto previous_state = state;
        const auto previous_tracked = result.last_tracked_progress;
        const auto current = vh::live_route_match_record_progress(direction, raw[i], valid[i], state, result);
        assert(current.has_value() == expected[i].has_value());
        if (current) {
            assert_near(*current, *expected[i]);
            assert_near(result.last_tracked_progress, *current);
            assert_near(*state.last_valid_progress, raw[i]);
            assert_near(*state.last_tracked_progress, *current);
        } else {
            assert(state.last_valid_progress == previous_state.last_valid_progress);
            assert(state.last_tracked_progress == previous_state.last_tracked_progress);
            assert(result.last_tracked_progress == previous_tracked);
        }
        assert(result.frames_captured == i + 1);
        assert(result.valid_matches == valid_counts[i]);
        assert_near(result.first_progress, 1.0);
        assert_near(result.last_progress, raw[i]);
        assert_near(result.min_progress_seen, i == 0 ? 1.0 : (i == 1 ? 0.2 : 0.0));
        assert_near(result.max_progress_seen, 1.0);
        assert_near(result.first_tracked_progress, i == 0 ? 0.0 : 0.2);
    }

    // Invalid extremes and repeated values must not count as raw regressions.
    assert(result.progress_regressions == 2);
    assert(result.reverse_progress_regressions == 1);
    assert_near(result.progress_rollback, 0.8);
    assert_near(result.reverse_progress_rollback, 0.6);
    assert(!result.progress_monotonic);
    assert(!result.reverse_progress_monotonic);
    assert(result.tracked_progress_regressions == tracked_regressions);
    assert(result.tracked_reverse_progress_regressions == tracked_reverse_regressions);
    assert_near(result.tracked_progress_rollback, tracked_rollback);
    assert_near(result.tracked_reverse_progress_rollback, tracked_reverse_rollback);
    assert(result.tracked_progress_monotonic == (tracked_rollback == 0.0));
    assert(result.tracked_reverse_progress_monotonic == (tracked_reverse_rollback == 0.0));
    assert_near(result.min_tracked_progress_seen, tracked_min);
    assert_near(result.max_tracked_progress_seen, tracked_max);
}

void check_deadband() {
    // The count uses a strict > 0.01 comparison. Smaller movement still contributes
    // to total rollback and clears monotonicity, even when the count remains zero.
    const auto boundary_raw = 0.01 / 0.35;
    const std::array<double, 3> samples{
        std::nextafter(boundary_raw, 0.0),
        boundary_raw,
        std::nextafter(boundary_raw, 1.0),
    };
    for (std::size_t i = 0; i < samples.size(); ++i) {
        vh::LiveRouteMatchProgressState state;
        vh::LiveRouteMatchingResult result;
        (void)vh::live_route_match_record_progress("any", 0.0, true, state, result);
        const auto current = vh::live_route_match_record_progress("any", samples[i], true, state, result);
        assert(current);
        assert(i == 0 ? *current < 0.01 : (i == 1 ? *current == 0.01 : *current > 0.01));
        assert(result.tracked_reverse_progress_regressions == (i == 2 ? 1U : 0U));
        assert_near(result.tracked_reverse_progress_rollback, *current);
        assert(!result.tracked_reverse_progress_monotonic);
        assert(result.tracked_progress_monotonic);
    }

    vh::LiveRouteMatchProgressState state;
    vh::LiveRouteMatchingResult result;
    (void)vh::live_route_match_record_progress("any", 0.1, true, state, result);
    const auto current = vh::live_route_match_record_progress("any", 0.08, true, state, result);
    assert_near(*current, 0.093);
    assert(result.tracked_progress_regressions == 0);
    assert_near(result.tracked_progress_rollback, 0.007);
    assert(!result.tracked_progress_monotonic);
    assert(result.tracked_reverse_progress_monotonic);
}

void check_endpoints_and_verification() {
    for (const std::string direction : {"forward", "reverse"}) {
        vh::LiveRouteMatchingConfig config;
        config.expected_progress = direction;
        config.endpoint_start_progress = 0.1;
        config.endpoint_end_progress = 0.9;
        const auto start = direction == "forward" ? 0.0 : 1.0;
        const auto end = 1.0 - start;
        vh::LiveRouteMatchProgressState state;
        vh::LiveRouteMatchingResult result;
        (void)vh::live_route_match_record_progress(direction, start, true, state, result);
        auto current = vh::live_route_match_record_progress(direction, end, true, state, result);
        assert(current);
        assert(vh::live_route_match_endpoint_reached(config, end));
        assert(!vh::live_route_match_endpoint_reached(config, *current));
        assert(!vh::live_route_match_endpoint_progress_passed(config, result));

        for (int i = 0; i < 30; ++i) {
            current = vh::live_route_match_record_progress(direction, end, true, state, result);
        }
        assert(current && vh::live_route_match_endpoint_reached(config, *current));
        assert(vh::live_route_match_endpoint_progress_passed(config, result));

        vh::RouteMatch match;
        match.valid = true;
        match.progress = end;
        const auto observation = vh::make_progress_only_live_route_verification_observation(
            match, current, {}, {}, {});
        assert(observation.tracked_route_progress == current);
        assert(!observation.local_pose);

        const auto retained = result.last_tracked_progress;
        match.valid = false;
        current = vh::live_route_match_record_progress(direction, end, false, state, result);
        assert(!current);
        assert(result.last_tracked_progress == retained);
        assert(!(current && vh::live_route_match_endpoint_reached(config, *current)));
        const auto invalid_observation = vh::make_progress_only_live_route_verification_observation(
            match, current, {}, {}, {});
        assert(!invalid_observation.tracked_route_progress);
        assert(!invalid_observation.local_pose);
    }
}

void check_smoothing_validation() {
    const std::array<double, 4> invalid{
        -0.1, 1.1, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
    };
    for (const auto value : invalid) {
        for (const bool invalid_previous : {false, true}) {
            bool rejected = false;
            try {
                (void)vh::live_route_match_next_tracked_progress(
                    "any", invalid_previous ? value : 0.5, invalid_previous ? 0.5 : value);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            assert(rejected);
        }
    }
    assert(vh::live_route_match_next_tracked_progress("any", 0.5, 0.5) == 0.5);
}

} // namespace

int main() {
    const auto none = std::nullopt;
    check_trace("forward", {none, 0.2, none, 0.26, 0.26, 0.26, 0.26, none},
                0, 0.0, 1, 0.06, 0.2, 0.26);
    check_trace("reverse", {none, 0.2, none, 0.2, 0.2, 0.14, 0.08, none},
                2, 0.12, 0, 0.0, 0.08, 0.2);
    check_trace("any", {none, 0.2, none, 0.26, 0.253, 0.193, 0.133, none},
                2, 0.127, 1, 0.06, 0.133, 0.26);
    check_deadband();
    check_endpoints_and_verification();
    check_smoothing_validation();
    return 0;
}
