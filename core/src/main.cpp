#include <iostream>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "visual_homing/camera_profile.hpp"
#include "visual_homing/camera_smoke.hpp"
#include "visual_homing/pipeline_harness.hpp"
#include "visual_homing/replay_frame_source.hpp"
#include "visual_homing/route_artifact_check.hpp"
#include "visual_homing/route_signature.hpp"
#include "visual_homing/time.hpp"

namespace {

vh::CameraSmokeConfig camera_smoke_config_from_profile(const vh::CameraProfile& profile, int fps, std::size_t frames) {
    vh::CameraSmokeConfig config;
    config.camera.width = profile.capture_width;
    config.camera.height = profile.capture_height;
    config.camera.frame_rate_hz = fps;
    config.camera.enable_live_capture = true;
    config.target_width = profile.target_width;
    config.target_height = profile.target_height;
    config.frames_to_capture = frames;
    return config;
}

vh::LiveRouteRecordingConfig live_route_config_from_profile(const vh::CameraProfile& profile,
                                                            int fps,
                                                            std::size_t frames,
                                                            const std::string& output_path,
                                                            double altitude_m,
                                                            double heading_hint_rad,
                                                            std::size_t warmup_frames) {
    vh::LiveRouteRecordingConfig config;
    config.camera.width = profile.capture_width;
    config.camera.height = profile.capture_height;
    config.camera.frame_rate_hz = fps;
    config.camera.enable_live_capture = true;
    config.frames_to_capture = frames;
    config.route_output_path = output_path;
    config.target_width = profile.target_width;
    config.target_height = profile.target_height;
    config.warmup_frames = warmup_frames;
    config.altitude_m = altitude_m;
    config.heading_hint_rad = heading_hint_rad;
    return config;
}

void log_profile_hardware_config(const char* prefix, const vh::CameraProfileRecord& record, std::ostream& output) {
    output << prefix
           << " profile_path=" << record.path
           << " profile_id=" << record.profile.id
           << " capture=" << record.profile.capture_width << "x" << record.profile.capture_height
           << " target=" << record.profile.target_width << "x" << record.profile.target_height
           << " horizontal_fov_rad=" << record.profile.horizontal_fov_rad
           << " vertical_fov_rad=" << record.profile.vertical_fov_rad << "\n";
}

std::string join_distinctiveness_samples(const std::vector<vh::RouteDistinctivenessSample>& samples) {
    if (samples.empty()) {
        return "none";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (index != 0) {
            output << ",";
        }
        output << samples[index].frame_id << "@"
               << std::fixed << std::setprecision(3) << samples[index].route_time_ms
               << "ms";
    }
    return output.str();
}

std::string format_route_time_ms(double route_time_ms) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << route_time_ms;
    return output.str();
}

std::string wall_time_utc_iso8601() {
    const auto now = std::chrono::system_clock::now();
    const auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now_time);
#else
    gmtime_r(&now_time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

} // namespace

