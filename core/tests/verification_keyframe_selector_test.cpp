#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "visual_homing/verification_keyframe_selector.hpp"

namespace {

vh::VerificationKeyframeSelectorConfig config() {
    return {
        .descriptor_dimensions = 4,
        .nominal_route_length_m = 100.0,
        .minimum_capture_interval_ns = 100,
        .maximum_capture_interval_ns = 1'000,
        .minimum_displacement_m = 10.0,
        .minimum_altitude_change_m = 2.0,
        .minimum_scale_log_change = std::log(2.0),
        .minimum_yaw_change_rad = 0.5,
        .minimum_scene_novelty = 0.2,
        .minimum_gate_separation_m = 20.0,
        .minimum_gate_scene_novelty = 0.3,
        .maximum_gate_position_uncertainty_m = 1.0,
        .minimum_gate_approach_margin_m = 2.0,
    };
}

vh::VerificationKeyframeObservation observation(
    std::uint64_t frame_id,
    std::uint64_t timestamp_ns,
    double route_progress = 0.5,
    std::vector<std::int8_t> descriptor = {0, 0, 0, 0}) {
    return {
        .frame_id = frame_id,
        .timestamp_ns = timestamp_ns,
        .health_ready = true,
        .route_progress = route_progress,
        .descriptor = std::move(descriptor),
    };
}

void add_local_pose(
    vh::VerificationKeyframeObservation& value,
    double x_m,
    double uncertainty_m = 0.1,
    double approach_radius_m = 3.0) {
    value.has_local_pose = true;
    value.local_x_m = x_m;
    value.local_position_uncertainty_m = uncertainty_m;
    value.approach_radius_m = approach_radius_m;
}

vh::VerificationKeyframeDecision after_initial(
    const vh::VerificationKeyframeObservation& current,
    vh::VerificationKeyframeObservation initial = observation(1, 1'000)) {
    vh::VerificationKeyframeSelector selector(config());
    const auto initial_decision = selector.evaluate(initial);
    assert(initial_decision.valid && initial_decision.request_native_capture);
    selector.commit(initial, initial_decision);
    return selector.evaluate(current);
}

void assert_only_trigger(
    const vh::VerificationKeyframeDecision& decision,
    std::uint32_t expected) {
    assert(decision.valid);
    assert(decision.request_native_capture);
    assert(decision.trigger_mask == expected);
}

} // namespace

int main() {
    {
        auto bad = config();
        bad.descriptor_dimensions = 0;
        bool rejected = false;
        try {
            vh::VerificationKeyframeSelector selector(bad);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        vh::VerificationKeyframeSelector selector(config());
        auto first = observation(1, 1'000);
        first.health_ready = false;
        auto decision = selector.evaluate(first);
        assert(!decision.valid && decision.invalid_reason == "health_not_ready");

        first.health_ready = true;
        first.descriptor.pop_back();
        decision = selector.evaluate(first);
        assert(!decision.valid && decision.invalid_reason == "invalid_observation");

        first = observation(1, 1'000);
        first.route_progress = std::numeric_limits<double>::quiet_NaN();
        decision = selector.evaluate(first);
        assert(!decision.valid && decision.invalid_reason == "invalid_observation");

        first = observation(1, 1'000);
        add_local_pose(first, 0.0, -0.1);
        decision = selector.evaluate(first);
        assert(!decision.valid && decision.invalid_reason == "invalid_local_pose_quality");
    }
    {
        vh::VerificationKeyframeSelector selector(config());
        const auto first = observation(1, 1'000);
        const auto decision = selector.evaluate(first);
        const auto repeated = selector.evaluate(first);
        assert_only_trigger(decision, vh::verification_trigger_initial);
        assert(repeated.state_generation == decision.state_generation);
        assert(!decision.gate_candidate && decision.gate_rejection_reason == "local_pose_unavailable");
        assert(selector.committed_keyframes() == 0);
        selector.commit(first, decision);
        assert(selector.committed_keyframes() == 1 && selector.committed_gates() == 0);

        bool rejected_stale = false;
        try {
            selector.commit(first, decision);
        } catch (const std::runtime_error&) {
            rejected_stale = true;
        }
        assert(rejected_stale);

        auto non_monotonic = observation(1, 1'100);
        const auto invalid = selector.evaluate(non_monotonic);
        assert(!invalid.valid && invalid.invalid_reason == "non_monotonic_observation");
    }
    {
        auto current = observation(2, 1'099, 0.0, {127, 127, 127, 127});
        const auto decision = after_initial(current);
        assert(decision.valid && !decision.request_native_capture);
        assert(decision.trigger_mask == 0);
        assert(decision.gate_rejection_reason == "verification_capture_not_requested");
    }
    {
        auto current = observation(2, 1'100, 0.3);
        assert_only_trigger(after_initial(current), vh::verification_trigger_displacement);

        current = observation(2, 1'100);
        current.altitude_m = 2.0;
        assert_only_trigger(after_initial(current), vh::verification_trigger_altitude);

        current = observation(2, 1'100);
        current.scale_ratio = 2.0;
        assert_only_trigger(after_initial(current), vh::verification_trigger_scale);

        auto initial = observation(1, 1'000);
        initial.yaw_rad = 3.0;
        current = observation(2, 1'100);
        current.yaw_rad = -2.5;
        const auto yaw_decision = after_initial(current, initial);
        assert_only_trigger(yaw_decision, vh::verification_trigger_yaw);
        assert(yaw_decision.yaw_change_rad < 1.0);

        current = observation(2, 1'100, 0.5, {127, 127, 127, 127});
        assert_only_trigger(after_initial(current), vh::verification_trigger_scene_novelty);

        current = observation(2, 2'000);
        assert_only_trigger(after_initial(current), vh::verification_trigger_maximum_interval);
    }
    {
        auto initial = observation(1, 1'000);
        add_local_pose(initial, 10.0);
        auto current = observation(2, 1'100, 0.5);
        add_local_pose(current, 20.0);
        const auto decision = after_initial(current, initial);
        assert_only_trigger(decision, vh::verification_trigger_displacement);
        assert(decision.displacement_uses_local_pose);
        assert(decision.displacement_m == 10.0);
    }
    {
        vh::VerificationKeyframeSelector selector(config());
        auto first = observation(1, 1'000);
        add_local_pose(first, 0.0);
        const auto first_decision = selector.evaluate(first);
        assert(first_decision.gate_candidate);
        selector.commit(first, first_decision);
        assert(selector.committed_gates() == 1);

        auto close = observation(2, 1'100, 0.5, {127, 127, 127, 127});
        add_local_pose(close, 10.0);
        const auto close_decision = selector.evaluate(close);
        assert(close_decision.request_native_capture && !close_decision.gate_candidate);
        assert(close_decision.gate_rejection_reason == "gate_separation_small");
        selector.commit(close, close_decision);

        auto repeated_gate_scene = observation(3, 1'200);
        add_local_pose(repeated_gate_scene, 30.0);
        const auto repeated_gate_decision = selector.evaluate(repeated_gate_scene);
        assert(repeated_gate_decision.request_native_capture && !repeated_gate_decision.gate_candidate);
        assert(repeated_gate_decision.gate_separation_m == 30.0);
        assert(repeated_gate_decision.gate_scene_novelty == 0.0);
        assert(repeated_gate_decision.gate_rejection_reason == "gate_scene_novelty_low");
        selector.commit(repeated_gate_scene, repeated_gate_decision);

        auto distinct_gate = observation(4, 1'300, 0.5, {127, 127, 127, 127});
        add_local_pose(distinct_gate, 60.0);
        const auto distinct_gate_decision = selector.evaluate(distinct_gate);
        assert(distinct_gate_decision.scene_novelty > 0.4);
        assert(distinct_gate_decision.gate_scene_novelty > 0.4);
        assert(distinct_gate_decision.gate_candidate);
        selector.commit(distinct_gate, distinct_gate_decision);
        assert(selector.committed_keyframes() == 4 && selector.committed_gates() == 2);
    }
    {
        auto initial = observation(1, 1'000);
        add_local_pose(initial, 0.0);

        auto poor_uncertainty = observation(2, 1'100, 0.3);
        add_local_pose(poor_uncertainty, 20.0, 1.1, 4.0);
        auto decision = after_initial(poor_uncertainty, initial);
        assert(!decision.gate_candidate);
        assert(decision.gate_rejection_reason == "local_position_uncertainty_high");

        auto small_margin = observation(2, 1'100, 0.3);
        add_local_pose(small_margin, 20.0, 0.5, 2.4);
        decision = after_initial(small_margin, initial);
        assert(!decision.gate_candidate);
        assert(decision.gate_rejection_reason == "approach_margin_small");
    }
    {
        vh::VerificationKeyframeSelector selector(config());
        const auto first = observation(1, 1'000);
        const auto decision = selector.evaluate(first);
        auto forged = decision;
        forged.trigger_mask |= vh::verification_trigger_altitude;
        bool rejected = false;
        try {
            selector.commit(first, forged);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);

        selector.reset();
        assert(selector.committed_keyframes() == 0 && selector.committed_gates() == 0);
        rejected = false;
        try {
            selector.commit(first, decision);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        assert_only_trigger(selector.evaluate(first), vh::verification_trigger_initial);
    }
    {
        const auto mask = vh::verification_trigger_displacement
            | vh::verification_trigger_yaw
            | vh::verification_trigger_maximum_interval;
        assert(vh::verification_trigger_mask_to_string(0) == "none");
        assert(vh::verification_trigger_mask_to_string(mask)
            == "displacement,yaw,maximum_interval");
    }
    return 0;
}
