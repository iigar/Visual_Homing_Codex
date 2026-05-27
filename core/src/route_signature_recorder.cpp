#include "visual_homing/route_signature_recorder.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace vh {
namespace {

std::uint64_t timestamp_to_ns(Timestamp timestamp) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp.time_since_epoch()).count();
    if (ns < 0) {
        throw std::runtime_error("Route signature recorder cannot store negative timestamps");
    }
    return static_cast<std::uint64_t>(ns);
}

std::int16_t altitude_to_band(double altitude_m) {
    const auto rounded = std::lround(altitude_m);
    if (rounded < std::numeric_limits<std::int16_t>::min()
        || rounded > std::numeric_limits<std::int16_t>::max()) {
        throw std::runtime_error("Route signature altitude band exceeds int16 range");
    }
    return static_cast<std::int16_t>(rounded);
}

std::uint16_t dimension_to_u16(int value, const char* name) {
    if (value <= 0 || value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error(std::string("Route signature frame ") + name + " is out of range");
    }
    return static_cast<std::uint16_t>(value);
}

} // namespace

void RouteSignatureRecorder::observe(const Frame& frame, const NavigationEstimate& nav) {
    if (frame.format != PixelFormat::Gray8) {
        throw std::runtime_error("Route signature recorder currently accepts only Gray8 frames");
    }
    if (frame.width <= 0 || frame.height <= 0) {
        throw std::runtime_error("Route signature recorder received invalid frame dimensions");
    }

    const auto expected_size = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    if (frame.data.size() != expected_size) {
        throw std::runtime_error("Route signature recorder received malformed frame payload");
    }

    RouteSignatureEntry entry;
    entry.frame_id = frame.id;
    entry.timestamp_ns = timestamp_to_ns(frame.timestamp);
    entry.altitude_band_m = altitude_to_band(nav.altitude_m);
    entry.heading_hint_rad = static_cast<float>(nav.course_error_rad);
    entry.width = dimension_to_u16(frame.width, "width");
    entry.height = dimension_to_u16(frame.height, "height");
    entry.format = frame.format;
    entry.payload = frame.data;
    route_.entries.push_back(std::move(entry));
}

const RouteSignatureFile& RouteSignatureRecorder::route() const {
    return route_;
}

void RouteSignatureRecorder::write_to(const std::filesystem::path& path) const {
    write_route_signature_file(path, route_);
}

} // namespace vh
