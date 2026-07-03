#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "visual_homing/external_nav_estimator.hpp"
#include "visual_homing/live_mavlink_serial_writer.hpp"

#ifndef VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BUILD_REQUESTED
#define VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BUILD_REQUESTED 0
#endif

#ifndef VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT_SCOPE
#define VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT_SCOPE 0
#endif

#ifndef VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE
#define VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE 0
#endif

#ifndef VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER_ATTACHED
#define VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER_ATTACHED 0
#endif

namespace vh {

struct LiveMavlinkExternalNavWriterConfig {
    std::string device_path;
    int baud_rate = 115200;
    std::uint8_t source_system = 191;
    std::uint8_t source_component = 1;
};

class ExternalNavWriter {
public:
    virtual ~ExternalNavWriter() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void send_vision_position_estimate(const ExternalNavEstimate& estimate, std::uint64_t time_usec) = 0;
    virtual bool running() const = 0;
    virtual std::string unavailable_reason() const = 0;
};

class LiveMavlinkExternalNavWriter final : public ExternalNavWriter {
public:
    LiveMavlinkExternalNavWriter(LiveMavlinkExternalNavWriterConfig config,
                                 LiveMavlinkByteTransport& transport);

    static constexpr bool external_nav_output_compiled_out() noexcept {
        return VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE == 0;
    }

    static constexpr bool external_nav_output_available() noexcept {
        return VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE == 1;
    }

    static constexpr bool build_requested() noexcept {
        return VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BUILD_REQUESTED == 1;
    }

    static constexpr bool bench_props_off_scope() noexcept {
        return VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT_SCOPE == 1;
    }

    static constexpr bool writer_attached() noexcept {
        return VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER_ATTACHED == 1;
    }

    bool start() override;
    void stop() override;
    void send_vision_position_estimate(const ExternalNavEstimate& estimate, std::uint64_t time_usec) override;
    bool running() const override;
    std::string unavailable_reason() const override;

private:
    void validate_estimate(const ExternalNavEstimate& estimate) const;

    LiveMavlinkExternalNavWriterConfig config_;
    LiveMavlinkByteTransport* transport_ = nullptr;
    std::uint8_t sequence_ = 0;
    std::string unavailable_reason_;
};

std::vector<std::uint8_t> encode_mavlink2_vision_position_estimate(
    const ExternalNavEstimate& estimate,
    std::uint64_t time_usec,
    std::uint8_t sequence,
    std::uint8_t source_system,
    std::uint8_t source_component);

} // namespace vh
