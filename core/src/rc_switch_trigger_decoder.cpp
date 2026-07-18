#include "visual_homing/rc_switch_trigger_decoder.hpp"

#include <cmath>
#include <stdexcept>

namespace vh {
namespace {

bool finite_nonnegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

RcSwitchPosition classify_position(const RcSwitchTriggerConfig& config,
                                   std::uint16_t pwm) {
    if (pwm <= config.low_max_pwm) {
        return RcSwitchPosition::Low;
    }
    if (pwm >= config.high_min_pwm) {
        return RcSwitchPosition::High;
    }
    return RcSwitchPosition::Unknown;
}

const char* debounce_reason(RcSwitchPosition position) {
    return position == RcSwitchPosition::Low ? "debouncing_low" : "debouncing_high";
}

} // namespace

const char* rc_switch_position_name(RcSwitchPosition position) {
    switch (position) {
    case RcSwitchPosition::Low:
        return "low";
    case RcSwitchPosition::High:
        return "high";
    case RcSwitchPosition::Unknown:
        return "unknown";
    }
    return "unknown";
}

RcSwitchTriggerDecoder::RcSwitchTriggerDecoder(RcSwitchTriggerConfig config)
    : config_(config) {
    if (config_.minimum_valid_pwm > config_.low_max_pwm
        || config_.low_max_pwm >= config_.high_min_pwm
        || config_.high_min_pwm > config_.maximum_valid_pwm) {
        throw std::invalid_argument(
            "RC PWM thresholds must satisfy minimum <= low < high <= maximum");
    }
    if (!finite_nonnegative(config_.debounce_ms)) {
        throw std::invalid_argument("debounce_ms must be finite and non-negative");
    }
    if (!finite_nonnegative(config_.cooldown_ms)) {
        throw std::invalid_argument("cooldown_ms must be finite and non-negative");
    }
}

RcSwitchTriggerResult RcSwitchTriggerDecoder::result(
    bool sample_accepted,
    bool trigger_edge,
    const std::string& reason) const {
    return RcSwitchTriggerResult{
        sample_accepted,
        trigger_edge,
        low_armed_,
        stable_position_,
        reason,
    };
}

RcSwitchTriggerResult RcSwitchTriggerDecoder::observe(
    const RcSwitchObservation& observation) {
    if (observation.pwm < config_.minimum_valid_pwm
        || observation.pwm > config_.maximum_valid_pwm) {
        return result(false, false, "pwm_out_of_range");
    }
    if (last_sample_timestamp_.has_value()
        && observation.timestamp < *last_sample_timestamp_) {
        return result(false, false, "timestamp_moved_backwards");
    }
    last_sample_timestamp_ = observation.timestamp;

    const auto observed_position = classify_position(config_, observation.pwm);
    if (observed_position == RcSwitchPosition::Unknown) {
        candidate_position_ = RcSwitchPosition::Unknown;
        candidate_since_.reset();
        return result(true, false, "pwm_in_hysteresis_band");
    }

    if (observed_position == stable_position_) {
        candidate_position_ = RcSwitchPosition::Unknown;
        candidate_since_.reset();
        return result(
            true,
            false,
            observed_position == RcSwitchPosition::Low ? "steady_low" : "steady_high");
    }

    if (!candidate_since_.has_value() || candidate_position_ != observed_position) {
        candidate_position_ = observed_position;
        candidate_since_ = observation.timestamp;
    }

    const auto candidate_age_ms = milliseconds_between(*candidate_since_, observation.timestamp);
    if (candidate_age_ms < config_.debounce_ms) {
        return result(true, false, debounce_reason(observed_position));
    }

    stable_position_ = observed_position;
    candidate_position_ = RcSwitchPosition::Unknown;
    candidate_since_.reset();

    if (stable_position_ == RcSwitchPosition::Low) {
        low_armed_ = true;
        return result(true, false, "low_position_armed");
    }

    if (!low_armed_) {
        return result(true, false, "high_without_debounced_low");
    }
    low_armed_ = false;

    if (last_trigger_timestamp_.has_value()) {
        const auto cooldown_age_ms = milliseconds_between(
            *last_trigger_timestamp_, observation.timestamp);
        if (cooldown_age_ms < config_.cooldown_ms) {
            return result(true, false, "cooldown_active");
        }
    }

    last_trigger_timestamp_ = observation.timestamp;
    return result(true, true, "rising_edge");
}

} // namespace vh
