#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"
#include "visual_homing/replay_frame_source.hpp"
#include "visual_homing/route_signature.hpp"

namespace {
using Timer = std::chrono::steady_clock;
constexpr int measured_passes = 3;

struct Sample {
    double match_us;
    vh::RouteMatch match;
    std::vector<vh::RouteMatchCandidate> candidates;
};

struct Pass {
    double init_us;
    std::vector<Sample> samples;
};

double elapsed_us(Timer::time_point start, Timer::time_point end) {
    return std::chrono::duration<double, std::micro>(end - start).count();
}

std::vector<vh::Frame> route_frames(const vh::RouteSignatureFile& route) {
    std::vector<vh::Frame> frames;
    for (const auto& entry : route.entries) {
        // This is an unpaced matcher microbenchmark. No timestamp/freshness replay.
        frames.push_back({.id = entry.frame_id, .width = entry.width,
            .height = entry.height, .format = entry.format, .data = entry.payload});
    }
    return frames;
}

Pass measure_pass(const vh::RouteSignatureFile& route,
                  const std::vector<vh::Frame>& frames,
                  const vh::Gray8RouteMatcherConfig& config) {
    const auto init_start = Timer::now();
    vh::Gray8RouteMatcher matcher(route, config);
    const auto init_end = Timer::now();
    Pass result{.init_us = elapsed_us(init_start, init_end), .samples = {}};
    result.samples.reserve(frames.size());
    for (const auto& frame : frames) {
        const auto start = Timer::now();
        const auto match = matcher.match(frame);
        const auto end = Timer::now();
        result.samples.push_back({elapsed_us(start, end), match, matcher.recent_top_candidates()});
    }
    return result;
}

vh::Gray8RouteMatcherConfig configuration(const std::string& mode) {
    if (mode != "self-global" && mode != "self-window30"
        && mode != "self-window30-scale" && mode != "pgm" && mode != "pgm-scale") {
        throw std::invalid_argument("Unknown mode: " + mode);
    }
    const bool image_mode = mode == "pgm" || mode == "pgm-scale";
    return {.window_radius = mode == "self-global" || image_mode ? 0U : 30U,
        .minimum_confidence = image_mode ? 0.0 : 0.99,
        .enable_scale_refinement = mode == "self-window30-scale" || mode == "pgm-scale",
        .top_candidate_count = image_mode ? 5U : 0U};
}

void self_test() {
    vh::RouteSignatureFile route;
    for (const auto value : {10, 50, 50, 240}) {
        vh::RouteSignatureEntry entry;
        entry.frame_id = route.entries.size() + 10;
        entry.width = 8;
        entry.height = 8;
        entry.format = vh::PixelFormat::Gray8;
        entry.payload.assign(64, static_cast<std::uint8_t>(value));
        route.entries.push_back(entry);
    }
    const auto frames = route_frames(route);
    const std::vector<std::size_t> expected{0, 1, 1, 3};
    for (const std::string mode : {"self-global", "self-window30", "self-window30-scale", "pgm", "pgm-scale"}) {
        for (int repeat = 0; repeat < measured_passes; ++repeat) {
            const auto pass = measure_pass(route, frames, configuration(mode));
            if (pass.samples.size() != expected.size()) {
                throw std::runtime_error("Self-test: missing samples");
            }
            for (std::size_t i = 0; i < expected.size(); ++i) {
                const auto& sample = pass.samples[i];
                if (!sample.match.valid || sample.match.route_index != expected[i]
                    || sample.match.confidence != 1.0
                    || ((mode == "pgm" || mode == "pgm-scale") && (sample.candidates.size() != 4
                        || sample.candidates.front().confidence != 1.0
                        || sample.candidates.front().route_index != (i == 1 || i == 2 ? 2U : expected[i])))) {
                    throw std::runtime_error("Self-test: exact/duplicate frame result changed");
                }
            }
        }
    }
    // A sequence jump outside the local window must not be reported as a global search.
    while (route.entries.size() < 65) {
        auto entry = route.entries.front();
        entry.payload.assign(64, 10);
        route.entries.push_back(entry);
    }
    route.entries.back().payload.assign(64, 200);
    const auto all_frames = route_frames(route);
    const std::vector<vh::Frame> jump{all_frames.front(), all_frames.back()};
    const auto global = measure_pass(route, jump, configuration("self-global"));
    const auto window = measure_pass(route, jump, configuration("self-window30"));
    if (global.samples.back().match.route_index != 64 || window.samples.back().match.valid) {
        throw std::runtime_error("Self-test: global/window state distinction lost");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--self-test") {
            self_test();
            std::cout << "matcher benchmark self-test passed\n";
            return 0;
        }
        if (argc < 3 || argc > 4) {
            throw std::invalid_argument(
                "usage: gray8_matcher_benchmark ROUTE {self-global|self-window30|self-window30-scale|pgm|pgm-scale} [IMAGE.pgm]");
        }
        const std::string mode = argv[2];
        const auto config = configuration(mode);
        const bool image_mode = mode == "pgm" || mode == "pgm-scale";
        if (image_mode != (argc == 4)) {
            throw std::invalid_argument("Only pgm and pgm-scale modes require an image path");
        }
        const auto route = vh::read_route_signature_file(argv[1]);
        if (route.entries.empty()) {
            throw std::invalid_argument("Empty route");
        }
        std::vector<vh::Frame> frames;
        if (image_mode) {
            const std::filesystem::path path = argv[3];
            vh::ReplayFrameSource source(path.parent_path(), {{0, 0, path.filename()}});
            source.start();
            frames.push_back(source.poll().value());
        } else {
            frames = route_frames(route);
        }
        // Discard one complete pass. Every pass constructs a fresh matcher: its first
        // query is cold in tracking state, even though CPU/file caches can be warm.
        (void)measure_pass(route, frames, config);
        std::vector<Pass> passes;
        for (int repeat = 0; repeat < measured_passes; ++repeat) {
            passes.push_back(measure_pass(route, frames, config));
        }
        std::cout << std::setprecision(17);
        std::cout << "{\"kind\":\"config\",\"mode\":\"" << mode
                  << "\",\"route_entries\":" << route.entries.size()
                  << ",\"queries_per_pass\":" << frames.size()
                  << ",\"warmup_passes\":1,\"measured_passes\":" << measured_passes
                  << ",\"window_radius\":" << config.window_radius
                  << ",\"minimum_confidence\":" << config.minimum_confidence
                  << ",\"scale_refinement\":" << (config.enable_scale_refinement ? "true" : "false")
                  << ",\"scale_refinement_radius\":1,\"top_candidate_count\":" << config.top_candidate_count
                  << ",\"direction_shift_px\":0,\"timestamp_replay\":false}\n";
        for (std::size_t repeat = 0; repeat < passes.size(); ++repeat) {
            const auto& pass = passes[repeat];
            std::cout << "{\"kind\":\"init\",\"pass\":" << repeat << ",\"init_us\":" << pass.init_us << "}\n";
            for (std::size_t i = 0; i < pass.samples.size(); ++i) {
                const auto& sample = pass.samples[i];
                std::cout << "{\"kind\":\"match\",\"pass\":" << repeat << ",\"query\":" << i
                          << ",\"match_us\":" << sample.match_us
                          << ",\"route_index\":" << sample.match.route_index
                          << ",\"timestamp_ns\":" << std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 sample.match.timestamp.time_since_epoch()).count()
                          << ",\"progress\":" << sample.match.progress
                          << ",\"direction_error_rad\":" << sample.match.direction_error_rad
                          << ",\"direction_observation_valid\":" << (sample.match.direction_observation_valid ? "true" : "false")
                          << ",\"confidence\":" << sample.match.confidence
                          << ",\"valid\":" << (sample.match.valid ? "true" : "false")
                          << ",\"candidates\":[";
                for (std::size_t j = 0; j < sample.candidates.size(); ++j) {
                    if (j != 0) std::cout << ',';
                    std::cout << "{\"route_index\":" << sample.candidates[j].route_index
                              << ",\"progress\":" << sample.candidates[j].progress
                              << ",\"confidence\":" << sample.candidates[j].confidence << '}';
                }
                std::cout << "]}\n";
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "matcher_benchmark_error=" << error.what() << '\n';
        return 1;
    }
}
