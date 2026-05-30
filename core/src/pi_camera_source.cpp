#include "visual_homing/pi_camera_source.hpp"

#include <stdexcept>

namespace vh {

PiCameraSource::PiCameraSource(PiCameraConfig config)
    : config_(config) {
    if (config_.width <= 0 || config_.height <= 0) {
        throw std::invalid_argument("PiCameraSource dimensions must be positive");
    }
    if (config_.frame_rate_hz <= 0) {
        throw std::invalid_argument("PiCameraSource frame_rate_hz must be positive");
    }
    if (config_.format != PixelFormat::Gray8) {
        throw std::invalid_argument("PiCameraSource initial backend only accepts Gray8 output");
    }
}

bool PiCameraSource::start() {
#ifdef VISUAL_HOMING_ENABLE_LIBCAMERA
    last_error_ = "PiCameraSource libcamera backend is not implemented yet";
#else
    last_error_ = "PiCameraSource libcamera backend is not compiled in";
#endif
    running_ = false;
    return false;
}

void PiCameraSource::stop() {
    running_ = false;
}

std::optional<Frame> PiCameraSource::poll() {
    return std::nullopt;
}

bool PiCameraSource::running() const {
    return running_;
}

const PiCameraConfig& PiCameraSource::config() const {
    return config_;
}

const std::string& PiCameraSource::last_error() const {
    return last_error_;
}

} // namespace vh
