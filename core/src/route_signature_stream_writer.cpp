#include "visual_homing/route_signature_stream_writer.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace vh {
namespace {

constexpr std::array<char, 4> magic = {'V', 'H', 'R', 'S'};
constexpr std::uint16_t header_size = 16;
constexpr std::uint8_t little_endian = 1;
constexpr std::streamoff entry_count_offset = 12;
constexpr std::uint32_t max_route_entries_v1 = 100000;
constexpr std::uint32_t max_payload_bytes_v1 = 64U * 1024U * 1024U;

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

std::uint8_t pixel_format_to_u8(PixelFormat format) {
    switch (format) {
    case PixelFormat::Gray8:
        return 1;
    case PixelFormat::Bgr8:
        return 2;
    case PixelFormat::Thermal16:
        return 3;
    }
    throw std::runtime_error("Unknown route signature pixel format");
}

std::uint64_t bytes_per_pixel(PixelFormat format) {
    switch (format) {
    case PixelFormat::Gray8:
        return 1;
    case PixelFormat::Bgr8:
        return 3;
    case PixelFormat::Thermal16:
        return 2;
    }
    throw std::runtime_error("Unknown route signature pixel format");
}

void require_stream_ok(const std::ios& stream, const std::string& action) {
    if (!stream) {
        throw std::runtime_error("Failed to " + action + " streaming route signature file");
    }
}

void validate_entry(const RouteSignatureEntry& entry) {
    if (entry.width == 0 || entry.height == 0) {
        throw std::runtime_error("Streaming route signature entry dimensions must be positive");
    }
    const auto expected = static_cast<std::uint64_t>(entry.width)
        * static_cast<std::uint64_t>(entry.height)
        * bytes_per_pixel(entry.format);
    if (expected > max_payload_bytes_v1) {
        throw std::runtime_error("Streaming route signature payload exceeds v1 safety limit");
    }
    if (entry.payload.size() != expected) {
        throw std::runtime_error(
            "Streaming route signature payload size does not match dimensions and format");
    }
}

void write_header(std::ostream& output, std::uint32_t entry_count) {
    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    write_u16(output, route_signature_format_version);
    write_u16(output, header_size);
    write_u8(output, little_endian);
    write_u8(output, 0);
    write_u16(output, 0);
    write_u32(output, entry_count);
}

void write_entry(std::ostream& output, const RouteSignatureEntry& entry) {
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

} // namespace

RouteSignatureStreamWriter::RouteSignatureStreamWriter(
    RouteSignatureStreamWriterConfig config)
    : config_(std::move(config)),
      partial_path_(config_.output_path.string() + ".partial") {
    if (config_.output_path.empty()) {
        throw std::invalid_argument("Streaming route output path must not be empty");
    }
    if (config_.checkpoint_interval_entries == 0) {
        throw std::invalid_argument(
            "Streaming route checkpoint interval must be positive");
    }
    if (std::filesystem::exists(config_.output_path)) {
        throw std::runtime_error("Streaming route output path already exists");
    }
    if (std::filesystem::exists(partial_path_)) {
        throw std::runtime_error("Streaming route partial path already exists");
    }

    output_.open(
        partial_path_,
        std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!output_) {
        throw std::runtime_error(
            "Could not open streaming route partial file: " + partial_path_.string());
    }
    write_header(output_, 0);
    output_.flush();
    require_stream_ok(output_, "initialize");
}

RouteSignatureStreamWriter::~RouteSignatureStreamWriter() {
    if (output_.is_open()) {
        output_.flush();
        output_.close();
    }
}

void RouteSignatureStreamWriter::append(const RouteSignatureEntry& entry) {
    require_active();
    if (entry_count_ >= max_route_entries_v1) {
        throw std::runtime_error("Streaming route entry count exceeds v1 safety limit");
    }
    validate_entry(entry);
    output_.seekp(0, std::ios::end);
    require_stream_ok(output_, "seek to append");
    write_entry(output_, entry);
    require_stream_ok(output_, "append entry to");
    ++entry_count_;
    if (entry_count_ % config_.checkpoint_interval_entries == 0) {
        checkpoint();
    }
}

void RouteSignatureStreamWriter::checkpoint() {
    require_active();
    output_.flush();
    require_stream_ok(output_, "flush entries in");
    const auto end_position = output_.tellp();
    require_stream_ok(output_, "capture end position in");
    output_.seekp(entry_count_offset, std::ios::beg);
    require_stream_ok(output_, "seek to checkpoint in");
    write_u32(output_, entry_count_);
    output_.flush();
    require_stream_ok(output_, "write checkpoint in");
    output_.seekp(end_position);
    require_stream_ok(output_, "restore append position in");
    checkpointed_entry_count_ = entry_count_;
}

void RouteSignatureStreamWriter::finalize() {
    if (finalized_) {
        return;
    }
    require_active();
    checkpoint();
    output_.close();
    if (std::filesystem::exists(config_.output_path)) {
        throw std::runtime_error("Streaming route output appeared before finalize");
    }
    std::filesystem::rename(partial_path_, config_.output_path);
    finalized_ = true;
}

const std::filesystem::path& RouteSignatureStreamWriter::output_path() const {
    return config_.output_path;
}

const std::filesystem::path& RouteSignatureStreamWriter::partial_path() const {
    return partial_path_;
}

std::uint32_t RouteSignatureStreamWriter::entry_count() const {
    return entry_count_;
}

std::uint32_t RouteSignatureStreamWriter::checkpointed_entry_count() const {
    return checkpointed_entry_count_;
}

bool RouteSignatureStreamWriter::finalized() const {
    return finalized_;
}

void RouteSignatureStreamWriter::require_active() const {
    if (finalized_ || !output_.is_open()) {
        throw std::runtime_error("Streaming route writer is not active");
    }
}

} // namespace vh
