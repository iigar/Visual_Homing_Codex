/**
 * Benewake TF-Luna LiDAR Sensor - C++ implementation
 */
#include "lidar.hpp"
#include <iostream>
#include <cstring>
#include <chrono>
#include <vector>

#ifdef __linux__
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace visual_homing {

LidarSensor::LidarSensor(const std::string& port, int baud)
    : port_(port), baud_(baud) {}

LidarSensor::~LidarSensor() {
    disconnect();
}

bool LidarSensor::connect() {
#ifdef __linux__
    fd_ = open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "LiDAR: Failed to open " << port_ << std::endl;
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(fd_, &tty);

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    tcsetattr(fd_, TCSANOW, &tty);

    connected_ = true;
    running_ = true;
    readThread_ = std::thread(&LidarSensor::readLoop, this);

    std::cout << "LiDAR: Connected on " << port_ << std::endl;
    return true;
#else
    return false;
#endif
}

void LidarSensor::disconnect() {
    running_ = false;
    if (readThread_.joinable()) readThread_.join();
#ifdef __linux__
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
#endif
    connected_ = false;
}

bool LidarSensor::setFrameRate(int fps) {
#ifdef __linux__
    if (fd_ < 0) return false;
    fps = std::max(1, std::min(250, fps));
    uint8_t cmd[6] = {
        0x5A, 0x06, 0x03,
        static_cast<uint8_t>(fps & 0xFF),
        static_cast<uint8_t>((fps >> 8) & 0xFF),
        0x00
    };
    cmd[5] = (cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4]) & 0xFF;
    return write(fd_, cmd, 6) == 6;
#else
    return false;
#endif
}

LidarData LidarSensor::getLatest() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return lastData_;
}

bool LidarSensor::isHealthy() const {
    return connected_.load();
}

void LidarSensor::readLoop() {
    std::vector<uint8_t> buffer;
    uint8_t tmp[128];

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

void LidarSensor::parseBuffer(std::vector<uint8_t>& buffer) {
    while (buffer.size() >= TF_FRAME_SIZE) {
        // Find header (two 0x59 bytes)
        bool found = false;
        for (size_t i = 0; i < buffer.size() - 1; i++) {
            if (buffer[i] == TF_HEADER && buffer[i + 1] == TF_HEADER) {
                if (i > 0) buffer.erase(buffer.begin(), buffer.begin() + i);
                found = true;
                break;
            }
        }

        if (!found) { buffer.clear(); return; }
        if (buffer.size() < TF_FRAME_SIZE) return;

        // Checksum
        uint8_t checksum = 0;
        for (int i = 0; i < 8; i++) checksum += buffer[i];
        if (checksum != buffer[8]) {
            buffer.erase(buffer.begin());
            continue;
        }

        // Parse
        int dist = buffer[2] | (buffer[3] << 8);
        int strength = buffer[4] | (buffer[5] << 8);
        int temp = buffer[6] | (buffer[7] << 8);

        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            lastData_.distance_cm = dist;
            lastData_.signal_strength = strength;
            lastData_.temperature_raw = temp;
        }

        buffer.erase(buffer.begin(), buffer.begin() + TF_FRAME_SIZE);
    }
}

} // namespace visual_homing
