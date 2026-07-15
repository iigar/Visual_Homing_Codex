#include "visual_homing/live_mavlink_external_nav_writer.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace vh {
namespace {

constexpr std::uint8_t mavlink2_stx = 0xFD;
constexpr std::uint32_t msg_vision_position_estimate = 102;
constexpr std::uint8_t vision_position_estimate_crc_extra = 158;
constexpr std::size_t vision_position_estimate_payload_len = 117;

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

bool finite_number(double value) {
    return std::isfinite(value);
}

void validate_vision_position_frame(const ExternalNavEstimate& estimate) {
    if (estimate.pose_frame != LocalCoordinateFrame::local_ned
        || !estimate.frame_alignment_known
        || !estimate.altitude_origin_aligned
        || !estimate.yaw_source_independent) {
        throw std::runtime_error(
            "VISION_POSITION_ESTIMATE requires explicit LOCAL_NED frame, aligned altitude origin, and independent yaw");
    }
}

} // namespace

LiveMavlinkExternalNavWriter::LiveMavlinkExternalNavWriter(
    LiveMavlinkExternalNavWriterConfig config,
    LiveMavlinkByteTransport& transport)
    : config_(std::move(config)),
      transport_(&transport) {
    if (config_.baud_rate <= 0) {
        throw std::invalid_argument("External-nav writer baud rate must be positive");
    }
}

bool LiveMavlinkExternalNavWriter::start() {
    if (running()) {
        return true;
    }
    if (transport_ == nullptr) {
        unavailable_reason_ = "External-nav writer has no byte transport";
        return false;
    }
    if (!transport_->open()) {
        unavailable_reason_ = transport_->unavailable_reason();
        if (unavailable_reason_.empty()) {
            unavailable_reason_ = "External-nav byte transport failed to open";
        }
        return false;
    }
    unavailable_reason_.clear();
    return true;
}

void LiveMavlinkExternalNavWriter::stop() {
    if (transport_ != nullptr) {
        transport_->close();
    }
}

void LiveMavlinkExternalNavWriter::send_vision_position_estimate(
    const ExternalNavEstimate& estimate,
    std::uint64_t time_usec) {
    if (!running() || transport_ == nullptr) {
        throw std::runtime_error("External-nav writer is not running");
    }
    validate_estimate(estimate);
    transport_->write_all(encode_mavlink2_vision_position_estimate(
        estimate,
        time_usec,
        sequence_,
        config_.source_system,
        config_.source_component));
    ++sequence_;
}

bool LiveMavlinkExternalNavWriter::running() const {
    return transport_ != nullptr && transport_->running();
}

std::string LiveMavlinkExternalNavWriter::unavailable_reason() const {
    if (!unavailable_reason_.empty()) {
        return unavailable_reason_;
    }
    if (transport_ != nullptr) {
        return transport_->unavailable_reason();
    }
    return "External-nav writer has no byte transport";
}

void LiveMavlinkExternalNavWriter::validate_estimate(const ExternalNavEstimate& estimate) const {
    validate_vision_position_frame(estimate);
    if (!estimate.valid_for_fc || estimate.reason != "valid") {
        throw std::runtime_error("External-nav writer rejected estimate that is not FC-ready");
    }
    if (!estimate.route_match_valid || !estimate.telemetry_fresh || !estimate.altitude_valid
        || !estimate.scale_known || !estimate.yaw_source_independent) {
        throw std::runtime_error("External-nav writer rejected estimate with failed readiness fields");
    }
    if (!finite_number(estimate.x_m) ||
        !finite_number(estimate.y_m) ||
        !finite_number(estimate.z_m) ||
        !finite_number(estimate.yaw_rad) ||
        !finite_number(estimate.confidence) ||
        !finite_number(estimate.route_progress) ||
        !finite_number(estimate.relative_altitude_m)) {
        throw std::runtime_error("External-nav writer rejected non-finite estimate");
    }
}

std::vector<std::uint8_t> encode_mavlink2_vision_position_estimate(
    const ExternalNavEstimate& estimate,
    std::uint64_t time_usec,
    std::uint8_t sequence,
    std::uint8_t source_system,
    std::uint8_t source_component) {
    validate_vision_position_frame(estimate);
    std::vector<std::uint8_t> payload;
    payload.reserve(vision_position_estimate_payload_len);
    append_u64_le(payload, time_usec);
    append_f32_le(payload, static_cast<float>(estimate.x_m));
    append_f32_le(payload, static_cast<float>(estimate.y_m));
    append_f32_le(payload, static_cast<float>(estimate.z_m));
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, static_cast<float>(estimate.yaw_rad));

    const auto unknown_covariance = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t index = 0; index < 21; ++index) {
        append_f32_le(payload, unknown_covariance);
    }
    payload.push_back(0);

    if (payload.size() != vision_position_estimate_payload_len) {
        throw std::logic_error("VISION_POSITION_ESTIMATE payload size mismatch");
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
    frame.push_back(static_cast<std::uint8_t>(msg_vision_position_estimate & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((msg_vision_position_estimate >> 8U) & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((msg_vision_position_estimate >> 16U) & 0xFFU));
    frame.insert(frame.end(), payload.begin(), payload.end());
    const auto crc = mavlink_crc(frame, 1, 9 + payload.size(), vision_position_estimate_crc_extra);
    append_u16_le(frame, crc);
    return frame;
}

} // namespace vh
