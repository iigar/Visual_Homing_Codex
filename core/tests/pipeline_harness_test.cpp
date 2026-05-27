#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "visual_homing/pipeline_harness.hpp"

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

    return 0;
}
