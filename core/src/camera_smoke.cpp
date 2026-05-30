#include "visual_homing/camera_smoke.hpp"

#include <iostream>
#include <stdexcept>

#include "visual_homing/time.hpp"

namespace vh {

CameraSmokeResult run_pi_camera_smoke(const CameraSmokeConfig& config, std::ostream& metrics) {
    if (config.frames_to_capture == 0) {
        throw std::invalid_argument("Camera smoke frames_to_capture must be positive");
    }

    PiCameraSource source(config.camera);
    CameraSmokeResult result;

    metrics << "camera_smoke_start width=" << config.camera.width
            << " height=" << config.camera.height
            << " fps=" << config.camera.frame_rate_hz
            << " requested_frames=" << config.frames_to_capture << "\n";

    result.started = source.start();
    if (!result.started) {
        metrics << "camera_smoke_unavailable error=" << source.last_error() << "\n";
        metrics << "camera_smoke_done started=false frames_captured=0 empty_polls=0\n";
        return result;
    }

    const auto started_at = now();
    while (result.frames_captured < config.frames_to_capture) {
        if (auto frame = source.poll()) {
            ++result.frames_captured;
            metrics << "camera_frame id=" << frame->id
                    << " size=" << frame->width << "x" << frame->height
                    << " bytes=" << frame->data.size() << "\n";
        } else {
            ++result.empty_polls;
        }

        if (result.empty_polls > config.frames_to_capture * 10U) {
            break;
        }
    }

    source.stop();
    metrics << "camera_smoke_done started=true"
            << " frames_captured=" << result.frames_captured
            << " empty_polls=" << result.empty_polls
            << " elapsed_ms=" << milliseconds_between(started_at, now()) << "\n";
    return result;
}

} // namespace vh
