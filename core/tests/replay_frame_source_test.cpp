#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

#include "visual_homing/replay_frame_source.hpp"
#include "visual_homing/time.hpp"

namespace {

void write_pgm(const std::filesystem::path& path, const std::vector<unsigned char>& pixels) {
    std::ofstream output(path, std::ios::binary);
    output << "P5\n2 2\n255\n";
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
}

std::filesystem::path write_fixture() {
    const auto root = std::filesystem::temp_directory_path() / "visual_homing_replay_frame_source_test";
    const auto frames = root / "frames";
    std::filesystem::create_directories(frames);

    write_pgm(frames / "frame_000.pgm", {0, 64, 128, 255});
    write_pgm(frames / "frame_001.pgm", {255, 128, 64, 0});

    const auto manifest = root / "manifest.csv";
    std::ofstream output(manifest);
    output << "# vh_replay_manifest_v1\n";
    output << "# id,timestamp_ns,path\n";
    output << "0,0,frames/frame_000.pgm\n";
    output << "1,33333333,frames/frame_001.pgm\n";
    return manifest;
}

} // namespace

int main() {
    const auto manifest = write_fixture();

    auto replay = vh::ReplayFrameSource::load_manifest(manifest);
    assert(replay.size() == 2);
    assert(!replay.running());
    assert(replay.start());
    assert(replay.running());

    const auto first = replay.poll();
    assert(first.has_value());
    assert(first->id == 0);
    assert(first->width == 2);
    assert(first->height == 2);
    assert(first->format == vh::PixelFormat::Gray8);
    assert(first->data.size() == 4);
    assert(first->data[0] == 0);
    assert(first->data[1] == 64);
    assert(first->data[2] == 128);
    assert(first->data[3] == 255);

    const auto second = replay.poll();
    assert(second.has_value());
    assert(second->id == 1);
    assert(vh::milliseconds_between(first->timestamp, second->timestamp) > 33.0);
    assert(vh::milliseconds_between(first->timestamp, second->timestamp) < 34.0);

    assert(!replay.poll().has_value());
    replay.stop();
    assert(!replay.running());

    return 0;
}
