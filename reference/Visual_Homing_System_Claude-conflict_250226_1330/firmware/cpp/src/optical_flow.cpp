/**
 * MATEK 3901-L0X Optical Flow Sensor - C++ implementation
 */
#include "optical_flow.hpp"
#include <iostream>
#include <cstring>
#include <chrono>
#include <cmath>
#include <vector>

#ifdef __linux__
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace visual_homing {

OpticalFlowSensor::OpticalFlowSensor(const std::string& port, int baud)
    : port_(port), baud_(baud) {}

OpticalFlowSensor::~OpticalFlowSensor() {
    disconnect();
}

bool OpticalFlowSensor::connect() {
#ifdef __linux__
    fd_ = open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "OpticalFlow: Failed to open " << port_ << std::endl;
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd_, &tty) != 0) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    speed_t speed = B115200;
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10; // 1 second timeout

    tcsetattr(fd_, TCSANOW, &tty);

    connected_ = true;
    running_ = true;
    readThread_ = std::thread(&OpticalFlowSensor::readLoop, this);

    std::cout << "OpticalFlow: Connected on " << port_ << std::endl;
    return true;
#else
    std::cerr << "OpticalFlow: Linux only" << std::endl;
    return false;
#endif
}

void OpticalFlowSensor::disconnect() {
    running_ = false;
    if (readThread_.joinable()) readThread_.join();
#ifdef __linux__
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
#endif
    connected_ = false;
}

FlowData OpticalFlowSensor::getLatest() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return lastData_;
}

bool OpticalFlowSensor::isHealthy() const {
    return connected_.load();
}

void OpticalFlowSensor::readLoop() {
    std::vector<uint8_t> buffer;
    uint8_t tmp[256];

    while (running_) {
#ifdef __linux__
        int n = read(fd_, tmp, sizeof(tmp));
        if (n > 0) {
            buffer.insert(buffer.end(), tmp, tmp + n);
            parseBuffer(buffer);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
#endif
    }
}

void OpticalFlowSensor::parseBuffer(std::vector<uint8_t>& buffer) {
    while (buffer.size() >= 9) {
        // Find MSP V2 header: $X
        size_t pos = std::string::npos;
        for (size_t i = 0; i < buffer.size() - 1; i++) {
            if (buffer[i] == '$' && buffer[i + 1] == 'X') {
                pos = i;
                break;
            }
        }

        if (pos == std::string::npos) {
            buffer.clear();
            return;
        }

        if (pos > 0) buffer.erase(buffer.begin(), buffer.begin() + pos);
        if (buffer.size() < 9) return;

        uint16_t funcId = buffer[4] | (buffer[5] << 8);
        uint16_t payloadSize = buffer[6] | (buffer[7] << 8);
        size_t totalSize = 8 + payloadSize + 1;

        if (buffer.size() < totalSize) return;

        const uint8_t* payload = buffer.data() + 8;

        if (funcId == MSP_SENSOR_OPTICAL_FLOW && payloadSize >= 9) {
            parseOpticalFlow(payload, payloadSize);
        } else if (funcId == MSP_SENSOR_RANGEFINDER && payloadSize >= 5) {
            parseRangefinder(payload, payloadSize);
        }

        buffer.erase(buffer.begin(), buffer.begin() + totalSize);
    }
}

void OpticalFlowSensor::parseOpticalFlow(const uint8_t* payload, size_t len) {
    int quality = payload[0];
    int32_t flowX, flowY;
    memcpy(&flowX, payload + 1, 4);
    memcpy(&flowY, payload + 5, 4);

    std::lock_guard<std::mutex> lock(dataMutex_);
    lastData_.flow_x = (flowX / 10.0f) * 0.0174533f; // deg/s to rad/s
    lastData_.flow_y = (flowY / 10.0f) * 0.0174533f;
    lastData_.quality = quality;
}

void OpticalFlowSensor::parseRangefinder(const uint8_t* payload, size_t len) {
    int32_t distance;
    memcpy(&distance, payload + 1, 4);

    std::lock_guard<std::mutex> lock(dataMutex_);
    lastData_.distance_mm = distance;
}

} // namespace visual_homing
