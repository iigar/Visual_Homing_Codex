#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "visual_homing/live_mavlink_bridge.hpp"

namespace vh {

struct LiveMavlinkSerialWriterConfig {
    std::string device_path;
    int baud_rate = 115200;
    std::uint8_t source_system = 191;
    std::uint8_t source_component = 1;
    std::uint8_t target_system = 1;
    std::uint8_t target_component = 1;
    double max_abs_yaw_rate_radps = 0.35;
};

class LiveMavlinkByteTransport {
public:
    virtual ~LiveMavlinkByteTransport() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual void write_all(const std::vector<std::uint8_t>& bytes) = 0;
    virtual bool running() const = 0;
    virtual std::string unavailable_reason() const = 0;
};

class PosixSerialByteTransport final : public LiveMavlinkByteTransport {
public:
    explicit PosixSerialByteTransport(std::string device_path, int baud_rate);
    PosixSerialByteTransport(const PosixSerialByteTransport&) = delete;
    PosixSerialByteTransport& operator=(const PosixSerialByteTransport&) = delete;
    ~PosixSerialByteTransport() override;

    bool open() override;
    void close() override;
    void write_all(const std::vector<std::uint8_t>& bytes) override;
    bool running() const override;
    std::string unavailable_reason() const override;

private:
    std::string device_path_;
    int baud_rate_ = 115200;
    int fd_ = -1;
    std::string unavailable_reason_;
};

class LiveMavlinkSerialCommandWriter final : public LiveMavlinkCommandWriter {
public:
    LiveMavlinkSerialCommandWriter(LiveMavlinkSerialWriterConfig config,
                                   LiveMavlinkByteTransport& transport);

    bool start() override;
    void stop() override;
    void send(const NavigationCommand& command) override;
    bool running() const override;
    std::string unavailable_reason() const override;

private:
    std::vector<std::uint8_t> encode_set_position_target_local_ned(const NavigationCommand& command);
    void validate_command(const NavigationCommand& command) const;

    LiveMavlinkSerialWriterConfig config_;
    LiveMavlinkByteTransport* transport_ = nullptr;
    std::uint8_t sequence_ = 0;
    std::string unavailable_reason_;
};

std::vector<std::uint8_t> encode_mavlink2_set_position_target_local_ned_yaw_rate(
    const NavigationCommand& command,
    std::uint8_t sequence,
    std::uint8_t source_system,
    std::uint8_t source_component,
    std::uint8_t target_system,
    std::uint8_t target_component);

} // namespace vh
