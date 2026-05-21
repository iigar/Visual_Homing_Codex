/**
 * Visual Homing System - MAVLink Interface
 */

#ifndef MAVLINK_INTERFACE_HPP
#define MAVLINK_INTERFACE_HPP

#include "visual_odometry.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace visual_homing {

struct VehicleState {
    bool armed = false;
    std::string mode = "UNKNOWN";
    double altitude = 0.0;
    double heading = 0.0;
    double groundspeed = 0.0;
    double battery_voltage = 0.0;
    int battery_remaining = 100;
};

/**
 * MAVLink Interface for ArduPilot communication
 */
class MAVLinkInterface {
public:
    MAVLinkInterface(const std::string& port = "/dev/serial0",
                     int baudrate = 115200);
    ~MAVLinkInterface();

    // Connection
    bool connect(double timeout = 10.0);
    void disconnect();

    // Send visual navigation data
    void sendVisionPosition(double x, double y, double z,
                           double roll, double pitch, double yaw,
                           double confidence = 0.95);

    void sendVisionSpeed(double vx, double vy, double vz,
                        double confidence = 0.95);

    void sendVelocityCommand(double vx, double vy, double vz,
                            double yaw_rate = 0.0);

    // Status
    bool isConnected() const { return connected_; }
    const VehicleState& getVehicleState() const { return vehicle_state_; }
    double getAltitude() const { return vehicle_state_.altitude; }

private:
    void receiveLoop();
    void heartbeatLoop();

    std::string port_;
    int baudrate_;
    int serial_fd_ = -1;

    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};

    std::thread recv_thread_;
    std::thread heartbeat_thread_;

    VehicleState vehicle_state_;
};

} // namespace visual_homing

#endif // MAVLINK_INTERFACE_HPP
