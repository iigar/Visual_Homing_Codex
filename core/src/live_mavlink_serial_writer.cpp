#include "visual_homing/live_mavlink_serial_writer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#if !defined(_WIN32)
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace vh {
namespace {

constexpr std::uint8_t mavlink2_stx = 0xFD;
constexpr std::uint32_t msg_set_position_target_local_ned = 84;
constexpr std::uint8_t set_position_target_local_ned_crc_extra = 143;
constexpr std::uint8_t mav_frame_body_ned = 8;
constexpr std::uint16_t type_mask_ignore_position = 0b0000000000000111;
constexpr std::uint16_t type_mask_ignore_velocity = 0b0000000000111000;
constexpr std::uint16_t type_mask_ignore_acceleration = 0b0000000111000000;
constexpr std::uint16_t type_mask_ignore_yaw = 0b0000010000000000;
constexpr std::uint16_t yaw_rate_only_type_mask =
    type_mask_ignore_position | type_mask_ignore_velocity | type_mask_ignore_acceleration | type_mask_ignore_yaw;

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

#if !defined(_WIN32)
speed_t baud_to_speed(int baud_rate) {
    switch (baud_rate) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 921600:
        return B921600;
    default:
        throw std::invalid_argument("Unsupported MAVLink command baud rate: " + std::to_string(baud_rate));
    }
}

std::string errno_message(const std::string& prefix) {
    return prefix + ": " + std::strerror(errno);
}

void configure_serial_write_port(int fd, int baud_rate) {
    termios options{};
    if (tcgetattr(fd, &options) != 0) {
        throw std::runtime_error(errno_message("Could not read command serial port attributes"));
    }

    cfmakeraw(&options);
    const auto speed = baud_to_speed(baud_rate);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    options.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);
    options.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    options.c_cflag &= static_cast<tcflag_t>(~PARENB);
    options.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    options.c_cflag |= CS8;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        throw std::runtime_error(errno_message("Could not configure command serial port attributes"));
    }
}
#endif

} // namespace

PosixSerialByteTransport::PosixSerialByteTransport(std::string device_path, int baud_rate)
    : device_path_(std::move(device_path)),
      baud_rate_(baud_rate) {
    if (device_path_.empty()) {
        throw std::invalid_argument("MAVLink command serial device path must not be empty");
    }
    if (baud_rate_ <= 0) {
        throw std::invalid_argument("MAVLink command serial baud rate must be positive");
    }
}

PosixSerialByteTransport::~PosixSerialByteTransport() {
    close();
}

bool PosixSerialByteTransport::open() {
    if (running()) {
        return true;
    }
    unavailable_reason_.clear();

#if defined(_WIN32)
    unavailable_reason_ = "MAVLink command serial output is only supported on POSIX serial devices";
    return false;
#else
    try {
        fd_ = ::open(device_path_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) {
            throw std::runtime_error(errno_message("Could not open MAVLink command device for write: " + device_path_));
        }
        configure_serial_write_port(fd_, baud_rate_);
        return true;
    } catch (const std::exception& error) {
        unavailable_reason_ = error.what();
        close();
        return false;
    }
#endif
}

void PosixSerialByteTransport::close() {
#if !defined(_WIN32)
    if (fd_ >= 0) {
        ::close(fd_);
    }
#endif
    fd_ = -1;
}

