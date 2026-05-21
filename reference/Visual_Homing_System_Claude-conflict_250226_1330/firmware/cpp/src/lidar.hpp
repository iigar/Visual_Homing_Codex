/**
 * Benewake TF-Luna LiDAR Sensor
 * C++ implementation
 */
#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>

namespace visual_homing {

struct LidarData {
    int distance_cm = 0;
    int signal_strength = 0;
    int temperature_raw = 0;

    float distanceM() const { return distance_cm / 100.0f; }
    float temperatureC() const { return temperature_raw / 100.0f; }
    bool isValid() const { return signal_strength > 100 && distance_cm >= 20 && distance_cm <= 800; }
};

class LidarSensor {
public:
    explicit LidarSensor(const std::string& port = "/dev/serial2", int baud = 115200);
    ~LidarSensor();

    bool connect();
    void disconnect();
    bool setFrameRate(int fps);

    LidarData getLatest() const;
    bool isConnected() const { return connected_; }
    bool isHealthy() const;

private:
    void readLoop();
    void parseBuffer(std::vector<uint8_t>& buffer);

    std::string port_;
    int baud_;
    int fd_ = -1;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread readThread_;
    mutable std::mutex dataMutex_;
    LidarData lastData_;

    static constexpr uint8_t TF_HEADER = 0x59;
    static constexpr size_t TF_FRAME_SIZE = 9;
};

} // namespace visual_homing
