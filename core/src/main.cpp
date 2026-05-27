#include <iostream>
#include <cstdint>
#include <exception>
#include <string>

#include "visual_homing/replay_frame_source.hpp"
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

    std::cout << "Realtime C++ core skeleton ready\n";
    std::cout << "usage: visual_homing_core --replay <manifest.csv>\n";
    std::cout << "uptime_ms=" << vh::milliseconds_between(started, vh::now()) << "\n";
    return 0;
}
