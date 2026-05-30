#include "visual_homing/camera_smoke.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

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
    metrics << "camera_smoke_backend_start_result started=" << (result.started ? "true" : "false")
            << " running=" << (source.running() ? "true" : "false");
    if (!source.last_error().empty()) {
        metrics << " error=" << source.last_error();
    }
    metrics << "\n";

    if (!result.started) {
        metrics << "camera_smoke_unavailable error=" << source.last_error() << "\n";
        metrics << "camera_smoke_done started=false frames_captured=0 empty_polls=0\n";
        return result;
    }

    const auto started_at = now();
    const auto timeout_ms = 2000.0 + (static_cast<double>(config.frames_to_capture) * 1000.0
        / static_cast<double>(config.camera.frame_rate_hz));
    while (result.frames_captured < config.frames_to_capture) {
        if (auto frame = source.poll()) {
            ++result.frames_captured;
            metrics << "camera_frame id=" << frame->id
                    << " size=" << frame->width << "x" << frame->height
                    << " bytes=" << frame->data.size() << "\n";
        } else {
            ++result.empty_polls;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (milliseconds_between(started_at, now()) > timeout_ms) {
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
