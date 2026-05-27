#include "visual_homing/replay_frame_source.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace vh {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(trim(field));
    }
    return fields;
}

std::uint64_t parse_u64(const std::string& value, const std::string& field_name, std::size_t line_number) {
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed, 10);
    if (consumed != value.size()) {
        throw std::runtime_error("Invalid " + field_name + " on manifest line " + std::to_string(line_number));
    }
    return parsed;
}

std::string read_pgm_token(std::istream& input) {
    std::string token;
    while (input >> token) {
        if (!token.empty() && token[0] == '#') {
            std::string ignored;
            std::getline(input, ignored);
            continue;
        }
        return token;
    }
    throw std::runtime_error("Unexpected end of PGM header");
}

Frame load_pgm_gray8(const std::filesystem::path& path, std::uint64_t id, std::uint64_t timestamp_ns) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open replay image: " + path.string());
    }

    const auto magic = read_pgm_token(input);
    if (magic != "P5") {
        throw std::runtime_error("Unsupported replay image format in " + path.string() + ": expected binary PGM P5");
    }

    const auto width = std::stoi(read_pgm_token(input));
    const auto height = std::stoi(read_pgm_token(input));
    const auto max_value = std::stoi(read_pgm_token(input));

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid PGM dimensions in " + path.string());
    }
    if (max_value != 255) {
        throw std::runtime_error("Unsupported PGM max value in " + path.string() + ": expected 255");
    }

    const auto separator = input.get();
    if (separator == '\r' && input.peek() == '\n') {
        input.get();
    }

    Frame frame;
    frame.id = id;
    frame.timestamp = Timestamp(std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds(timestamp_ns)));
    frame.width = width;
    frame.height = height;
    frame.format = PixelFormat::Gray8;
    frame.data.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    input.read(reinterpret_cast<char*>(frame.data.data()), static_cast<std::streamsize>(frame.data.size()));
    if (input.gcount() != static_cast<std::streamsize>(frame.data.size())) {
        throw std::runtime_error("Truncated PGM pixel payload in " + path.string());
    }

    return frame;
}

} // namespace

ReplayFrameSource ReplayFrameSource::load_manifest(const std::filesystem::path& manifest_path) {
    std::ifstream input(manifest_path);
    if (!input) {
        throw std::runtime_error("Could not open replay manifest: " + manifest_path.string());
    }

    std::vector<ReplayFrameEntry> entries;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto cleaned = trim(line);
        if (cleaned.empty() || cleaned[0] == '#') {
            continue;
        }

        const auto fields = split_csv_line(cleaned);
        if (fields.size() != 3) {
            throw std::runtime_error("Expected id,timestamp_ns,path on manifest line " + std::to_string(line_number));
        }

        ReplayFrameEntry entry;
        entry.id = parse_u64(fields[0], "frame id", line_number);
        entry.timestamp_ns = parse_u64(fields[1], "timestamp_ns", line_number);
        entry.image_path = fields[2];
        entries.push_back(std::move(entry));
    }

    if (entries.empty()) {
        throw std::runtime_error("Replay manifest contains no frame entries: " + manifest_path.string());
    }

    return ReplayFrameSource(manifest_path.parent_path(), std::move(entries));
}

ReplayFrameSource::ReplayFrameSource(std::filesystem::path base_dir, std::vector<ReplayFrameEntry> entries)
    : base_dir_(std::move(base_dir)), entries_(std::move(entries)) {}

bool ReplayFrameSource::start() {
    next_index_ = 0;
    running_ = true;
    return true;
}

void ReplayFrameSource::stop() {
    running_ = false;
}

std::optional<Frame> ReplayFrameSource::poll() {
    if (!running_ || next_index_ >= entries_.size()) {
        return std::nullopt;
    }

    const auto& entry = entries_[next_index_++];
    const auto image_path = entry.image_path.is_absolute() ? entry.image_path : base_dir_ / entry.image_path;
    return load_pgm_gray8(image_path, entry.id, entry.timestamp_ns);
}

std::size_t ReplayFrameSource::size() const {
    return entries_.size();
}

bool ReplayFrameSource::running() const {
    return running_;
}

} // namespace vh
