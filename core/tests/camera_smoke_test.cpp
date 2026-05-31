#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

#include "visual_homing/camera_smoke.hpp"

int main() {
    std::ostringstream metrics;
    vh::CameraSmokeConfig config;
    config.camera.width = 160;
    config.camera.height = 120;
    config.camera.frame_rate_hz = 10;
    config.target_width = 16;
    config.target_height = 12;
    config.frames_to_capture = 3;

    const auto result = vh::run_pi_camera_smoke(config, metrics);
    assert(!result.started);
    assert(result.frames_captured == 0);
    assert(result.empty_polls == 0);

    const auto output = metrics.str();
    assert(output.find("camera_smoke_start width=160 height=120 fps=10 target=16x12 requested_frames=3") != std::string::npos);
    assert(output.find("camera_smoke_unavailable error=") != std::string::npos);
    assert(output.find("camera_smoke_done started=false frames_captured=0 empty_polls=0") != std::string::npos);

    bool rejected = false;
    try {
        vh::CameraSmokeConfig invalid;
        invalid.frames_to_capture = 0;
        std::ostringstream ignored;
        (void)vh::run_pi_camera_smoke(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    bool rejected_target = false;
    try {
        vh::CameraSmokeConfig invalid;
        invalid.target_width = 0;
        std::ostringstream ignored;
        (void)vh::run_pi_camera_smoke(invalid, ignored);
    } catch (const std::invalid_argument&) {
        rejected_target = true;
    }
    assert(rejected_target);

    return 0;
}
