#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/live_mavlink_external_nav_writer.hpp"

namespace {

class MemoryTransport final : public vh::LiveMavlinkByteTransport {
public:
    bool open_result = true;
    bool running_state = false;
    int opens = 0;
    int closes = 0;
    std::string reason = "external-nav memory transport unavailable";
    std::vector<std::vector<std::uint8_t>> writes;

    bool open() override {
        ++opens;
        running_state = open_result;
        return open_result;
    }

    void close() override {
        ++closes;
        running_state = false;
    }

    void write_all(const std::vector<std::uint8_t>& bytes) override {
        if (!running_state) {
            throw std::runtime_error("memory transport write while closed");
        }
        writes.push_back(bytes);
    }

    bool running() const override {
        return running_state;
    }

    std::string unavailable_reason() const override {
        return reason;
    }
};

vh::ExternalNavEstimate ready_estimate() {
    vh::ExternalNavEstimate estimate;
    estimate.pose_frame = vh::LocalCoordinateFrame::local_ned;
    estimate.frame_alignment_known = true;
    estimate.altitude_origin_aligned = true;
    estimate.x_m = 1.25;
    estimate.y_m = 0.0;
    estimate.z_m = -0.82;
    estimate.yaw_rad = -1.5;
    estimate.telemetry_yaw_rad = -1.45;
    estimate.yaw_direction_error_rad = -0.05;
    estimate.yaw_source_independent = true;
    estimate.confidence = 0.91;
    estimate.route_progress = 0.125;
    estimate.route_index = 45;
    estimate.route_entries = 360;
    estimate.relative_altitude_seen = true;
    estimate.relative_altitude_m = 0.82;
    estimate.route_match_valid = true;
    estimate.telemetry_fresh = true;
    estimate.altitude_valid = true;
    estimate.scale_known = true;
    estimate.valid_for_fc = true;
    estimate.reason = "valid";
    estimate.source_tag = "visual_route_progress";
    return estimate;
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes.at(offset)) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes.at(offset + 1)) << 8U);
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes.at(offset)) |
           (static_cast<std::uint32_t>(bytes.at(offset + 1)) << 8U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 2)) << 16U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 3)) << 24U);
}

std::uint64_t read_u64_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint64_t>(read_u32_le(bytes, offset)) |
           (static_cast<std::uint64_t>(read_u32_le(bytes, offset + 4)) << 32U);
}

float read_f32_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    const auto raw = read_u32_le(bytes, offset);
    float value = 0.0F;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

void assert_frame_shape(const std::vector<std::uint8_t>& frame, std::uint8_t sequence) {
    assert(frame.size() == 129);
    assert(frame.at(0) == 0xFD);
    assert(frame.at(1) == 117);
    assert(frame.at(2) == 0);
    assert(frame.at(3) == 0);
    assert(frame.at(4) == sequence);
    assert(frame.at(5) == 191);
    assert(frame.at(6) == 1);
    assert(frame.at(7) == 102);
    assert(frame.at(8) == 0);
    assert(frame.at(9) == 0);

    constexpr std::size_t payload = 10;
    assert(read_u64_le(frame, payload) == 123456789ULL);
    assert(std::abs(read_f32_le(frame, payload + 8) - 1.25F) < 0.00001F);
    assert(read_f32_le(frame, payload + 12) == 0.0F);
    assert(std::abs(read_f32_le(frame, payload + 16) - -0.82F) < 0.00001F);
    assert(read_f32_le(frame, payload + 20) == 0.0F);
    assert(read_f32_le(frame, payload + 24) == 0.0F);
    assert(std::abs(read_f32_le(frame, payload + 28) - -1.5F) < 0.00001F);
    assert(std::isnan(read_f32_le(frame, payload + 32)));
    assert(frame.at(payload + 116) == 0);
    assert(read_u16_le(frame, 127) != 0);
}

} // namespace

