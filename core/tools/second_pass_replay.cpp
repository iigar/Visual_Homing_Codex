// Offline-only structured adapter. See docs/SECOND_PASS_EXPORTER_UA.md.
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/pipeline_harness.hpp"

namespace {
std::int64_t integer(const std::string& text, std::int64_t minimum, std::int64_t maximum) {
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < minimum || value > maximum) {
        throw std::invalid_argument("Invalid integer argument: " + text);
    }
    return value;
}

double number(const std::string& text, double maximum = std::numeric_limits<double>::max()) {
    double value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) || value < 0 || value > maximum) {
        throw std::invalid_argument("Invalid numeric argument: " + text);
    }
    return value;
}

std::vector<vh::Timestamp> read_times(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open source clock file");
    std::vector<vh::Timestamp> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto ns = integer(line, 1, std::numeric_limits<std::int64_t>::max());
        const vh::Timestamp stamp(std::chrono::duration_cast<vh::Clock::duration>(std::chrono::nanoseconds(ns)));
        if (std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count() != ns
            || (!result.empty() && stamp <= result.back()) || result.size() >= 100000) {
            throw std::runtime_error("Invalid source clock sequence");
        }
        result.push_back(stamp);
    }
    if (!input.eof() || result.empty()) throw std::runtime_error("Incomplete source clock sequence");
    return result;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 16) {
        std::cerr << "usage: second_pass_replay ROUTE MANIFEST CLOCK WIDTH HEIGHT WINDOW CONFIDENCE SHIFT "
                     "RAD_PER_PIXEL NAV_CONFIDENCE NAV_AGE_MS NAV_GAIN NAV_RATE NAV_ACCEL NAV_SPEED\n";
        return 2;
    }
    try {
        vh::RouteMatchingConfig config;
        config.route_path = argv[1];
        config.manifest_path = argv[2];
        const auto times = read_times(argv[3]);
        config.target_width = static_cast<int>(integer(argv[4], 1, 4096));
        config.target_height = static_cast<int>(integer(argv[5], 1, 4096));
        if (static_cast<std::uint64_t>(config.target_width) * config.target_height > 64 * 1024 * 1024) {
            throw std::invalid_argument("Target image exceeds pixel limit");
        }
        config.window_radius = static_cast<std::size_t>(integer(argv[6], 0, 100000));
        config.minimum_confidence = number(argv[7], 1);
        config.max_direction_shift_px = static_cast<int>(integer(argv[8], 0, config.target_width - 1));
        config.radians_per_pixel = number(argv[9]);
        config.navigator_minimum_confidence = number(argv[10], 1);
        config.navigator_max_match_age_ms = number(argv[11]);
        config.navigator_yaw_gain = number(argv[12]);
        config.navigator_max_yaw_rate_radps = number(argv[13]);
        config.navigator_max_yaw_accel_radps2 = number(argv[14]);
        config.navigator_forward_speed_mps = number(argv[15]);
        config.dry_run_mavlink_armed = true;
        config.dry_run_mavlink_mode = vh::FlightMode::Guided;
        // All six processing phase times equal this source frame's timestamp.
        // This is deterministic zero-work simulation, not measured latency.
        std::size_t calls = 0, observations = 0;
        std::cout << "sequence,frame_id,timestamp_ns,observation,missing_reason,match_valid,reference_index,"
                     "endpoint_stop,navigation_command_valid,route_readiness,verification_accepted,published\n";
        const auto result = vh::match_replay_route(config, std::cerr, [&]() {
            const auto call = calls++;
            return times.at(call == 0 ? 0 : (call - 1) / 6);
        }, [&](const vh::ReplayMatchObservation& frame) {
            if (frame.sequence != observations || frame.timestamp != times.at(observations)) {
                throw std::runtime_error("Source observation/clock mismatch");
            }
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(frame.timestamp.time_since_epoch()).count();
            std::cout << frame.sequence << ',' << frame.frame_id << ',' << ns << ",observed,,"
                      << (frame.reference_index ? "true" : "false") << ',';
            if (frame.reference_index) std::cout << *frame.reference_index;
            std::cout << ",," << (frame.navigation_command_valid ? "true" : "false") << ",,,\n";
            if (!std::cout) throw std::runtime_error("Prediction stream write failed");
            ++observations;
        });
        std::cout.flush();
        if (!std::cout || observations != times.size() || result.frames_processed != observations
            || calls != 1 + 6 * observations) throw std::runtime_error("Incomplete replay export");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "second_pass_replay_error: " << error.what() << '\n';
        return 2;
    }
}
