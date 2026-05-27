#include <cassert>
#include <chrono>
#include <filesystem>
#include <vector>

#include "visual_homing/route_signature_recorder.hpp"

namespace {

vh::Timestamp at_ns(std::uint64_t nanoseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(std::chrono::nanoseconds(nanoseconds)));
}

} // namespace

int main() {
    vh::RouteSignatureRecorder recorder;

    vh::Frame frame;
    frame.id = 21;
    frame.timestamp = at_ns(123456789ULL);
    frame.width = 2;
    frame.height = 2;
    frame.format = vh::PixelFormat::Gray8;
    frame.data = {8, 16, 32, 64};

    vh::NavigationEstimate nav;
    nav.altitude_m = 121.6;
    nav.course_error_rad = -0.125;

    recorder.observe(frame, nav);
    assert(recorder.route().entries.size() == 1);

    const auto& entry = recorder.route().entries[0];
    assert(entry.frame_id == 21);
    assert(entry.timestamp_ns == 123456789ULL);
    assert(entry.altitude_band_m == 122);
    assert(entry.heading_hint_rad < -0.124F);
    assert(entry.heading_hint_rad > -0.126F);
    assert(entry.width == 2);
    assert(entry.height == 2);
    assert(entry.format == vh::PixelFormat::Gray8);
    assert(entry.payload == frame.data);

    const auto path = std::filesystem::temp_directory_path() / "visual_homing_route_signature_recorder_test.vhrs";
    recorder.write_to(path);
    const auto loaded = vh::read_route_signature_file(path);
    assert(loaded.entries.size() == 1);
    assert(loaded.entries[0].payload == frame.data);
    assert(loaded.entries[0].altitude_band_m == 122);

    return 0;
}
