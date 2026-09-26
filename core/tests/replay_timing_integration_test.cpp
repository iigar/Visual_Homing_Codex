#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "visual_homing/pipeline_harness.hpp"
#include "visual_homing/route_signature.hpp"

namespace {
using namespace std::chrono_literals;

vh::Timestamp at_ms(int value) { return vh::Timestamp{} + std::chrono::milliseconds(value); }

struct Fixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path()
        / ("visual_homing_replay_timing_" + std::to_string(vh::Clock::now().time_since_epoch().count()));
    vh::RouteMatchingConfig config;

    Fixture() {
        if (!std::filesystem::create_directory(directory)) {
            throw std::runtime_error("Replay timing fixture directory already exists");
        }
        const std::array<std::array<std::uint8_t, 4>, 3> pixels{{
            {10, 40, 80, 120}, {20, 60, 100, 200}, {255, 255, 255, 255}}};
        vh::RouteSignatureFile route;
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            std::ofstream pgm(directory / (std::to_string(i) + ".pgm"), std::ios::binary);
            pgm << "P5\n4 4\n255\n";
            // Each target pixel is a constant 2x2 block: resize has a known exact result.
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) pgm.put(static_cast<char>(pixels[i][(y / 2) * 2 + x / 2]));
            }
            assert(pgm.good());
            if (i < 2) {
                vh::RouteSignatureEntry entry;
                entry.frame_id = i;
                entry.width = 2;
                entry.height = 2;
                entry.payload.assign(pixels[i].begin(), pixels[i].end());
                route.entries.push_back(std::move(entry));
            }
        }
        config.route_path = directory / "route.vhrs";
        vh::write_route_signature_file(config.route_path, route);
        config.manifest_path = directory / "manifest.csv";
        std::ofstream manifest(config.manifest_path);
        // Timestamps intentionally include a future frame and an old out-of-order frame.
        const std::uint64_t timestamps[] = {
            1000000000, 1100000000, 1200000000, 1700000001,
            1800000000, 1900000000, 100000000, 2100000000};
        const int images[] = {0, 1, 0, 1, 2, 1, 0, 0};
        for (std::size_t i = 0; i < std::size(timestamps); ++i) {
            manifest << 10 + i << ',' << timestamps[i] << ',' << images[i] << ".pgm\n";
        }
        assert(manifest.good());
        config.target_width = 2;
        config.target_height = 2;
        config.minimum_confidence = 0.99;
        config.navigator_minimum_confidence = 0.95;
        config.navigator_max_match_age_ms = 200.0;
        config.navigator_forward_speed_mps = 0.5;
        config.max_direction_shift_px = 0;
    }
    ~Fixture() { std::filesystem::remove_all(directory); }
};

std::string field(const std::string& line, const std::string& key) {
    std::istringstream input(line);
    std::string token;
    const auto prefix = key + '=';
    while (input >> token) {
        if (token.starts_with(prefix)) return token.substr(prefix.size());
    }
    throw std::runtime_error("Missing test log field: " + key);
}

std::string replay(const Fixture& fixture) {
    const std::vector<vh::Timestamp> times{
        at_ms(900), // HealthMonitor initialization, then start/end per frame.
        at_ms(1000), at_ms(1000),
        at_ms(1190), at_ms(1300), // Match age exactly 200 ms, processing latency 110 ms.
        at_ms(1300), at_ms(1400) + 1ns,
        at_ms(1600), at_ms(1700), // Match timestamp one nanosecond in the future.
        at_ms(1800), at_ms(1800),
        at_ms(1900), at_ms(1903),
        at_ms(2000), at_ms(2001),
        at_ms(2100), at_ms(2107)};
    std::size_t next = 0;
    std::ostringstream metrics;
    const auto result = vh::match_replay_route(fixture.config, metrics, [&]() {
        assert(next < times.size());
        return times.at(next++);
    });
    assert(next == times.size());
    assert(result.frames_processed == 8);
    assert(result.last_frame_age_ms == 0.0 && result.last_processing_latency_ms == 7.0);
    const bool expected_commands[] = {true, true, false, false, false, true, false, true};
    const int expected_indices[] = {0, 1, 0, 1, -1, 1, 0, 0};
    std::istringstream output(metrics.str());
    std::string line;
    std::size_t frames = 0, commands = 0;
    while (std::getline(output, line)) {
        if (line.starts_with("match_frame ")) {
            assert(frames < 8);
            assert(field(line, "id") == std::to_string(10 + frames));
            assert(field(line, "valid") == (frames == 4 ? "false" : "true"));
            assert(field(line, "command_valid") == (expected_commands[frames] ? "true" : "false"));
            assert(field(line, "mavlink_ok") == "true" && field(line, "navigation_ok") == "true");
            if (expected_indices[frames] >= 0) {
                assert(field(line, "route_index") == std::to_string(expected_indices[frames]));
                assert(field(line, "confidence") == "1");
            }
            if (frames == 1) assert(field(line, "latency_ms") == "110");
            ++frames;
        } else if (line.starts_with("dry_run_command ")) {
            assert(commands < 8);
            assert(field(line, "valid") == (expected_commands[commands] ? "true" : "false"));
            assert(field(line, "vx_mps") == (expected_commands[commands] ? "0.5" : "0"));
            ++commands;
        }
    }
    assert(frames == 8 && commands == 8);
    return metrics.str();
}
} // namespace

int main() {
    Fixture fixture;
    const auto first = replay(fixture);
    const auto second = replay(fixture);
    assert(first == second); // Whole actual caller output is deterministic, not just decisions.
    bool rejected = false;
    std::ostringstream empty_output;
    try {
        (void)vh::match_replay_route(fixture.config, empty_output, {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && empty_output.str().empty());
}
