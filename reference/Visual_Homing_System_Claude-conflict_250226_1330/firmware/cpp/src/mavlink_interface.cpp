/**
 * Visual Homing System - MAVLink Interface Implementation
 * Note: Simplified implementation - full version requires MAVLink headers
 */

#include "mavlink_interface.hpp"
#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>

namespace visual_homing {

MAVLinkInterface::MAVLinkInterface(const std::string& port, int baudrate)
    : port_(port)
    , baudrate_(baudrate)
{}

MAVLinkInterface::~MAVLinkInterface() {
    disconnect();
}

bool MAVLinkInterface::connect(double timeout) {
    std::cout << "Connecting to " << port_ << " at " << baudrate_ << std::endl;

    // Open serial port
    serial_fd_ = open(port_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ < 0) {
        std::cerr << "Failed to open serial port: " << port_ << std::endl;
        return false;
    }

    // Configure serial port
    struct termios options;
    tcgetattr(serial_fd_, &options);

    // Set baud rate
    speed_t baud;
    switch (baudrate_) {
        case 115200: baud = B115200; break;
        case 57600: baud = B57600; break;
        default: baud = B115200;
    }
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    // 8N1
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag |= (CLOCAL | CREAD);

    // Raw mode
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    tcsetattr(serial_fd_, TCSANOW, &options);

    connected_ = true;
    running_ = true;

    // Start threads
    recv_thread_ = std::thread(&MAVLinkInterface::receiveLoop, this);
    heartbeat_thread_ = std::thread(&MAVLinkInterface::heartbeatLoop, this);

    std::cout << "MAVLink connected" << std::endl;
    return true;
}

void MAVLinkInterface::disconnect() {
    running_ = false;
    connected_ = false;

    if (recv_thread_.joinable()) recv_thread_.join();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();

    if (serial_fd_ >= 0) {
        close(serial_fd_);
        serial_fd_ = -1;
    }

    std::cout << "MAVLink disconnected" << std::endl;
}

void MAVLinkInterface::receiveLoop() {
    uint8_t buffer[256];
    while (running_) {
        int n = read(serial_fd_, buffer, sizeof(buffer));
        if (n > 0) {
            // TODO: Parse MAVLink messages
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void MAVLinkInterface::heartbeatLoop() {
    while (running_) {
        // TODO: Send heartbeat
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void MAVLinkInterface::sendVisionPosition(double x, double y, double z,
                                          double roll, double pitch, double yaw,
                                          double confidence) {
    if (!connected_) return;
    // TODO: Send VISION_POSITION_ESTIMATE message
}

void MAVLinkInterface::sendVisionSpeed(double vx, double vy, double vz,
                                       double confidence) {
    if (!connected_) return;
    // TODO: Send VISION_SPEED_ESTIMATE message
}

void MAVLinkInterface::sendVelocityCommand(double vx, double vy, double vz,
                                           double yaw_rate) {
    if (!connected_) return;
    // TODO: Send SET_POSITION_TARGET_LOCAL_NED message
}

} // namespace visual_homing
