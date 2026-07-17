#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include "visual_homing/route_local_odometry.hpp"

namespace {

vh::RouteLocalOdometryEstimate ready_estimate() {
    vh::RouteLocalOdometryEstimate estimate;
    estimate.x_m = 5.0;
    estimate.y_m = -0.25;
    estimate.z_m = 0.0;
    estimate.yaw_rad = -1.2;
    estimate.reset_counter = 9;
    estimate.valid_for_fc = true;
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

std::uint16_t crc_accumulate(std::uint8_t byte, std::uint16_t crc) {
    auto tmp = static_cast<std::uint8_t>(byte ^ static_cast<std::uint8_t>(crc & 0xFFU));
    tmp = static_cast<std::uint8_t>(tmp ^ static_cast<std::uint8_t>(tmp << 4U));
    return static_cast<std::uint16_t>(
        (crc >> 8U) ^
        (static_cast<std::uint16_t>(tmp) << 8U) ^
        (static_cast<std::uint16_t>(tmp) << 3U) ^
        (static_cast<std::uint16_t>(tmp) >> 4U));
}

std::uint16_t frame_crc(const std::vector<std::uint8_t>& frame) {
    std::uint16_t crc = 0xFFFFU;
    for (std::size_t index = 1; index < frame.size() - 2; ++index) {
        crc = crc_accumulate(frame.at(index), crc);
    }
    return crc_accumulate(91, crc);
}

} // namespace

int main() {
    {
        const auto frame = vh::encode_mavlink2_route_local_odometry(
            ready_estimate(),
            123456789ULL,
            7,
            191,
            1);

        assert(frame.size() == 244);
        assert(frame.at(0) == 0xFD);
        assert(frame.at(1) == 232);
        assert(frame.at(2) == 0);
        assert(frame.at(3) == 0);
        assert(frame.at(4) == 7);
        assert(frame.at(5) == 191);
        assert(frame.at(6) == 1);
        assert(frame.at(7) == 0x4B);
        assert(frame.at(8) == 0x01);
        assert(frame.at(9) == 0x00);

        constexpr std::size_t payload = 10;
        assert(read_u64_le(frame, payload) == 123456789ULL);
        assert(std::abs(read_f32_le(frame, payload + 8) - 5.0F) < 0.00001F);
        assert(std::abs(read_f32_le(frame, payload + 12) - -0.25F) < 0.00001F);
        assert(read_f32_le(frame, payload + 16) == 0.0F);
        assert(std::abs(read_f32_le(frame, payload + 20) - std::cos(-0.6F)) < 0.00001F);
        assert(read_f32_le(frame, payload + 24) == 0.0F);
        assert(read_f32_le(frame, payload + 28) == 0.0F);
        assert(std::abs(read_f32_le(frame, payload + 32) - std::sin(-0.6F)) < 0.00001F);

        for (std::size_t offset = 36; offset < 228; offset += 4) {
            assert(std::isnan(read_f32_le(frame, payload + offset)));
        }
        assert(frame.at(payload + 228) == 20);
        assert(frame.at(payload + 229) == 12);
        assert(frame.at(payload + 230) == 9);
        assert(frame.at(payload + 231) == 2);
        assert(read_u16_le(frame, frame.size() - 2) == frame_crc(frame));
    }

    {
        auto invalid = ready_estimate();
        invalid.valid_for_fc = false;
        bool rejected = false;
        try {
            (void)vh::encode_mavlink2_route_local_odometry(invalid, 1, 0, 191, 1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

    {
        auto non_finite = ready_estimate();
        non_finite.yaw_rad = std::numeric_limits<double>::quiet_NaN();
        bool rejected = false;
        try {
            (void)vh::encode_mavlink2_route_local_odometry(non_finite, 1, 0, 191, 1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

    return 0;
}
