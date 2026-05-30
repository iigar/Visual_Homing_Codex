#pragma once

#include <optional>

#include "visual_homing/frame.hpp"
#include "visual_homing/health.hpp"
#include "visual_homing/mavlink.hpp"
#include "visual_homing/navigation.hpp"
#include "visual_homing/route.hpp"

namespace vh {

class CameraSource {
public:
    virtual ~CameraSource() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual std::optional<Frame> poll() = 0;
};

class FramePreprocessor {
public:
    virtual ~FramePreprocessor() = default;
    virtual Frame process(const Frame& input) = 0;
};

class RouteRecorder {
public:
    virtual ~RouteRecorder() = default;
    virtual void observe(const Frame& frame, const NavigationEstimate& nav) = 0;
};

class RouteMatcher {
public:
    virtual ~RouteMatcher() = default;
    virtual RouteMatch match(const Frame& frame) = 0;
};

class Navigator {
public:
    virtual ~Navigator() = default;
    virtual NavigationCommand update(const RouteMatch& match, const HealthSnapshot& health) = 0;
};

class MavlinkBridge {
public:
    virtual ~MavlinkBridge() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void send(const NavigationCommand& command) = 0;
};

class MavlinkTelemetrySource {
public:
    virtual ~MavlinkTelemetrySource() = default;
    virtual std::optional<MavlinkTelemetry> poll_telemetry() = 0;
};

} // namespace vh
