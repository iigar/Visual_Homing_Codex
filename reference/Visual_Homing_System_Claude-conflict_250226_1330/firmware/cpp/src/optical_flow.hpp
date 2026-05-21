/**
 * MATEK 3901-L0X Optical Flow Sensor - MSP V2 Protocol
 * C++ implementation
 */
#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>

namespace visual_homing {

struct FlowData {
    float flow_x = 0;       // rad/s
    float flow_y = 0;       // rad/s
    int quality = 0;         // 0-255
    int distance_mm = 0;     // ground distance from VL53L0X

    float distanceM() const { return distance_mm / 1000.0f; }
    bool isValid() const { return quality > 50 && distance_mm > 20; }
};

class OpticalFlowSensor {
public:
    explicit OpticalFlowSensor(const std::string& port = "/dev/serial1", int baud = 115200);
    ~OpticalFlowSensor();

    bool connect();
    void disconnect();

    FlowData getLatest() const;
    bool isConnected() const { return connected_; }
    bool isHealthy() const;

private:
    void readLoop();
    void parseBuffer(std::vector<uint8_t>& buffer);
    void parseOpticalFlow(const uint8_t* payload, size_t len);
    void parseRangefinder(const uint8_t* payload, size_t len);

    std::string port_;
    int baud_;
    int fd_ = -1;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread readThread_;
    mutable std::mutex dataMutex_;
    FlowData lastData_;

    static constexpr uint16_t MSP_SENSOR_RANGEFINDER = 0x1F01;
    static constexpr uint16_t MSP_SENSOR_OPTICAL_FLOW = 0x1F02;
};

} // namespace visual_homing
