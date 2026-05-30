#pragma once

#include <memory>
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
    ~PiCameraSource() override;

    bool start() override;
    void stop() override;
    std::optional<Frame> poll() override;

    bool running() const;
    const PiCameraConfig& config() const;
    const std::string& last_error() const;

private:
    struct Backend;

    PiCameraConfig config_;
    bool running_ = false;
    std::string last_error_;
    std::unique_ptr<Backend> backend_;
};

} // namespace vh
