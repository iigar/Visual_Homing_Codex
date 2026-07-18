#include <cassert>
#include <chrono>
#include <limits>
#include <stdexcept>

#include "visual_homing/rc_switch_trigger_decoder.hpp"

namespace {

vh::Timestamp at_milliseconds(std::int64_t milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(
        std::chrono::milliseconds(milliseconds)));
}

vh::RcSwitchTriggerResult observe(vh::RcSwitchTriggerDecoder& decoder,
                                  std::int64_t milliseconds,
                                  std::uint16_t pwm) {
    return decoder.observe(vh::RcSwitchObservation{at_milliseconds(milliseconds), pwm});
}

void establish_low(vh::RcSwitchTriggerDecoder& decoder,
                   std::int64_t start_ms) {
    auto result = observe(decoder, start_ms, 999);
    assert(result.sample_accepted);
    assert(!result.trigger_edge);
    assert(result.reason == "debouncing_low");
    result = observe(decoder, start_ms + 150, 999);
    assert(result.sample_accepted);
    assert(!result.trigger_edge);
    assert(result.low_armed);
    assert(result.stable_position == vh::RcSwitchPosition::Low);
    assert(result.reason == "low_position_armed");
}

vh::RcSwitchTriggerResult transition_high(vh::RcSwitchTriggerDecoder& decoder,
                                           std::int64_t start_ms) {
    auto result = observe(decoder, start_ms, 2000);
    assert(result.sample_accepted);
    assert(!result.trigger_edge);
    assert(result.reason == "debouncing_high");
    return observe(decoder, start_ms + 150, 2000);
}

} // namespace

int main() {
    {
        vh::RcSwitchTriggerDecoder decoder;
        auto result = observe(decoder, 0, 2000);
        assert(!result.trigger_edge);
        assert(result.reason == "debouncing_high");
        result = observe(decoder, 150, 2000);
        assert(!result.trigger_edge);
        assert(!result.low_armed);
        assert(result.stable_position == vh::RcSwitchPosition::High);
        assert(result.reason == "high_without_debounced_low");
        result = observe(decoder, 500, 2000);
        assert(!result.trigger_edge);
        assert(result.reason == "steady_high");
    }
    {
        vh::RcSwitchTriggerDecoder decoder;
        establish_low(decoder, 0);
        const auto result = transition_high(decoder, 300);
        assert(result.trigger_edge);
        assert(!result.low_armed);
        assert(result.stable_position == vh::RcSwitchPosition::High);
        assert(result.reason == "rising_edge");
        const auto held = observe(decoder, 1000, 2000);
        assert(!held.trigger_edge);
        assert(held.reason == "steady_high");
    }
    {
        vh::RcSwitchTriggerDecoder decoder;
        establish_low(decoder, 0);
        assert(transition_high(decoder, 300).trigger_edge);

        establish_low(decoder, 700);
        const auto blocked = transition_high(decoder, 1000);
        assert(!blocked.trigger_edge);
        assert(!blocked.low_armed);
        assert(blocked.reason == "cooldown_active");

        establish_low(decoder, 3600);
        const auto allowed = transition_high(decoder, 3900);
        assert(allowed.trigger_edge);
        assert(allowed.reason == "rising_edge");
    }
    {
        vh::RcSwitchTriggerDecoder decoder;
        auto result = observe(decoder, 0, 999);
        assert(result.reason == "debouncing_low");
        result = observe(decoder, 100, 1500);
        assert(result.reason == "pwm_in_hysteresis_band");
        result = observe(decoder, 200, 999);
        assert(result.reason == "debouncing_low");
        result = observe(decoder, 349, 999);
        assert(result.reason == "debouncing_low");
        result = observe(decoder, 350, 999);
        assert(result.reason == "low_position_armed");
    }
    {
        vh::RcSwitchTriggerDecoder decoder;
        auto result = observe(decoder, 100, 700);
        assert(!result.sample_accepted);
        assert(result.reason == "pwm_out_of_range");
        result = observe(decoder, 200, 999);
        assert(result.sample_accepted);
        result = observe(decoder, 199, 999);
        assert(!result.sample_accepted);
        assert(result.reason == "timestamp_moved_backwards");
    }
    {
        vh::RcSwitchTriggerConfig config;
        config.debounce_ms = 0.0;
        config.cooldown_ms = 0.0;
        vh::RcSwitchTriggerDecoder decoder(config);
        auto result = observe(decoder, 0, 999);
        assert(result.low_armed);
        result = observe(decoder, 1, 2000);
        assert(result.trigger_edge);
    }
    {
        bool rejected = false;
        try {
            vh::RcSwitchTriggerConfig config;
            config.low_max_pwm = config.high_min_pwm;
            const vh::RcSwitchTriggerDecoder decoder(config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        bool rejected = false;
        try {
            vh::RcSwitchTriggerConfig config;
            config.cooldown_ms = std::numeric_limits<double>::quiet_NaN();
            const vh::RcSwitchTriggerDecoder decoder(config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    assert(std::string(vh::rc_switch_position_name(vh::RcSwitchPosition::Unknown)) == "unknown");
    assert(std::string(vh::rc_switch_position_name(vh::RcSwitchPosition::Low)) == "low");
    assert(std::string(vh::rc_switch_position_name(vh::RcSwitchPosition::High)) == "high");
    return 0;
}
