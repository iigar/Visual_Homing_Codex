#include "visual_homing/route_signature.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace vh {
namespace {

constexpr std::array<char, 4> magic = {'V', 'H', 'R', 'S'};
constexpr std::uint16_t header_size = 16;
constexpr std::uint8_t little_endian = 1;

std::uint8_t pixel_format_to_u8(PixelFormat format) {
    switch (format) {
    case PixelFormat::Gray8:
        return 1;
    case PixelFormat::Bgr8:
        return 2;
    case PixelFormat::Thermal16:
        return 3;
    }
    throw std::runtime_error("Unknown pixel format");
}

PixelFormat pixel_format_from_u8(std::uint8_t value) {
    switch (value) {
    case 1:
        return PixelFormat::Gray8;
    case 2:
        return PixelFormat::Bgr8;
    case 3:
        return PixelFormat::Thermal16;
    default:
        throw std::runtime_error("Unsupported route signature pixel format");
    }
}

void write_u8(std::ostream& output, std::uint8_t value) {
    output.put(static_cast<char>(value));
}

void write_u16(std::ostream& output, std::uint16_t value) {
    write_u8(output, static_cast<std::uint8_t>(value & 0xFFU));
    write_u8(output, static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_i16(std::ostream& output, std::int16_t value) {
    write_u16(output, static_cast<std::uint16_t>(value));
}

void write_u32(std::ostream& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        write_u8(output, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void write_u64(std::ostream& output, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        write_u8(output, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void write_f32(std::ostream& output, float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(output, bits);
}

std::uint8_t read_u8(std::istream& input) {
    const auto value = input.get();
    if (value == std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected end of route signature file");
    }
    return static_cast<std::uint8_t>(value);
}

std::uint16_t read_u16(std::istream& input) {
    std::uint16_t value = 0;
    value |= static_cast<std::uint16_t>(read_u8(input));
    value |= static_cast<std::uint16_t>(read_u8(input)) << 8U;
    return value;
}

std::int16_t read_i16(std::istream& input) {
    return static_cast<std::int16_t>(read_u16(input));
}

std::uint32_t read_u32(std::istream& input) {
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        value |= static_cast<std::uint32_t>(read_u8(input)) << shift;
    }
    return value;
}

std::uint64_t read_u64(std::istream& input) {
    std::uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        value |= static_cast<std::uint64_t>(read_u8(input)) << shift;
    }
    return value;
}

float read_f32(std::istream& input) {
    const auto bits = read_u32(input);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void require_stream_ok(const std::ios& stream, const std::string& action) {
    if (!stream) {
        throw std::runtime_error("Failed to " + action + " route signature file");
    }
}

} // namespace

void write_route_signature_file(const std::filesystem::path& path, const RouteSignatureFile& route) {
    if (route.version != route_signature_format_version) {
        throw std::runtime_error("Unsupported route signature version for write");
    }
    if (route.entries.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Route signature entry count exceeds v1 limit");
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not open route signature file for write: " + path.string());
    }

    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    write_u16(output, route.version);
    write_u16(output, header_size);
    write_u8(output, little_endian);
    write_u8(output, 0);
    write_u16(output, 0);
    write_u32(output, static_cast<std::uint32_t>(route.entries.size()));

    for (const auto& entry : route.entries) {
        if (entry.width == 0 || entry.height == 0) {
            throw std::runtime_error("Route signature entry dimensions must be positive");
        }
        if (entry.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Route signature payload exceeds v1 limit");
        }

        write_u64(output, entry.frame_id);
        write_u64(output, entry.timestamp_ns);
        write_i16(output, entry.altitude_band_m);
        write_f32(output, entry.heading_hint_rad);
        write_u16(output, entry.width);
        write_u16(output, entry.height);
        write_u8(output, pixel_format_to_u8(entry.format));
        write_u8(output, 0);
        write_u16(output, 0);
        write_u32(output, static_cast<std::uint32_t>(entry.payload.size()));
        output.write(
            reinterpret_cast<const char*>(entry.payload.data()),
            static_cast<std::streamsize>(entry.payload.size()));
    }

    require_stream_ok(output, "write");
}

RouteSignatureFile read_route_signature_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open route signature file for read: " + path.string());
    }

    std::array<char, 4> read_magic{};
    input.read(read_magic.data(), static_cast<std::streamsize>(read_magic.size()));
    if (read_magic != magic) {
        throw std::runtime_error("Invalid route signature magic");
    }

    RouteSignatureFile route;
    route.version = read_u16(input);
    const auto read_header_size = read_u16(input);
    const auto endian = read_u8(input);
    (void)read_u8(input);
    (void)read_u16(input);
    const auto entry_count = read_u32(input);

    if (route.version != route_signature_format_version) {
        throw std::runtime_error("Unsupported route signature version for read");
    }
    if (read_header_size != header_size) {
        throw std::runtime_error("Unsupported route signature header size");
    }
    if (endian != little_endian) {
        throw std::runtime_error("Unsupported route signature endian policy");
    }

    route.entries.reserve(entry_count);
    for (std::uint32_t index = 0; index < entry_count; ++index) {
        RouteSignatureEntry entry;
        entry.frame_id = read_u64(input);
        entry.timestamp_ns = read_u64(input);
        entry.altitude_band_m = read_i16(input);
        entry.heading_hint_rad = read_f32(input);
        entry.width = read_u16(input);
        entry.height = read_u16(input);
        entry.format = pixel_format_from_u8(read_u8(input));
        (void)read_u8(input);
        (void)read_u16(input);
        const auto payload_size = read_u32(input);

        if (entry.width == 0 || entry.height == 0) {
            throw std::runtime_error("Route signature entry has invalid dimensions");
        }

        entry.payload.resize(payload_size);
        input.read(reinterpret_cast<char*>(entry.payload.data()), static_cast<std::streamsize>(entry.payload.size()));
        if (input.gcount() != static_cast<std::streamsize>(entry.payload.size())) {
            throw std::runtime_error("Truncated route signature payload");
        }

        route.entries.push_back(std::move(entry));
    }

    return route;
}

RouteSignatureSummary summarize_route_signature(const RouteSignatureFile& route) {
    RouteSignatureSummary summary;
    summary.version = route.version;
    summary.entry_count = static_cast<std::uint64_t>(route.entries.size());

    if (route.entries.empty()) {
        return summary;
    }

    const auto& first = route.entries.front();
    summary.first_frame_id = first.frame_id;
    summary.last_frame_id = route.entries.back().frame_id;
    summary.first_timestamp_ns = first.timestamp_ns;
    summary.last_timestamp_ns = route.entries.back().timestamp_ns;
    summary.width = first.width;
    summary.height = first.height;
    summary.min_payload_bytes = static_cast<std::uint64_t>(first.payload.size());
    summary.max_payload_bytes = static_cast<std::uint64_t>(first.payload.size());
    summary.min_altitude_band_m = first.altitude_band_m;
    summary.max_altitude_band_m = first.altitude_band_m;
    summary.min_heading_hint_rad = first.heading_hint_rad;
    summary.max_heading_hint_rad = first.heading_hint_rad;

    auto previous_timestamp = first.timestamp_ns;
    const auto expected_payload_size = first.payload.size();
    const auto expected_altitude_band_m = first.altitude_band_m;
    const auto expected_heading_hint_rad = first.heading_hint_rad;
    for (std::size_t index = 0; index < route.entries.size(); ++index) {
        const auto& entry = route.entries[index];
        const auto payload_size = static_cast<std::uint64_t>(entry.payload.size());
        summary.min_payload_bytes = std::min(summary.min_payload_bytes, payload_size);
        summary.max_payload_bytes = std::max(summary.max_payload_bytes, payload_size);
        summary.total_payload_bytes += payload_size;
        summary.min_altitude_band_m = std::min(summary.min_altitude_band_m, entry.altitude_band_m);
        summary.max_altitude_band_m = std::max(summary.max_altitude_band_m, entry.altitude_band_m);
        summary.min_heading_hint_rad = std::min(summary.min_heading_hint_rad, entry.heading_hint_rad);
        summary.max_heading_hint_rad = std::max(summary.max_heading_hint_rad, entry.heading_hint_rad);

        if (index > 0 && entry.timestamp_ns < previous_timestamp) {
            summary.timestamps_monotonic = false;
        }
        previous_timestamp = entry.timestamp_ns;

        if (entry.width != summary.width || entry.height != summary.height) {
            summary.uniform_dimensions = false;
        }
        if (entry.payload.size() != expected_payload_size) {
            summary.uniform_payload_size = false;
        }
        if (entry.altitude_band_m != expected_altitude_band_m) {
            summary.uniform_altitude_band = false;
        }
        if (entry.heading_hint_rad != expected_heading_hint_rad) {
            summary.uniform_heading_hint = false;
        }
        if (entry.format != PixelFormat::Gray8) {
            summary.all_gray8 = false;
        }
    }

    return summary;
}

RouteSignatureSummary inspect_route_signature_file(const std::filesystem::path& path) {
    return summarize_route_signature(read_route_signature_file(path));
}

} // namespace vh
