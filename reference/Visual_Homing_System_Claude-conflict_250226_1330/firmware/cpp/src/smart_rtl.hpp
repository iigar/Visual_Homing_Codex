/**
 * Smart RTL (Return-to-Launch) Controller
 * Hybrid navigation: IMU/Baro at high altitude + Optical Flow/Visual at low altitude
 */
#pragma once

#include <string>
#include <cmath>

namespace visual_homing {

enum class SmartRTLPhase {
    IDLE,
    HIGH_ALT,        // >50m: ArduPilot IMU/Baro
    DESCENT,         // Gradual descent after 50% return
    LOW_ALT,         // <50m: Optical Flow + Visual
    PRECISION_LAND,  // <5m: LiDAR-aided landing
    COMPLETED,
    ERROR
};

struct SmartRTLConfig {
    float highAltThreshold = 50.0f;   // meters
    float precisionLandAlt = 5.0f;    // meters
    float descentStartPct = 0.5f;     // 50% of return path
    float descentRate = 2.0f;         // m/s
    float highAltSpeed = 10.0f;       // m/s
    float lowAltSpeed = 3.0f;         // m/s
    float precisionSpeed = 0.5f;      // m/s
    int flowMinQuality = 50;
    float visualMinConfidence = 0.3f;
    float minAltitude = 0.3f;         // touchdown
};

struct VelocityCommand {
    float vx = 0, vy = 0, vz = 0;
    float yawRate = 0;
    bool autopilotControl = true;
    float targetAltitude = 0;
    float maxSpeed = 0;
};

class SmartRTL {
public:
    explicit SmartRTL(const SmartRTLConfig& config = SmartRTLConfig{})
        : config_(config) {}

    SmartRTLPhase initiate(float altitude, float homeDistance) {
        active_ = true;
        currentAlt_ = altitude;
        homeDistance_ = homeDistance;
        totalDistance_ = homeDistance;
        progress_ = 0;

        if (altitude > config_.highAltThreshold) {
            phase_ = SmartRTLPhase::HIGH_ALT;
            navSource_ = "imu_baro";
        } else {
            phase_ = SmartRTLPhase::LOW_ALT;
            navSource_ = "optical_flow";
        }
        return phase_;
    }

    void update(float altitude, float homeDistance, int flowQuality = 0, float visualConf = 0) {
        if (!active_) return;

        currentAlt_ = altitude;
        homeDistance_ = homeDistance;

        if (totalDistance_ > 0) {
            float covered = totalDistance_ - homeDistance;
            progress_ = std::min(1.0f, std::max(0.0f, covered / totalDistance_));
        }

        // Phase transitions
        switch (phase_) {
            case SmartRTLPhase::HIGH_ALT:
                if (progress_ >= config_.descentStartPct) {
                    phase_ = SmartRTLPhase::DESCENT;
                }
                break;

            case SmartRTLPhase::DESCENT: {
                float remaining = 1.0f - progress_;
                if (remaining > 0) {
                    targetAlt_ = config_.highAltThreshold * remaining / (1.0f - config_.descentStartPct);
                }
                if (altitude <= config_.highAltThreshold) {
                    phase_ = SmartRTLPhase::LOW_ALT;
                    navSource_ = flowQuality >= config_.flowMinQuality ? "optical_flow" : "visual";
                }
                break;
            }

            case SmartRTLPhase::LOW_ALT:
                if (flowQuality >= config_.flowMinQuality) {
                    navSource_ = visualConf >= config_.visualMinConfidence ? "optical_flow_visual" : "optical_flow";
                } else if (visualConf >= config_.visualMinConfidence) {
                    navSource_ = "visual_only";
                } else {
                    navSource_ = "imu_fallback";
                }
                if (altitude <= config_.precisionLandAlt && homeDistance < 10.0f) {
                    phase_ = SmartRTLPhase::PRECISION_LAND;
                    navSource_ = "optical_flow_lidar";
                }
                break;

            case SmartRTLPhase::PRECISION_LAND:
                if (altitude <= config_.minAltitude) {
                    phase_ = SmartRTLPhase::COMPLETED;
                    active_ = false;
                }
                break;

            default:
                break;
        }
    }

    VelocityCommand getVelocityCommand() const {
        VelocityCommand cmd;
        switch (phase_) {
            case SmartRTLPhase::HIGH_ALT:
                cmd.autopilotControl = true;
                break;
            case SmartRTLPhase::DESCENT:
                cmd.vz = config_.descentRate;
                cmd.autopilotControl = true;
                cmd.targetAltitude = targetAlt_;
                break;
            case SmartRTLPhase::LOW_ALT:
                cmd.vz = 0.5f;
                cmd.autopilotControl = false;
                cmd.maxSpeed = config_.lowAltSpeed;
                break;
            case SmartRTLPhase::PRECISION_LAND:
                cmd.vz = currentAlt_ > 1.0f ? 0.3f : 0.15f;
                cmd.autopilotControl = false;
                cmd.maxSpeed = config_.precisionSpeed;
                break;
            default:
                break;
        }
        return cmd;
    }

    void abort() { phase_ = SmartRTLPhase::ERROR; active_ = false; }

    bool isActive() const { return active_; }
    SmartRTLPhase phase() const { return phase_; }
    float altitude() const { return currentAlt_; }
    float distance() const { return homeDistance_; }
    float progress() const { return progress_; }
    const std::string& navSource() const { return navSource_; }

private:
    SmartRTLConfig config_;
    SmartRTLPhase phase_ = SmartRTLPhase::IDLE;
    bool active_ = false;
    float currentAlt_ = 0;
    float homeDistance_ = 0;
    float totalDistance_ = 0;
    float targetAlt_ = 0;
    float progress_ = 0;
    std::string navSource_ = "none";
};

} // namespace visual_homing
