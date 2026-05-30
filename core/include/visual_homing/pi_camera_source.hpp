#pragma once

#include <string>

#include "visual_homing/interfaces.hpp"

namespace vh {

struct PiCameraConfig {
    int width = 320;
    int height = 240;
    int frame_rate_hz = 15;
    PixelFormat format = PixelFormat::Gray8;
};

class PiCameraSource final : public CameraSource {
public:
    explicit PiCameraSource(PiCameraConfig config = {});

    bool start() override;
    void stop() override;
    std::optional<Frame> poll() override;

    bool running() const;
    const PiCameraConfig& config() const;
    const std::string& last_error() const;

private:
    PiCameraConfig config_;
    bool running_ = false;
    std::string last_error_;
};

} // namespace vh
