#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "visual_homing/pipeline_harness.hpp"
#include "visual_homing/route_signature.hpp"

namespace {

void write_pgm(const std::filesystem::path& path, const std::vector<unsigned char>& pixels) {
    std::ofstream output(path, std::ios::binary);
    output << "P5\n4 4\n255\n";
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
}

std::filesystem::path write_fixture() {
    const auto root = std::filesystem::temp_directory_path() / "visual_homing_pipeline_harness_test";
    const auto frames = root / "frames";
    std::filesystem::create_directories(frames);

    write_pgm(frames / "frame_000.pgm", {
        0, 10, 100, 110,
        20, 30, 120, 130,
        200, 210, 40, 50,
        220, 230, 60, 70,
    });
    write_pgm(frames / "frame_001.pgm", {
        5, 15, 105, 115,
        25, 35, 125, 135,
        205, 215, 45, 55,
        225, 235, 65, 75,
    });

    const auto manifest = root / "manifest.csv";
    std::ofstream output(manifest);
    output << "0,0,frames/frame_000.pgm\n";
    output << "1,33333333,frames/frame_001.pgm\n";
    return manifest;
}

} // namespace

int main() {
    std::ostringstream metrics;

    vh::PipelineConfig config;
    config.manifest_path = write_fixture();
    config.target_width = 2;
    config.target_height = 2;

    const auto result = vh::run_replay_pipeline(config, metrics);
    assert(result.frames_processed == 2);
    assert(result.last_processing_latency_ms >= 0.0);

    const auto output = metrics.str();
    assert(output.find("pipeline_start frames_available=2 target=2x2") != std::string::npos);
    assert(output.find("frame id=0 size=2x2") != std::string::npos);
    assert(output.find("frame id=1 size=2x2") != std::string::npos);
    assert(output.find("state=degraded") != std::string::npos);
    assert(output.find("pipeline_done frames_processed=2") != std::string::npos);

    std::ostringstream record_metrics;
    vh::RouteRecordingConfig record_config;
    record_config.manifest_path = config.manifest_path;
    record_config.route_output_path = std::filesystem::temp_directory_path() / "visual_homing_pipeline_record_test.vhrs";
    record_config.target_width = 2;
    record_config.target_height = 2;
    record_config.altitude_m = 75.2;
    record_config.heading_hint_rad = 0.125;

    const auto record_result = vh::record_replay_route(record_config, record_metrics);
    assert(record_result.frames_processed == 2);

    const auto route = vh::read_route_signature_file(record_config.route_output_path);
    assert(route.entries.size() == 2);
    assert(route.entries[0].frame_id == 0);
    assert(route.entries[0].timestamp_ns == 0);
    assert(route.entries[0].altitude_band_m == 75);
    assert(route.entries[0].width == 2);
    assert(route.entries[0].height == 2);
    assert(route.entries[0].payload == std::vector<std::uint8_t>({15, 115, 215, 55}));
    assert(route.entries[1].frame_id == 1);
    assert(route.entries[1].timestamp_ns == 33333333ULL);
    assert(route.entries[1].payload == std::vector<std::uint8_t>({20, 120, 220, 60}));

    const auto record_output = record_metrics.str();
    assert(record_output.find("route_record_start frames_available=2 target=2x2") != std::string::npos);
    assert(record_output.find("route_record_done frames_processed=2 entries=2") != std::string::npos);

    std::ostringstream match_metrics;
    vh::RouteMatchingConfig match_config;
    match_config.route_path = record_config.route_output_path;
    match_config.manifest_path = config.manifest_path;
    match_config.target_width = 2;
    match_config.target_height = 2;
    match_config.window_radius = 1;
    match_config.minimum_confidence = 0.9;

    const auto match_result = vh::match_replay_route(match_config, match_metrics);
    assert(match_result.frames_processed == 2);

    const auto match_output = match_metrics.str();
    assert(match_output.find("route_match_start frames_available=2") != std::string::npos);
    assert(match_output.find("match_frame id=0 route_index=0") != std::string::npos);
    assert(match_output.find("match_frame id=1 route_index=1") != std::string::npos);
    assert(match_output.find("direction_error_rad=") != std::string::npos);
    assert(match_output.find("valid=true") != std::string::npos);
    assert(match_output.find("route_match_done frames_processed=2") != std::string::npos);

    return 0;
}
