#include <cassert>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/live_mavlink_serial_writer.hpp"

namespace {

class MemoryTransport final : public vh::LiveMavlinkByteTransport {
public:
    bool open_result = true;
    bool running_state = false;
    int opens = 0;
    int closes = 0;
    std::string reason = "memory transport unavailable";
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

vh::NavigationCommand command(double yaw_rate) {
    vh::NavigationCommand output;
    output.valid = true;
    output.vx_mps = 0.0;
    output.vy_mps = 0.0;
    output.yaw_rate_radps = yaw_rate;
    output.confidence = 0.99;
    return output;
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

float read_f32_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    const auto raw = read_u32_le(bytes, offset);
    float value = 0.0F;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

void assert_frame_shape(const std::vector<std::uint8_t>& frame,
                        std::uint8_t sequence,
                        float expected_yaw_rate) {
    assert(frame.size() == 65);
    assert(frame.at(0) == 0xFD);
    assert(frame.at(1) == 53);
    assert(frame.at(2) == 0);
    assert(frame.at(3) == 0);
    assert(frame.at(4) == sequence);
    assert(frame.at(5) == 191);
    assert(frame.at(6) == 1);
    assert(frame.at(7) == 84);
    assert(frame.at(8) == 0);
    assert(frame.at(9) == 0);

    constexpr std::size_t payload = 10;
    assert(read_u32_le(frame, payload) == 0);
    assert(read_f32_le(frame, payload + 16) == 0.0F);
    assert(read_f32_le(frame, payload + 20) == 0.0F);
    assert(read_f32_le(frame, payload + 24) == 0.0F);
    assert(std::abs(read_f32_le(frame, payload + 44) - expected_yaw_rate) < 0.00001F);
    assert(read_u16_le(frame, payload + 48) == 1535);
    assert(frame.at(payload + 50) == 1);
    assert(frame.at(payload + 51) == 1);
    assert(frame.at(payload + 52) == 8);
    assert(read_u16_le(frame, 63) != 0);
}

} // namespace

int main() {
    {
        const auto frame = vh::encode_mavlink2_set_position_target_local_ned_yaw_rate(
            command(0.125),
            7,
            191,
            1,
            1,
            1);
        assert_frame_shape(frame, 7, 0.125F);
    }

    {
        MemoryTransport transport;
        vh::LiveMavlinkSerialWriterConfig config;
        config.max_abs_yaw_rate_radps = 0.35;
        vh::LiveMavlinkSerialCommandWriter writer(config, transport);

        assert(!writer.running());
        assert(writer.start());
        assert(writer.running());
        assert(transport.opens == 1);
        assert(writer.start());
        assert(transport.opens == 1);

        writer.send(command(-0.25));
        writer.send(command(0.0));
        assert(transport.writes.size() == 2);
        assert_frame_shape(transport.writes.at(0), 0, -0.25F);
        assert_frame_shape(transport.writes.at(1), 1, 0.0F);

        writer.stop();
        assert(!writer.running());
        assert(transport.closes == 1);
    }

    {
        MemoryTransport transport;
        transport.open_result = false;
        vh::LiveMavlinkSerialWriterConfig config;
        vh::LiveMavlinkSerialCommandWriter writer(config, transport);
        assert(!writer.start());
        assert(!writer.running());
        assert(writer.unavailable_reason() == "memory transport unavailable");
    }

    {
        MemoryTransport transport;
        vh::LiveMavlinkSerialWriterConfig config;
        vh::LiveMavlinkSerialCommandWriter writer(config, transport);
        assert(writer.start());

        auto invalid = command(0.1);
        invalid.valid = false;
        bool rejected_invalid = false;
        try {
            writer.send(invalid);
        } catch (const std::runtime_error&) {
            rejected_invalid = true;
        }
        assert(rejected_invalid);
        assert(transport.writes.empty());

        auto nonzero_velocity = command(0.1);
        nonzero_velocity.vx_mps = 0.01;
        bool rejected_velocity = false;
        try {
            writer.send(nonzero_velocity);
        } catch (const std::runtime_error&) {
            rejected_velocity = true;
        }
        assert(rejected_velocity);
        assert(transport.writes.empty());

        bool rejected_bound = false;
        try {
            writer.send(command(0.36));
        } catch (const std::runtime_error&) {
            rejected_bound = true;
        }
        assert(rejected_bound);
        assert(transport.writes.empty());
    }

    return 0;
}
