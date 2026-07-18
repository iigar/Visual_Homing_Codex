#include <cassert>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "visual_homing/route_signature.hpp"
#include "visual_homing/route_signature_streaming_recorder.hpp"

namespace {

vh::Frame frame_at(std::uint64_t id) {
    vh::Frame frame;
    frame.id = id;
    frame.timestamp = vh::Timestamp{} + std::chrono::milliseconds(static_cast<std::int64_t>(id));
    frame.width = 16;
    frame.height = 12;
    frame.format = vh::PixelFormat::Gray8;
    frame.data.assign(16 * 12, static_cast<std::uint8_t>(id % 251));
    return frame;
}

vh::NavigationEstimate nav_at(std::uint64_t id) {
    vh::NavigationEstimate nav;
    nav.timestamp = vh::Timestamp{} + std::chrono::milliseconds(static_cast<std::int64_t>(id));
    nav.altitude_m = 40.0 + static_cast<double>(id % 3);
    nav.course_error_rad = 0.01 * static_cast<double>(id);
    nav.confidence = 1.0;
    return nav;
}

void remove_route_artifacts(const std::filesystem::path& path) {
    std::filesystem::remove(path);
    auto partial = path;
    partial += ".partial";
    std::filesystem::remove(partial);
}

} // namespace

int main() {
    const auto route_path = std::filesystem::temp_directory_path()
        / "visual_homing_route_signature_streaming_recorder_test.vhrs";
    remove_route_artifacts(route_path);

    vh::RouteSignatureStreamingRecorder recorder({
        .output_path = route_path,
        .queue_capacity_entries = 128,
        .checkpoint_interval_entries = 7,
    });
    for (std::uint64_t id = 1; id <= 100; ++id) {
        recorder.observe(frame_at(id), nav_at(id));
    }
    recorder.finalize();
    recorder.finalize();

    const auto metrics = recorder.metrics();
    assert(metrics.entries_enqueued == 100);
    assert(metrics.entries_written == 100);
    assert(metrics.current_queue_depth == 0);
    assert(metrics.max_queue_depth > 0);
    assert(metrics.max_queue_depth <= 128);
    assert(metrics.queue_full_events == 0);
    assert(metrics.write_failures == 0);
    assert(metrics.total_write_latency_ms >= 0.0);
    assert(metrics.max_write_latency_ms >= 0.0);
    assert(metrics.finalized);

    const auto route = vh::read_route_signature_file(route_path);
    assert(route.entries.size() == 100);
    assert(route.entries.front().frame_id == 1);
    assert(route.entries.back().frame_id == 100);
    assert(route.entries.front().width == 16);
    assert(route.entries.front().height == 12);
    assert(route.entries.front().payload == frame_at(1).data);
    assert(route.entries.back().altitude_band_m == 41);
    remove_route_artifacts(route_path);

    const auto interrupted_path = std::filesystem::temp_directory_path()
        / "visual_homing_route_signature_streaming_recorder_interrupted_test.vhrs";
    remove_route_artifacts(interrupted_path);
    {
        vh::RouteSignatureStreamingRecorder interrupted({
            .output_path = interrupted_path,
            .queue_capacity_entries = 8,
            .checkpoint_interval_entries = 2,
        });
        interrupted.observe(frame_at(1), nav_at(1));
        interrupted.observe(frame_at(2), nav_at(2));
        interrupted.observe(frame_at(3), nav_at(3));
    }
    auto interrupted_partial = interrupted_path;
    interrupted_partial += ".partial";
    assert(!std::filesystem::exists(interrupted_path));
    assert(std::filesystem::exists(interrupted_partial));
    const auto interrupted_route = vh::read_route_signature_file(interrupted_partial);
    assert(interrupted_route.entries.size() == 2);
    remove_route_artifacts(interrupted_path);

    bool rejected_capacity = false;
    try {
        vh::RouteSignatureStreamingRecorder invalid({
            .output_path = route_path,
            .queue_capacity_entries = 0,
            .checkpoint_interval_entries = 1,
        });
    } catch (const std::invalid_argument&) {
        rejected_capacity = true;
    }
    assert(rejected_capacity);
    assert(!std::filesystem::exists(route_path));
    auto invalid_partial = route_path;
    invalid_partial += ".partial";
    assert(!std::filesystem::exists(invalid_partial));

    bool rejected_checkpoint = false;
    try {
        vh::RouteSignatureStreamingRecorder invalid({
            .output_path = route_path,
            .queue_capacity_entries = 1,
            .checkpoint_interval_entries = 0,
        });
    } catch (const std::invalid_argument&) {
        rejected_checkpoint = true;
    }
    assert(rejected_checkpoint);
    assert(!std::filesystem::exists(invalid_partial));

    return 0;
}