void PosixSerialByteTransport::write_all(const std::vector<std::uint8_t>& bytes) {
    if (!running()) {
        throw std::runtime_error("MAVLink command serial transport is not open");
    }
    if (bytes.empty()) {
        return;
    }

#if defined(_WIN32)
    throw std::runtime_error("MAVLink command serial output is only supported on POSIX serial devices");
#else
    std::size_t written_total = 0;
    while (written_total < bytes.size()) {
        const auto remaining = bytes.size() - written_total;
        const auto to_write = std::min<std::size_t>(
            remaining,
            static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
        const auto written = ::write(fd_, bytes.data() + written_total, to_write);
        if (written > 0) {
            written_total += static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        throw std::runtime_error(errno_message("Could not write MAVLink command bytes"));
    }
#endif
}

bool PosixSerialByteTransport::running() const {
    return fd_ >= 0;
}

std::string PosixSerialByteTransport::unavailable_reason() const {
    return unavailable_reason_;
}

LiveMavlinkSerialCommandWriter::LiveMavlinkSerialCommandWriter(LiveMavlinkSerialWriterConfig config,
                                                               LiveMavlinkByteTransport& transport)
    : config_(std::move(config)),
      transport_(&transport) {
    if (config_.baud_rate <= 0) {
        throw std::invalid_argument("MAVLink command writer baud rate must be positive");
    }
    if (!std::isfinite(config_.max_abs_yaw_rate_radps) || config_.max_abs_yaw_rate_radps <= 0.0) {
        throw std::invalid_argument("MAVLink command writer max yaw rate must be positive and finite");
    }
}

bool LiveMavlinkSerialCommandWriter::start() {
    if (running()) {
        return true;
    }
    if (transport_ == nullptr) {
        unavailable_reason_ = "MAVLink command writer has no byte transport";
        return false;
    }
    if (!transport_->open()) {
        unavailable_reason_ = transport_->unavailable_reason();
        if (unavailable_reason_.empty()) {
            unavailable_reason_ = "MAVLink command byte transport failed to open";
        }
        return false;
    }
    unavailable_reason_.clear();
    return true;
}

void LiveMavlinkSerialCommandWriter::stop() {
    if (transport_ != nullptr) {
        transport_->close();
    }
}

void LiveMavlinkSerialCommandWriter::send(const NavigationCommand& command) {
    if (!running() || transport_ == nullptr) {
        throw std::runtime_error("MAVLink command writer is not running");
    }
    validate_command(command);
    transport_->write_all(encode_set_position_target_local_ned(command));
    ++sequence_;
}

bool LiveMavlinkSerialCommandWriter::running() const {
    return transport_ != nullptr && transport_->running();
}

std::string LiveMavlinkSerialCommandWriter::unavailable_reason() const {
    if (!unavailable_reason_.empty()) {
        return unavailable_reason_;
    }
    if (transport_ != nullptr) {
        return transport_->unavailable_reason();
    }
    return "MAVLink command writer has no byte transport";
}

std::vector<std::uint8_t> LiveMavlinkSerialCommandWriter::encode_set_position_target_local_ned(
    const NavigationCommand& command) {
    return encode_mavlink2_set_position_target_local_ned_yaw_rate(
        command,
        sequence_,
        config_.source_system,
        config_.source_component,
        config_.target_system,
        config_.target_component);
}

void LiveMavlinkSerialCommandWriter::validate_command(const NavigationCommand& command) const {
    if (!command.valid) {
        throw std::runtime_error("MAVLink command writer rejected invalid navigation command");
    }
    if (!std::isfinite(command.vx_mps) ||
        !std::isfinite(command.vy_mps) ||
        !std::isfinite(command.yaw_rate_radps) ||
        !std::isfinite(command.confidence)) {
        throw std::runtime_error("MAVLink command writer rejected non-finite navigation command");
    }
    if (std::abs(command.vx_mps) > 0.0 || std::abs(command.vy_mps) > 0.0) {
        throw std::runtime_error("MAVLink command writer is yaw-rate-only and requires zero velocity");
    }
    if (std::abs(command.yaw_rate_radps) > config_.max_abs_yaw_rate_radps) {
        throw std::runtime_error("MAVLink command writer rejected yaw rate above configured bound");
    }
}

std::vector<std::uint8_t> encode_mavlink2_set_position_target_local_ned_yaw_rate(
    const NavigationCommand& command,
    std::uint8_t sequence,
    std::uint8_t source_system,
    std::uint8_t source_component,
    std::uint8_t target_system,
    std::uint8_t target_component) {
    std::vector<std::uint8_t> payload;
    payload.reserve(53);
    append_u32_le(payload, 0);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, 0.0F);
    append_f32_le(payload, static_cast<float>(command.yaw_rate_radps));
    append_u16_le(payload, yaw_rate_only_type_mask);
    payload.push_back(target_system);
    payload.push_back(target_component);
    payload.push_back(mav_frame_body_ned);

    if (payload.size() != 53) {
        throw std::logic_error("SET_POSITION_TARGET_LOCAL_NED payload size mismatch");
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
    frame.push_back(static_cast<std::uint8_t>(msg_set_position_target_local_ned & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((msg_set_position_target_local_ned >> 8U) & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((msg_set_position_target_local_ned >> 16U) & 0xFFU));
    frame.insert(frame.end(), payload.begin(), payload.end());
    const auto crc = mavlink_crc(frame, 1, 9 + payload.size(), set_position_target_local_ned_crc_extra);
    append_u16_le(frame, crc);
    return frame;
}

} // namespace vh
