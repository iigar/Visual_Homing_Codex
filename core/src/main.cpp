#include <iostream>
#include <cstdint>
#include <exception>
#include <string>

#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/pipeline_harness.hpp"
#include "visual_homing/replay_frame_source.hpp"
#include "visual_homing/route_signature.hpp"
#include "visual_homing/time.hpp"

int main(int argc, char** argv) {
    const auto started = vh::now();
    std::cout << "Visual Homing Core boot\n";

    if (argc == 3 && std::string(argv[1]) == "--replay") {
        try {
            auto replay = vh::ReplayFrameSource::load_manifest(argv[2]);
            replay.start();

            std::uint64_t frames = 0;
            while (const auto frame = replay.poll()) {
                ++frames;
                std::cout << "frame id=" << frame->id
                          << " size=" << frame->width << "x" << frame->height
                          << " bytes=" << frame->data.size() << "\n";
            }

            replay.stop();
            std::cout << "replay_frames=" << frames << "\n";
            std::cout << "uptime_ms=" << vh::milliseconds_between(started, vh::now()) << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "replay_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 5 && std::string(argv[1]) == "--pipeline") {
        try {
            vh::PipelineConfig config;
            config.manifest_path = argv[2];
            config.target_width = std::stoi(argv[3]);
            config.target_height = std::stoi(argv[4]);
            const auto result = vh::run_replay_pipeline(config, std::cout);
            return result.frames_processed > 0 ? 0 : 1;
        } catch (const std::exception& error) {
            std::cerr << "pipeline_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 8 && std::string(argv[1]) == "--record-route") {
        try {
            vh::RouteRecordingConfig config;
            config.manifest_path = argv[2];
            config.route_output_path = argv[3];
            config.target_width = std::stoi(argv[4]);
            config.target_height = std::stoi(argv[5]);
            config.altitude_m = std::stod(argv[6]);
            config.heading_hint_rad = std::stod(argv[7]);
            const auto result = vh::record_replay_route(config, std::cout);
            return result.frames_processed > 0 ? 0 : 1;
        } catch (const std::exception& error) {
            std::cerr << "record_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 8 || argc == 10) && std::string(argv[1]) == "--match-route") {
        try {
            vh::RouteMatchingConfig config;
            config.route_path = argv[2];
            config.manifest_path = argv[3];
            config.target_width = std::stoi(argv[4]);
            config.target_height = std::stoi(argv[5]);
            config.window_radius = static_cast<std::size_t>(std::stoull(argv[6]));
            config.minimum_confidence = std::stod(argv[7]);
            if (argc == 10) {
                config.max_direction_shift_px = std::stoi(argv[8]);
                config.radians_per_pixel = std::stod(argv[9]);
            }
            const auto result = vh::match_replay_route(config, std::cout);
            return result.frames_processed > 0 ? 0 : 1;
        } catch (const std::exception& error) {
            std::cerr << "match_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 6 || argc == 8) && std::string(argv[1]) == "--pi-camera-smoke") {
        try {
            vh::CameraSmokeConfig config;
            config.camera.width = std::stoi(argv[2]);
            config.camera.height = std::stoi(argv[3]);
            config.camera.frame_rate_hz = std::stoi(argv[4]);
            config.camera.enable_live_capture = true;
            config.frames_to_capture = static_cast<std::size_t>(std::stoull(argv[5]));
            if (argc == 8) {
                config.target_width = std::stoi(argv[6]);
                config.target_height = std::stoi(argv[7]);
            }
            const auto result = vh::run_pi_camera_smoke(config, std::cout);
            return result.started && result.frames_captured == config.frames_to_capture ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "pi_camera_smoke_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 10 || argc == 11) && std::string(argv[1]) == "--record-live-route") {
        try {
            vh::LiveRouteRecordingConfig config;
            config.camera.width = std::stoi(argv[2]);
            config.camera.height = std::stoi(argv[3]);
            config.camera.frame_rate_hz = std::stoi(argv[4]);
            config.camera.enable_live_capture = true;
            config.frames_to_capture = static_cast<std::size_t>(std::stoull(argv[5]));
            config.route_output_path = argv[6];
            config.target_width = std::stoi(argv[7]);
            config.target_height = std::stoi(argv[8]);
            config.altitude_m = std::stod(argv[9]);
            if (argc == 11) {
                config.heading_hint_rad = std::stod(argv[10]);
            }
            const auto result = vh::record_live_camera_route(config, std::cout);
            return result.started && result.route_written && result.frames_captured == config.frames_to_capture ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "record_live_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 3 && std::string(argv[1]) == "--inspect-route") {
        try {
            const auto summary = vh::inspect_route_signature_file(argv[2]);
            std::cout << "route_inspect path=" << argv[2]
                      << " version=" << summary.version
                      << " entries=" << summary.entry_count
                      << " first_frame_id=" << summary.first_frame_id
                      << " last_frame_id=" << summary.last_frame_id
                      << " first_timestamp_ns=" << summary.first_timestamp_ns
                      << " last_timestamp_ns=" << summary.last_timestamp_ns
                      << " size=" << summary.width << "x" << summary.height
                      << " min_payload_bytes=" << summary.min_payload_bytes
                      << " max_payload_bytes=" << summary.max_payload_bytes
                      << " total_payload_bytes=" << summary.total_payload_bytes
                      << " timestamps_monotonic=" << (summary.timestamps_monotonic ? "true" : "false")
                      << " uniform_dimensions=" << (summary.uniform_dimensions ? "true" : "false")
                      << " uniform_payload_size=" << (summary.uniform_payload_size ? "true" : "false")
                      << " all_gray8=" << (summary.all_gray8 ? "true" : "false") << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "inspect_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    std::cout << "Realtime C++ core skeleton ready\n";
    std::cout << "usage: visual_homing_core --replay <manifest.csv>\n";
    std::cout << "usage: visual_homing_core --pipeline <manifest.csv> <width> <height>\n";
    std::cout << "usage: visual_homing_core --record-route <manifest.csv> <route.vhrs> <width> <height> <altitude_m> <heading_hint_rad>\n";
    std::cout << "usage: visual_homing_core --match-route <route.vhrs> <manifest.csv> <width> <height> <window_radius> <minimum_confidence> [max_direction_shift_px radians_per_pixel]\n";
    std::cout << "usage: visual_homing_core --pi-camera-smoke <width> <height> <fps> <frames> [target_width target_height]\n";
    std::cout << "usage: visual_homing_core --record-live-route <camera_width> <camera_height> <fps> <frames> <route.vhrs> <target_width> <target_height> <altitude_m> [heading_hint_rad]\n";
    std::cout << "usage: visual_homing_core --inspect-route <route.vhrs>\n";
    std::cout << "uptime_ms=" << vh::milliseconds_between(started, vh::now()) << "\n";
    return 0;
}
