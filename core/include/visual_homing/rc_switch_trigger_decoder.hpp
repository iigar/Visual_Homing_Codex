#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "visual_homing/time.hpp"

namespace vh {

enum class RcSwitchPosition {
    Unknown,
    Low,
    High,
};

const char* rc_switch_position_name(RcSwitchPosition position);

struct RcSwitchTriggerConfig {
    std::uint16_t minimum_valid_pwm = 800;
    std::uint16_t low_max_pwm = 1200;
    std::uint16_t high_min_pwm = 1800;
    std::uint16_t maximum_valid_pwm = 2200;
    double debounce_ms = 150.0;
    double cooldown_ms = 3000.0;
};

struct RcSwitchObservation {
    Timestamp timestamp{};
    std::uint16_t pwm = 0;
};

struct RcSwitchTriggerResult {
    bool sample_accepted = false;
    bool trigger_edge = false;
    bool low_armed = false;
    RcSwitchPosition stable_position = RcSwitchPosition::Unknown;
    std::string reason;
};

class RcSwitchTriggerDecoder {
public:
    explicit RcSwitchTriggerDecoder(RcSwitchTriggerConfig config = {});

    RcSwitchTriggerResult observe(const RcSwitchObservation& observation);

private:
    RcSwitchTriggerResult result(bool sample_accepted,
                                 bool trigger_edge,
                                 const std::string& reason) const;

    RcSwitchTriggerConfig config_;
    RcSwitchPosition stable_position_ = RcSwitchPosition::Unknown;
    RcSwitchPosition candidate_position_ = RcSwitchPosition::Unknown;
    std::optional<Timestamp> candidate_since_;
    std::optional<Timestamp> last_sample_timestamp_;
    std::optional<Timestamp> last_trigger_timestamp_;
    bool low_armed_ = false;
};

} // namespace vh
