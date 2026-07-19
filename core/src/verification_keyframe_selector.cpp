#include "visual_homing/verification_keyframe_selector.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vh {
namespace {

constexpr double pi = 3.14159265358979323846;

void require_config(bool condition, const char* message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

double wrapped_angle_difference(double lhs, double rhs) {
    auto delta = std::remainder(lhs - rhs, 2.0 * pi);
    return std::abs(delta);
}

double descriptor_novelty(
    const std::vector<std::int8_t>& current,
    const std::vector<std::int8_t>& reference) {
    if (current.size() != reference.size() || current.empty()) {
        throw std::runtime_error("Verification descriptor sizes must match");
    }
    std::uint64_t sum = 0;
    for (std::size_t index = 0; index < current.size(); ++index) {
        sum += static_cast<std::uint64_t>(std::abs(
            static_cast<int>(current[index]) - static_cast<int>(reference[index])));
    }
    return static_cast<double>(sum) / (static_cast<double>(current.size()) * 254.0);
}

double local_distance(
    const VerificationKeyframeObservation& lhs,
    const VerificationKeyframeObservation& rhs) {
    const auto dx = lhs.local_x_m - rhs.local_x_m;
    const auto dy = lhs.local_y_m - rhs.local_y_m;
    const auto dz = lhs.local_z_m - rhs.local_z_m;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool finite_observation(const VerificationKeyframeObservation& observation) {
    if (!std::isfinite(observation.route_progress)
        || !std::isfinite(observation.altitude_m)
        || !std::isfinite(observation.scale_ratio)
        || !std::isfinite(observation.yaw_rad)) {
        return false;
    }
    if (observation.has_local_pose) {
        return std::isfinite(observation.local_x_m)
            && std::isfinite(observation.local_y_m)
            && std::isfinite(observation.local_z_m)
            && std::isfinite(observation.local_yaw_rad)
            && std::isfinite(observation.local_position_uncertainty_m)
            && std::isfinite(observation.approach_radius_m);
    }
    return true;
}

} // namespace

struct VerificationKeyframeSelector::Impl {
    explicit Impl(VerificationKeyframeSelectorConfig selector_config)
        : config(std::move(selector_config)) {}

    VerificationKeyframeSelectorConfig config;
    std::optional<VerificationKeyframeObservation> last_keyframe;
    std::optional<VerificationKeyframeObservation> last_gate;
    std::uint64_t generation = 0;
    std::uint64_t keyframe_count = 0;
    std::uint64_t gate_count = 0;
};

VerificationKeyframeSelector::VerificationKeyframeSelector(
    VerificationKeyframeSelectorConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {
    const auto& value = impl_->config;
    require_config(value.descriptor_dimensions > 0, "Verification descriptor dimensions must be positive");
    require_config(std::isfinite(value.nominal_route_length_m) && value.nominal_route_length_m > 0.0,
        "Verification nominal route length must be finite and positive");
    require_config(value.minimum_capture_interval_ns > 0
        && value.maximum_capture_interval_ns >= value.minimum_capture_interval_ns,
        "Verification capture intervals are invalid");
    require_config(std::isfinite(value.minimum_displacement_m) && value.minimum_displacement_m > 0.0,
        "Verification displacement threshold must be finite and positive");
    require_config(std::isfinite(value.minimum_altitude_change_m) && value.minimum_altitude_change_m > 0.0,
        "Verification altitude threshold must be finite and positive");
    require_config(std::isfinite(value.minimum_scale_log_change) && value.minimum_scale_log_change > 0.0,
        "Verification scale threshold must be finite and positive");
    require_config(std::isfinite(value.minimum_yaw_change_rad)
        && value.minimum_yaw_change_rad > 0.0 && value.minimum_yaw_change_rad <= pi,
        "Verification yaw threshold must be within zero to pi");
    require_config(std::isfinite(value.minimum_scene_novelty)
        && value.minimum_scene_novelty > 0.0 && value.minimum_scene_novelty <= 1.0,
        "Verification novelty threshold must be within zero to one");
    require_config(std::isfinite(value.minimum_gate_separation_m) && value.minimum_gate_separation_m > 0.0,
        "Gate separation threshold must be finite and positive");
    require_config(std::isfinite(value.minimum_gate_scene_novelty)
        && value.minimum_gate_scene_novelty > 0.0 && value.minimum_gate_scene_novelty <= 1.0,
        "Gate novelty threshold must be within zero to one");
    require_config(std::isfinite(value.maximum_gate_position_uncertainty_m)
        && value.maximum_gate_position_uncertainty_m >= 0.0,
        "Gate uncertainty bound must be finite and non-negative");
    require_config(std::isfinite(value.minimum_gate_approach_margin_m)
        && value.minimum_gate_approach_margin_m > 0.0,
        "Gate approach margin must be finite and positive");
}

VerificationKeyframeSelector::~VerificationKeyframeSelector() = default;

VerificationKeyframeDecision VerificationKeyframeSelector::evaluate(
    const VerificationKeyframeObservation& observation) const {
    VerificationKeyframeDecision result;
    result.frame_id = observation.frame_id;
    result.timestamp_ns = observation.timestamp_ns;
    result.state_generation = impl_->generation;
    if (!observation.health_ready) {
        result.invalid_reason = "health_not_ready";
        return result;
    }
    if (!finite_observation(observation)
        || observation.route_progress < 0.0 || observation.route_progress > 1.0
        || observation.scale_ratio <= 0.0
        || observation.descriptor.size() != impl_->config.descriptor_dimensions) {
        result.invalid_reason = "invalid_observation";
        return result;
    }
    if (observation.has_local_pose
        && (observation.local_position_uncertainty_m < 0.0 || observation.approach_radius_m < 0.0)) {
        result.invalid_reason = "invalid_local_pose_quality";
        return result;
    }

    result.valid = true;
    if (!impl_->last_keyframe) {
        result.request_native_capture = true;
        result.trigger_mask = verification_trigger_initial;
    } else {
        const auto& previous = *impl_->last_keyframe;
        if (observation.frame_id <= previous.frame_id || observation.timestamp_ns <= previous.timestamp_ns) {
            result.valid = false;
            result.invalid_reason = "non_monotonic_observation";
            return result;
        }
        const auto elapsed_ns = observation.timestamp_ns - previous.timestamp_ns;
        result.elapsed_since_capture_s = static_cast<double>(elapsed_ns) / 1'000'000'000.0;
        if (observation.has_local_pose && previous.has_local_pose) {
            result.displacement_m = local_distance(observation, previous);
            result.displacement_uses_local_pose = true;
        } else {
            result.displacement_m = std::abs(observation.route_progress - previous.route_progress)
                * impl_->config.nominal_route_length_m;
        }
        result.altitude_change_m = std::abs(observation.altitude_m - previous.altitude_m);
        result.scale_log_change = std::abs(std::log(observation.scale_ratio / previous.scale_ratio));
        result.yaw_change_rad = wrapped_angle_difference(observation.yaw_rad, previous.yaw_rad);
        result.scene_novelty = descriptor_novelty(observation.descriptor, previous.descriptor);

        if (elapsed_ns >= impl_->config.minimum_capture_interval_ns) {
            if (result.displacement_m >= impl_->config.minimum_displacement_m) {
                result.trigger_mask |= verification_trigger_displacement;
            }
            if (result.altitude_change_m >= impl_->config.minimum_altitude_change_m) {
                result.trigger_mask |= verification_trigger_altitude;
            }
            if (result.scale_log_change >= impl_->config.minimum_scale_log_change) {
                result.trigger_mask |= verification_trigger_scale;
            }
            if (result.yaw_change_rad >= impl_->config.minimum_yaw_change_rad) {
                result.trigger_mask |= verification_trigger_yaw;
            }
            if (result.scene_novelty >= impl_->config.minimum_scene_novelty) {
                result.trigger_mask |= verification_trigger_scene_novelty;
            }
            if (elapsed_ns >= impl_->config.maximum_capture_interval_ns) {
                result.trigger_mask |= verification_trigger_maximum_interval;
            }
        }
        result.request_native_capture = result.trigger_mask != 0;
    }

    if (!result.request_native_capture) {
        result.gate_rejection_reason = "verification_capture_not_requested";
        return result;
    }
    if (!observation.has_local_pose) {
        result.gate_rejection_reason = "local_pose_unavailable";
        return result;
    }
    if (observation.local_position_uncertainty_m > impl_->config.maximum_gate_position_uncertainty_m) {
        result.gate_rejection_reason = "local_position_uncertainty_high";
        return result;
    }
    if (observation.approach_radius_m - observation.local_position_uncertainty_m
        < impl_->config.minimum_gate_approach_margin_m) {
        result.gate_rejection_reason = "approach_margin_small";
        return result;
    }
    if (!impl_->last_gate) {
        result.gate_candidate = true;
        return result;
    }
    const auto& previous_gate = *impl_->last_gate;
    result.gate_separation_m = observation.has_local_pose && previous_gate.has_local_pose
        ? local_distance(observation, previous_gate)
        : std::abs(observation.route_progress - previous_gate.route_progress)
            * impl_->config.nominal_route_length_m;
    result.gate_scene_novelty = descriptor_novelty(
        observation.descriptor,
        previous_gate.descriptor);
    if (result.gate_separation_m < impl_->config.minimum_gate_separation_m) {
        result.gate_rejection_reason = "gate_separation_small";
        return result;
    }
    if (result.gate_scene_novelty < impl_->config.minimum_gate_scene_novelty) {
        result.gate_rejection_reason = "gate_scene_novelty_low";
        return result;
    }
    result.gate_candidate = true;
    return result;
}

void VerificationKeyframeSelector::commit(
    const VerificationKeyframeObservation& observation,
    const VerificationKeyframeDecision& decision) {
    if (!decision.valid || !decision.request_native_capture
        || decision.frame_id != observation.frame_id
        || decision.timestamp_ns != observation.timestamp_ns
        || decision.state_generation != impl_->generation) {
        throw std::runtime_error("Verification keyframe commit decision is stale or invalid");
    }
    const auto expected = evaluate(observation);
    if (!expected.valid || !expected.request_native_capture
        || expected.trigger_mask != decision.trigger_mask
        || expected.gate_candidate != decision.gate_candidate) {
        throw std::runtime_error("Verification keyframe commit decision does not match current state");
    }
    impl_->last_keyframe = observation;
    ++impl_->keyframe_count;
    if (decision.gate_candidate) {
        impl_->last_gate = observation;
        ++impl_->gate_count;
    }
    ++impl_->generation;
}

void VerificationKeyframeSelector::reset() {
    impl_->last_keyframe.reset();
    impl_->last_gate.reset();
    impl_->keyframe_count = 0;
    impl_->gate_count = 0;
    ++impl_->generation;
}

std::uint64_t VerificationKeyframeSelector::committed_keyframes() const {
    return impl_->keyframe_count;
}

std::uint64_t VerificationKeyframeSelector::committed_gates() const {
    return impl_->gate_count;
}

std::string verification_trigger_mask_to_string(std::uint32_t mask) {
    if (mask == 0) {
        return "none";
    }
    struct Item { std::uint32_t bit; const char* name; };
    constexpr Item items[] = {
        {verification_trigger_initial, "initial"},
        {verification_trigger_displacement, "displacement"},
        {verification_trigger_altitude, "altitude"},
        {verification_trigger_scale, "scale"},
        {verification_trigger_yaw, "yaw"},
        {verification_trigger_scene_novelty, "scene_novelty"},
        {verification_trigger_maximum_interval, "maximum_interval"},
    };
    std::ostringstream output;
    bool first = true;
    for (const auto& item : items) {
        if ((mask & item.bit) != 0) {
            if (!first) {
                output << ',';
            }
            output << item.name;
            first = false;
        }
    }
    return output.str();
}

} // namespace vh