int main(int argc, char** argv) {
    const auto started = vh::now();
    const std::string command = argc > 1 ? argv[1] : "";
    const bool api_command = command.rfind("--api-", 0) == 0;
    if (!api_command) {
        std::cout << "Visual Homing Core boot wall_time_utc=" << wall_time_utc_iso8601() << "\n";
    }

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

    if ((argc == 8 || argc == 10 || argc == 14) && std::string(argv[1]) == "--match-route") {
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
            if (argc == 14) {
                config.max_direction_shift_px = std::stoi(argv[8]);
                vh::CameraProfile profile;
                profile.id = argv[9];
                profile.capture_width = std::stoi(argv[10]);
                profile.capture_height = std::stoi(argv[11]);
                profile.target_width = config.target_width;
                profile.target_height = config.target_height;
                profile.horizontal_fov_rad = std::stod(argv[12]);
                profile.vertical_fov_rad = std::stod(argv[13]);
                config.camera_profile = profile;
            }
            const auto result = vh::match_replay_route(config, std::cout);
            return result.frames_processed > 0 ? 0 : 1;
        } catch (const std::exception& error) {
            std::cerr << "match_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 10 && std::string(argv[1]) == "--match-route-profile") {
        try {
            vh::RouteMatchingConfig config;
            config.route_path = argv[2];
            config.manifest_path = argv[3];
            config.target_width = std::stoi(argv[4]);
            config.target_height = std::stoi(argv[5]);
            config.window_radius = static_cast<std::size_t>(std::stoull(argv[6]));
            config.minimum_confidence = std::stod(argv[7]);
            config.max_direction_shift_px = std::stoi(argv[8]);
            auto profile = vh::load_camera_profile_file(argv[9]);
            if (profile.target_width != config.target_width || profile.target_height != config.target_height) {
                throw std::invalid_argument("Camera profile target dimensions must match --match-route-profile target dimensions");
            }
            config.camera_profile = profile;
            const auto result = vh::match_replay_route(config, std::cout);
            return result.frames_processed > 0 ? 0 : 1;
        } catch (const std::exception& error) {
            std::cerr << "match_route_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 3 && std::string(argv[1]) == "--inspect-camera-profile") {
        try {
            const auto profile = vh::load_camera_profile_file(argv[2]);
            const auto scale = vh::derive_camera_angular_scale(profile);
            std::cout << "camera_profile path=" << argv[2]
                      << " id=" << profile.id
                      << " sensor_type=" << vh::to_string(profile.sensor_type)
                      << " pixel_format=" << vh::to_string(profile.pixel_format)
                      << " capture=" << profile.capture_width << "x" << profile.capture_height
                      << " target=" << profile.target_width << "x" << profile.target_height
                      << " horizontal_fov_rad=" << profile.horizontal_fov_rad
                      << " vertical_fov_rad=" << profile.vertical_fov_rad
                      << " radians_per_capture_pixel_x=" << scale.radians_per_capture_pixel_x
                      << " radians_per_capture_pixel_y=" << scale.radians_per_capture_pixel_y
                      << " radians_per_target_pixel_x=" << scale.radians_per_target_pixel_x
                      << " radians_per_target_pixel_y=" << scale.radians_per_target_pixel_y
                      << " maximum_low_texture_fraction=" << profile.maximum_low_texture_fraction
                      << " maximum_ambiguous_nearest_fraction=" << profile.maximum_ambiguous_nearest_fraction
                      << " minimum_average_nearest_mean_abs_diff=" << profile.minimum_average_nearest_mean_abs_diff
                      << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "inspect_camera_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 3 && std::string(argv[1]) == "--list-camera-profiles") {
        try {
            const auto records = vh::list_camera_profile_directory(argv[2]);
            std::cout << "camera_profiles directory=" << argv[2]
                      << " count=" << records.size() << "\n";
            for (const auto& record : records) {
                const auto scale = vh::derive_camera_angular_scale(record.profile);
                std::cout << "camera_profile path=" << record.path
                          << " id=" << record.profile.id
                          << " sensor_type=" << vh::to_string(record.profile.sensor_type)
                          << " pixel_format=" << vh::to_string(record.profile.pixel_format)
                          << " capture=" << record.profile.capture_width << "x" << record.profile.capture_height
                          << " target=" << record.profile.target_width << "x" << record.profile.target_height
                          << " horizontal_fov_rad=" << record.profile.horizontal_fov_rad
                          << " vertical_fov_rad=" << record.profile.vertical_fov_rad
                          << " radians_per_target_pixel_x=" << scale.radians_per_target_pixel_x
                          << "\n";
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "list_camera_profiles_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 4 && std::string(argv[1]) == "--get-active-camera-profile") {
        try {
            const auto record = vh::load_active_camera_profile(argv[2], argv[3]);
            std::cout << "active_camera_profile directory=" << argv[2]
                      << " state_path=" << argv[3]
                      << " path=" << record.path
                      << " id=" << record.profile.id
                      << " sensor_type=" << vh::to_string(record.profile.sensor_type)
                      << " pixel_format=" << vh::to_string(record.profile.pixel_format)
                      << " capture=" << record.profile.capture_width << "x" << record.profile.capture_height
                      << " target=" << record.profile.target_width << "x" << record.profile.target_height
                      << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "get_active_camera_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 5 && std::string(argv[1]) == "--set-active-camera-profile") {
        try {
            const auto record = vh::set_active_camera_profile(argv[2], argv[3], argv[4]);
            std::cout << "active_camera_profile_set directory=" << argv[2]
                      << " state_path=" << argv[3]
                      << " path=" << record.path
                      << " id=" << record.profile.id
                      << " sensor_type=" << vh::to_string(record.profile.sensor_type)
                      << " pixel_format=" << vh::to_string(record.profile.pixel_format)
                      << " capture=" << record.profile.capture_width << "x" << record.profile.capture_height
                      << " target=" << record.profile.target_width << "x" << record.profile.target_height
                      << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "set_active_camera_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 4 && std::string(argv[1]) == "--api-list-camera-profiles") {
        try {
            const auto records = vh::list_camera_profile_directory(argv[2]);
            std::string active_profile_id;
            try {
                active_profile_id = vh::load_active_camera_profile_id(argv[3]);
            } catch (const std::exception&) {
                active_profile_id.clear();
            }
            std::cout << vh::camera_profile_registry_json(records, active_profile_id) << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "api_list_camera_profiles_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 4 && std::string(argv[1]) == "--api-get-active-camera-profile") {
        try {
            const auto record = vh::load_active_camera_profile(argv[2], argv[3]);
            std::cout << vh::active_camera_profile_json(record) << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "api_get_active_camera_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 5 && std::string(argv[1]) == "--api-set-active-camera-profile") {
        try {
            const auto record = vh::set_active_camera_profile(argv[2], argv[3], argv[4]);
            std::cout << vh::active_camera_profile_json(record) << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "api_set_active_camera_profile_error=" << error.what() << "\n";
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

    if (argc == 5 && std::string(argv[1]) == "--pi-camera-smoke-profile") {
        try {
            vh::CameraProfileRecord record{
                .path = argv[2],
                .profile = vh::load_camera_profile_file(argv[2]),
            };
            log_profile_hardware_config("camera_smoke_profile", record, std::cout);
            const auto config = camera_smoke_config_from_profile(
                record.profile,
                std::stoi(argv[3]),
                static_cast<std::size_t>(std::stoull(argv[4])));
            const auto result = vh::run_pi_camera_smoke(config, std::cout);
            return result.started && result.frames_captured == config.frames_to_capture ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "pi_camera_smoke_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if (argc == 6 && std::string(argv[1]) == "--pi-camera-smoke-active-profile") {
        try {
            const auto record = vh::load_active_camera_profile(argv[2], argv[3]);
            log_profile_hardware_config("camera_smoke_active_profile", record, std::cout);
            const auto config = camera_smoke_config_from_profile(
                record.profile,
                std::stoi(argv[4]),
                static_cast<std::size_t>(std::stoull(argv[5])));
            const auto result = vh::run_pi_camera_smoke(config, std::cout);
            return result.started && result.frames_captured == config.frames_to_capture ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "pi_camera_smoke_active_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 10 || argc == 11 || argc == 12) && std::string(argv[1]) == "--record-live-route") {
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
            if (argc == 12) {
                config.heading_hint_rad = std::stod(argv[10]);
                config.warmup_frames = static_cast<std::size_t>(std::stoull(argv[11]));
            }
            const auto result = vh::record_live_camera_route(config, std::cout);
            return result.started && result.route_written && result.frames_captured == config.frames_to_capture ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "record_live_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 7 || argc == 8 || argc == 9) && std::string(argv[1]) == "--record-live-route-profile") {
        try {
            vh::CameraProfileRecord record{
                .path = argv[2],
                .profile = vh::load_camera_profile_file(argv[2]),
            };
            log_profile_hardware_config("live_route_profile", record, std::cout);
            const auto config = live_route_config_from_profile(
                record.profile,
                std::stoi(argv[3]),
                static_cast<std::size_t>(std::stoull(argv[4])),
                argv[5],
                std::stod(argv[6]),
                argc >= 8 ? std::stod(argv[7]) : 0.0,
                argc == 9 ? static_cast<std::size_t>(std::stoull(argv[8])) : 0);
            const auto result = vh::record_live_camera_route(config, std::cout);
            return result.started && result.route_written && result.frames_captured == config.frames_to_capture ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "record_live_route_profile_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 8 || argc == 9 || argc == 10) && std::string(argv[1]) == "--record-live-route-active-profile") {
        try {
            const auto record = vh::load_active_camera_profile(argv[2], argv[3]);
            log_profile_hardware_config("live_route_active_profile", record, std::cout);
            const auto config = live_route_config_from_profile(
                record.profile,
                std::stoi(argv[4]),
                static_cast<std::size_t>(std::stoull(argv[5])),
                argv[6],
                std::stod(argv[7]),
                argc >= 9 ? std::stod(argv[8]) : 0.0,
                argc == 10 ? static_cast<std::size_t>(std::stoull(argv[9])) : 0);
            const auto result = vh::record_live_camera_route(config, std::cout);
            return result.started && result.route_written && result.frames_captured == config.frames_to_capture ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "record_live_route_active_profile_error=" << error.what() << "\n";
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

    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--self-match-route") {
        try {
            vh::RouteSelfMatchConfig config;
            if (argc == 4) {
                config.minimum_confidence = std::stod(argv[3]);
            }
            const auto summary = vh::self_match_route_signature_file(argv[2], config);
            std::cout << "route_self_match path=" << argv[2]
                      << " entries_checked=" << summary.entries_checked
                      << " valid_matches=" << summary.valid_matches
                      << " exact_index_matches=" << summary.exact_index_matches
                      << " minimum_confidence_seen=" << summary.minimum_confidence_seen
                      << " average_confidence=" << summary.average_confidence
                      << " last_progress=" << summary.last_progress
                      << " progress_monotonic=" << (summary.progress_monotonic ? "true" : "false")
                      << " passed=" << (summary.passed ? "true" : "false") << "\n";
            return summary.passed ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "self_match_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--perturb-route") {
        try {
            vh::RoutePerturbationCheckConfig config;
            if (argc == 4) {
                config.minimum_confidence = std::stod(argv[3]);
            }
            const auto summary = vh::perturbation_check_route_signature_file(argv[2], config);
            std::cout << "route_perturb_check path=" << argv[2]
                      << " entries_checked=" << summary.entries_checked
                      << " brightness_valid_matches=" << summary.brightness_valid_matches
                      << " noise_valid_matches=" << summary.noise_valid_matches
                      << " shift_valid_matches=" << summary.shift_valid_matches
                      << " shift_direction_matches=" << summary.shift_direction_matches
                      << " minimum_brightness_confidence=" << summary.minimum_brightness_confidence
                      << " minimum_noise_confidence=" << summary.minimum_noise_confidence
                      << " minimum_shift_confidence=" << summary.minimum_shift_confidence
                      << " malformed_rejected=" << (summary.malformed_rejected ? "true" : "false")
                      << " passed=" << (summary.passed ? "true" : "false") << "\n";
            return summary.passed ? 0 : 2;
        } catch (const std::exception& error) {
            std::cerr << "perturb_route_error=" << error.what() << "\n";
            return 1;
        }
    }

    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--route-distinctiveness") {
        try {
            vh::RouteDistinctivenessConfig config;
            if (argc == 4) {
                config.edge_trim_entries = static_cast<std::uint64_t>(std::stoull(argv[3]));
            }
            const auto summary = vh::analyze_route_distinctiveness_file(argv[2], config);
            std::cout << "route_distinctiveness path=" << argv[2]
                      << " entries_checked=" << summary.entries_checked
                      << " entries_ignored_at_start=" << summary.entries_ignored_at_start
                      << " entries_ignored_at_end=" << summary.entries_ignored_at_end
                      << " first_evaluated_frame_id=" << summary.first_evaluated_frame_id
                      << " last_evaluated_frame_id=" << summary.last_evaluated_frame_id
                      << " first_evaluated_route_time_ms=" << format_route_time_ms(summary.first_evaluated_route_time_ms)
                      << " last_evaluated_route_time_ms=" << format_route_time_ms(summary.last_evaluated_route_time_ms)
                      << " adjacent_pairs_checked=" << summary.adjacent_pairs_checked
                      << " low_texture_entries=" << summary.low_texture_entries
                      << " exact_duplicate_entries=" << summary.exact_duplicate_entries
                      << " ambiguous_nearest_entries=" << summary.ambiguous_nearest_entries
                      << " low_texture_fraction=" << summary.low_texture_fraction
                      << " ambiguous_nearest_fraction=" << summary.ambiguous_nearest_fraction
                      << " minimum_payload_range=" << summary.minimum_payload_range
                      << " average_payload_range=" << summary.average_payload_range
                      << " minimum_adjacent_mean_abs_diff=" << summary.minimum_adjacent_mean_abs_diff
                      << " average_adjacent_mean_abs_diff=" << summary.average_adjacent_mean_abs_diff
                      << " minimum_nearest_mean_abs_diff=" << summary.minimum_nearest_mean_abs_diff
                      << " average_nearest_mean_abs_diff=" << summary.average_nearest_mean_abs_diff
                      << " low_texture_samples=" << join_distinctiveness_samples(summary.low_texture_samples)
                      << " exact_duplicate_samples=" << join_distinctiveness_samples(summary.exact_duplicate_samples)
                      << " ambiguous_nearest_samples=" << join_distinctiveness_samples(summary.ambiguous_nearest_samples)
                      << " warning=" << (summary.warning ? "true" : "false")
                      << " quality_pass=" << (summary.quality_pass ? "true" : "false") << "\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "route_distinctiveness_error=" << error.what() << "\n";
            return 1;
        }
    }

    std::cout << "Realtime C++ core skeleton ready\n";
    std::cout << "usage: visual_homing_core --replay <manifest.csv>\n";
    std::cout << "usage: visual_homing_core --pipeline <manifest.csv> <width> <height>\n";
    std::cout << "usage: visual_homing_core --record-route <manifest.csv> <route.vhrs> <width> <height> <altitude_m> <heading_hint_rad>\n";
    std::cout << "usage: visual_homing_core --match-route <route.vhrs> <manifest.csv> <width> <height> <window_radius> <minimum_confidence> [max_direction_shift_px radians_per_pixel]\n";
    std::cout << "usage: visual_homing_core --match-route <route.vhrs> <manifest.csv> <width> <height> <window_radius> <minimum_confidence> <max_direction_shift_px> <profile_id> <capture_width> <capture_height> <horizontal_fov_rad> <vertical_fov_rad>\n";
    std::cout << "usage: visual_homing_core --match-route-profile <route.vhrs> <manifest.csv> <width> <height> <window_radius> <minimum_confidence> <max_direction_shift_px> <camera.profile>\n";
    std::cout << "usage: visual_homing_core --inspect-camera-profile <camera.profile>\n";
    std::cout << "usage: visual_homing_core --list-camera-profiles <profile_dir>\n";
    std::cout << "usage: visual_homing_core --get-active-camera-profile <profile_dir> <active_profile_state>\n";
    std::cout << "usage: visual_homing_core --set-active-camera-profile <profile_dir> <active_profile_state> <profile_id>\n";
    std::cout << "usage: visual_homing_core --api-list-camera-profiles <profile_dir> <active_profile_state>\n";
    std::cout << "usage: visual_homing_core --api-get-active-camera-profile <profile_dir> <active_profile_state>\n";
    std::cout << "usage: visual_homing_core --api-set-active-camera-profile <profile_dir> <active_profile_state> <profile_id>\n";
    std::cout << "usage: visual_homing_core --pi-camera-smoke <width> <height> <fps> <frames> [target_width target_height]\n";
    std::cout << "usage: visual_homing_core --pi-camera-smoke-profile <camera.profile> <fps> <frames>\n";
    std::cout << "usage: visual_homing_core --pi-camera-smoke-active-profile <profile_dir> <active_profile_state> <fps> <frames>\n";
    std::cout << "usage: visual_homing_core --record-live-route <camera_width> <camera_height> <fps> <frames> <route.vhrs> <target_width> <target_height> <altitude_m> [heading_hint_rad [warmup_frames]]\n";
    std::cout << "usage: visual_homing_core --record-live-route-profile <camera.profile> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad [warmup_frames]]\n";
    std::cout << "usage: visual_homing_core --record-live-route-active-profile <profile_dir> <active_profile_state> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad [warmup_frames]]\n";
    std::cout << "usage: visual_homing_core --inspect-route <route.vhrs>\n";
    std::cout << "usage: visual_homing_core --self-match-route <route.vhrs> [minimum_confidence]\n";
    std::cout << "usage: visual_homing_core --perturb-route <route.vhrs> [minimum_confidence]\n";
    std::cout << "usage: visual_homing_core --route-distinctiveness <route.vhrs> [edge_trim_entries]\n";
    std::cout << "uptime_ms=" << vh::milliseconds_between(started, vh::now()) << "\n";
    return 0;
}
