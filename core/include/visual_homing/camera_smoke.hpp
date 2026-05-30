#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>

#include "visual_homing/pi_camera_source.hpp"

namespace vh {

struct CameraSmokeConfig {
    PiCameraConfig camera;
    std::size_t frames_to_capture = 30;
};

struct CameraSmokeResult {
    bool started = false;
    std::uint64_t frames_captured = 0;
    std::uint64_t empty_polls = 0;
};

CameraSmokeResult run_pi_camera_smoke(const CameraSmokeConfig& config, std::ostream& metrics);

} // namespace vh