int main() {
#if VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE
    static_assert(VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BLOCKED == 0);
    static_assert(vh::LiveMavlinkExternalNavWriter::external_nav_output_available());
    static_assert(!vh::LiveMavlinkExternalNavWriter::external_nav_output_compiled_out());
    static_assert(vh::LiveMavlinkExternalNavWriter::writer_attached());
#else
    static_assert(VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BLOCKED == 1);
    static_assert(VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE == 0);
    static_assert(!vh::LiveMavlinkExternalNavWriter::external_nav_output_available());
    static_assert(vh::LiveMavlinkExternalNavWriter::external_nav_output_compiled_out());
    static_assert(!vh::LiveMavlinkExternalNavWriter::writer_attached());
#endif
#if VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BUILD_REQUESTED
    static_assert(vh::LiveMavlinkExternalNavWriter::build_requested());
    static_assert(vh::LiveMavlinkExternalNavWriter::bench_props_off_scope());
#else
    static_assert(!vh::LiveMavlinkExternalNavWriter::build_requested());
    static_assert(!vh::LiveMavlinkExternalNavWriter::bench_props_off_scope());
#endif

    {
        const auto frame = vh::encode_mavlink2_vision_position_estimate(
            ready_estimate(),
            123456789ULL,
            7,
            191,
            1);
        assert_frame_shape(frame, 7);
    }

    {
        MemoryTransport transport;
        vh::LiveMavlinkExternalNavWriterConfig config;
        vh::LiveMavlinkExternalNavWriter writer(config, transport);

        assert(!writer.running());
        assert(writer.start());
        assert(writer.running());
        assert(transport.opens == 1);
        assert(writer.start());
        assert(transport.opens == 1);

        writer.send_vision_position_estimate(ready_estimate(), 123456789ULL);
        writer.send_vision_position_estimate(ready_estimate(), 123456789ULL);
        assert(transport.writes.size() == 2);
        assert_frame_shape(transport.writes.at(0), 0);
        assert_frame_shape(transport.writes.at(1), 1);

        writer.stop();
        assert(!writer.running());
        assert(transport.closes == 1);
    }

    {
        MemoryTransport transport;
        transport.open_result = false;
        vh::LiveMavlinkExternalNavWriterConfig config;
        vh::LiveMavlinkExternalNavWriter writer(config, transport);
        assert(!writer.start());
        assert(!writer.running());
        assert(writer.unavailable_reason() == "external-nav memory transport unavailable");
    }

    {
        MemoryTransport transport;
        vh::LiveMavlinkExternalNavWriterConfig config;
        vh::LiveMavlinkExternalNavWriter writer(config, transport);
        assert(writer.start());

        auto invalid = ready_estimate();
        invalid.valid_for_fc = false;
        invalid.reason = "scale_not_known";
        bool rejected_invalid = false;
        try {
            writer.send_vision_position_estimate(invalid, 123456789ULL);
        } catch (const std::runtime_error&) {
            rejected_invalid = true;
        }
        assert(rejected_invalid);
        assert(transport.writes.empty());

        auto no_altitude = ready_estimate();
        no_altitude.altitude_valid = false;
        bool rejected_altitude = false;
        try {
            writer.send_vision_position_estimate(no_altitude, 123456789ULL);
        } catch (const std::runtime_error&) {
            rejected_altitude = true;
        }
        assert(rejected_altitude);
        assert(transport.writes.empty());

        auto non_finite = ready_estimate();
        non_finite.x_m = std::numeric_limits<double>::quiet_NaN();
        bool rejected_non_finite = false;
        try {
            writer.send_vision_position_estimate(non_finite, 123456789ULL);
        } catch (const std::runtime_error&) {
            rejected_non_finite = true;
        }
        assert(rejected_non_finite);
        assert(transport.writes.empty());

        auto non_finite_direction = ready_estimate();
        non_finite_direction.yaw_direction_error_rad = std::numeric_limits<double>::quiet_NaN();
        bool rejected_non_finite_direction = false;
        try {
            writer.send_vision_position_estimate(non_finite_direction, 123456789ULL);
        } catch (const std::runtime_error&) {
            rejected_non_finite_direction = true;
        }
        assert(rejected_non_finite_direction);
        assert(transport.writes.empty());

        auto route_frame = ready_estimate();
        route_frame.pose_frame = vh::LocalCoordinateFrame::route_frd;
        bool rejected_route_frame = false;
        try {
            writer.send_vision_position_estimate(route_frame, 123456789ULL);
        } catch (const std::runtime_error&) {
            rejected_route_frame = true;
        }
        assert(rejected_route_frame);
        assert(transport.writes.empty());

        auto unknown_alignment = ready_estimate();
        unknown_alignment.frame_alignment_known = false;
        bool rejected_unknown_alignment = false;
        try {
            writer.send_vision_position_estimate(unknown_alignment, 123456789ULL);
        } catch (const std::runtime_error&) {
            rejected_unknown_alignment = true;
        }
        assert(rejected_unknown_alignment);
        assert(transport.writes.empty());

        auto feedback_yaw = ready_estimate();
        feedback_yaw.yaw_source_independent = false;
        bool rejected_feedback_yaw = false;
        try {
            writer.send_vision_position_estimate(feedback_yaw, 123456789ULL);
        } catch (const std::runtime_error&) {
            rejected_feedback_yaw = true;
        }
        assert(rejected_feedback_yaw);
        assert(transport.writes.empty());
    }

    {
        auto route_frame = ready_estimate();
        route_frame.pose_frame = vh::LocalCoordinateFrame::route_frd;
        bool rejected = false;
        try {
            (void)vh::encode_mavlink2_vision_position_estimate(route_frame, 123456789ULL, 0, 191, 1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

    {
        auto feedback_yaw = ready_estimate();
        feedback_yaw.yaw_source_independent = false;
        bool rejected = false;
        try {
            (void)vh::encode_mavlink2_vision_position_estimate(feedback_yaw, 123456789ULL, 0, 191, 1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

    return 0;
}
