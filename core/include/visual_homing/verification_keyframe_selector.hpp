#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vh {

constexpr std::uint32_t verification_trigger_initial = 1U << 0U;
constexpr std::uint32_t verification_trigger_displacement = 1U << 1U;
constexpr std::uint32_t verification_trigger_altitude = 1U << 2U;
constexpr std::uint32_t verification_trigger_scale = 1U << 3U;
constexpr std::uint32_t verification_trigger_yaw = 1U << 4U;
constexpr std::uint32_t verification_trigger_scene_novelty = 1U << 5U;
constexpr std::uint32_t verification_trigger_maximum_interval = 1U << 6U;

struct VerificationKeyframeSelectorConfig {
    std::uint32_t descriptor_dimensions = 160;
    double nominal_route_length_m = 1000.0;
    std::uint64_t minimum_capture_interval_ns = 1'000'000'000ULL;
    std::uint64_t maximum_capture_interval_ns = 10'000'000'000ULL;
    double minimum_displacement_m = 5.0;
    double minimum_altitude_change_m = 0.75;
    double minimum_scale_log_change = 0.22314355131420976;
    double minimum_yaw_change_rad = 0.3490658503988659;
    double minimum_scene_novelty = 0.12;
    double minimum_gate_separation_m = 20.0;
    double minimum_gate_scene_novelty = 0.18;
    double maximum_gate_position_uncertainty_m = 2.0;
    double minimum_gate_approach_margin_m = 0.5;
};

struct VerificationKeyframeObservation {
    std::uint64_t frame_id = 0;
    std::uint64_t timestamp_ns = 0;
    bool health_ready = false;
    double route_progress = 0.0;
    double altitude_m = 0.0;
    double scale_ratio = 1.0;
    double yaw_rad = 0.0;
    std::vector<std::int8_t> descriptor;
    bool has_local_pose = false;
    double local_x_m = 0.0;
    double local_y_m = 0.0;
    double local_z_m = 0.0;
    double local_yaw_rad = 0.0;
    double local_position_uncertainty_m = 0.0;
    double approach_radius_m = 0.0;
};

struct VerificationKeyframeDecision {
    bool valid = false;
    bool request_native_capture = false;
    bool gate_candidate = false;
    std::uint32_t trigger_mask = 0;
    std::uint64_t frame_id = 0;
    std::uint64_t timestamp_ns = 0;
    std::uint64_t state_generation = 0;
    double elapsed_since_capture_s = 0.0;
    double displacement_m = 0.0;
    bool displacement_uses_local_pose = false;
    double altitude_change_m = 0.0;
    double scale_log_change = 0.0;
    double yaw_change_rad = 0.0;
    double scene_novelty = 0.0;
    double gate_separation_m = 0.0;
    double gate_scene_novelty = 0.0;
    std::string invalid_reason;
    std::string gate_rejection_reason;
};

class VerificationKeyframeSelector {
public:
    explicit VerificationKeyframeSelector(VerificationKeyframeSelectorConfig config);
    ~VerificationKeyframeSelector();

    VerificationKeyframeSelector(const VerificationKeyframeSelector&) = delete;
    VerificationKeyframeSelector& operator=(const VerificationKeyframeSelector&) = delete;
    VerificationKeyframeSelector(VerificationKeyframeSelector&&) = delete;
    VerificationKeyframeSelector& operator=(VerificationKeyframeSelector&&) = delete;

    VerificationKeyframeDecision evaluate(const VerificationKeyframeObservation& observation) const;
    void commit(
        const VerificationKeyframeObservation& observation,
        const VerificationKeyframeDecision& decision);
    void reset();

    std::uint64_t committed_keyframes() const;
    std::uint64_t committed_gates() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::string verification_trigger_mask_to_string(std::uint32_t mask);

} // namespace vh
