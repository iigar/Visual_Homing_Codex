#include "visual_homing/route_local_odometry.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace vh {
namespace {

constexpr std::uint8_t mavlink2_stx = 0xFD;
constexpr std::uint32_t msg_odometry = 331;
constexpr std::uint8_t odometry_crc_extra = 91;
constexpr std::size_t odometry_payload_len = 232;
constexpr std::uint8_t mav_frame_body_frd = 12;
constexpr std::uint8_t mav_frame_local_frd = 20;
constexpr std::uint8_t mav_estimator_type_vision = 2;

void append_u16_le(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32_le(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void append_u64_le(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    append_u32_le(bytes, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
    append_u32_le(bytes, static_cast<std::uint32_t>((value >> 32U) & 0xFFFFFFFFULL));
}

void append_f32_le(std::vector<std::uint8_t>& bytes, float value) {
    std::uint32_t raw = 0;
    static_assert(sizeof(raw) == sizeof(value));
    std::memcpy(&raw, &value, sizeof(raw));
    append_u32_le(bytes, raw);
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

std::uint16_t mavlink_crc(const std::vector<std::uint8_t>& bytes,
                          std::size_t start,
                          std::size_t count,
                          std::uint8_t crc_extra) {
    std::uint16_t crc = 0xFFFFU;
    for (std::size_t index = start; index < start + count; ++index) {
        crc = crc_accumulate(bytes.at(index), crc);
    }
    return crc_accumulate(crc_extra, crc);
}

void validate_estimate(const RouteLocalOdometryEstimate& estimate) {
    if (!estimate.valid_for_fc) {
        throw std::runtime_error("Route-local ODOMETRY rejected estimate that is not FC-ready");
    }
    if (!std::isfinite(estimate.x_m)
        || !std::isfinite(estimate.y_m)
        || !std::isfinite(estimate.z_m)
        || !std::isfinite(estimate.yaw_rad)) {
        throw std::runtime_error("Route-local ODOMETRY rejected non-finite pose");
    }
}

} // namespace

std::vector<std::uint8_t> encode_mavlink2_route_local_odometry(
    const RouteLocalOdometryEstimate& estimate,
    std::uint64_t time_usec,
    std::uint8_t sequence,
    std::uint8_t source_system,
    std::uint8_t source_component) {
    validate_estimate(estimate);

    std::vector<std::uint8_t> payload;
    payload.reserve(odometry_payload_len);
    append_u64_le(payload, time_usec);
    append_f32_le(payload, static_cast<float>(estimate.x_m));
    append_f32_le(payload, static_cast<float>(estimate.y_m));
    append_f32_le(payload, static_cast<float>(estimate.z_m));

    const auto half_yaw = estimate.yaw_rad * 0.5;
    append_f32_le(payload, static_cast<float>(std::cos(half_yaw)));
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, static_cast<float>(std::sin(half_yaw)));

    const auto unknown = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t index = 0; index < 6; ++index) {
        append_f32_le(payload, unknown);
    }
    for (std::size_t index = 0; index < 21; ++index) {
        append_f32_le(payload, unknown);
    }
    for (std::size_t index = 0; index < 21; ++index) {
        append_f32_le(payload, unknown);
    }
    payload.push_back(mav_frame_local_frd);
    payload.push_back(mav_frame_body_frd);
    payload.push_back(estimate.reset_counter);
    payload.push_back(mav_estimator_type_vision);

    if (payload.size() != odometry_payload_len) {
        throw std::logic_error("ODOMETRY payload size mismatch");
    }

    std::vector<std::uint8_t> frame;
    frame.reserve(10 + payload.size() + 2);
    frame.push_back(mavlink2_stx);
    frame.push_back(static_cast<std::uint8_t>(payload.size()));
    frame.push_back(0);
    frame.push_back(0);
    frame.push_back(sequence);
    frame.push_back(source_system);
    frame.push_back(source_component);
    frame.push_back(static_cast<std::uint8_t>(msg_odometry & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((msg_odometry >> 8U) & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((msg_odometry >> 16U) & 0xFFU));
    frame.insert(frame.end(), payload.begin(), payload.end());
    const auto crc = mavlink_crc(frame, 1, 9 + payload.size(), odometry_crc_extra);
    append_u16_le(frame, crc);
    return frame;
}

} // namespace vh
