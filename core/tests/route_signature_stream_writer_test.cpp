#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/route_signature_stream_writer.hpp"

namespace {

std::filesystem::path unique_path(const std::string& label) {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("visual_homing_" + label + "_" + std::to_string(suffix) + ".vhrs");
}

std::filesystem::path partial_path(const std::filesystem::path& output_path) {
    return std::filesystem::path(output_path.string() + ".partial");
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

vh::RouteSignatureEntry entry(std::uint64_t id, std::uint8_t value) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = 1'000'000U * id;
    result.altitude_band_m = static_cast<std::int16_t>(id);
    result.heading_hint_rad = static_cast<float>(id) * 0.1F;
    result.width = 4;
    result.height = 3;
    result.format = vh::PixelFormat::Gray8;
    result.payload.assign(12, value);
    return result;
}

void cleanup(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(partial_path(path), error);
}

} // namespace

int main() {
    {
        const auto expected_path = unique_path("stream_expected");
        const auto streamed_path = unique_path("stream_actual");
        cleanup(expected_path);
        cleanup(streamed_path);

        vh::RouteSignatureFile route;
        route.entries = {entry(1, 10), entry(2, 20), entry(3, 30)};
        vh::write_route_signature_file(expected_path, route);

        vh::RouteSignatureStreamWriterConfig config;
        config.output_path = streamed_path;
        config.checkpoint_interval_entries = 2;
        vh::RouteSignatureStreamWriter writer(config);
        for (const auto& route_entry : route.entries) {
            writer.append(route_entry);
        }
        assert(writer.entry_count() == 3);
        assert(writer.checkpointed_entry_count() == 2);
        assert(std::filesystem::exists(writer.partial_path()));
        writer.finalize();
        assert(writer.finalized());
        assert(writer.checkpointed_entry_count() == 3);
        assert(std::filesystem::exists(streamed_path));
        assert(!std::filesystem::exists(partial_path(streamed_path)));
        assert(read_bytes(expected_path) == read_bytes(streamed_path));

        const auto read_back = vh::read_route_signature_file(streamed_path);
        assert(read_back.entries.size() == route.entries.size());
        assert(read_back.entries.back().payload == route.entries.back().payload);
        cleanup(expected_path);
        cleanup(streamed_path);
    }
    {
        const auto output_path = unique_path("stream_interrupted");
        cleanup(output_path);
        {
            vh::RouteSignatureStreamWriterConfig config;
            config.output_path = output_path;
            config.checkpoint_interval_entries = 2;
            vh::RouteSignatureStreamWriter writer(config);
            writer.append(entry(1, 11));
            writer.append(entry(2, 22));
            writer.append(entry(3, 33));
            assert(writer.entry_count() == 3);
            assert(writer.checkpointed_entry_count() == 2);
        }
        assert(!std::filesystem::exists(output_path));
        assert(std::filesystem::exists(partial_path(output_path)));
        const auto recovered = vh::read_route_signature_file(partial_path(output_path));
        assert(recovered.entries.size() == 2);
        assert(recovered.entries[0].payload.front() == 11);
        assert(recovered.entries[1].payload.front() == 22);
        cleanup(output_path);
    }
    {
        const auto output_path = unique_path("stream_empty");
        cleanup(output_path);
        vh::RouteSignatureStreamWriterConfig config;
        config.output_path = output_path;
        vh::RouteSignatureStreamWriter writer(config);
        writer.finalize();
        writer.finalize();
        assert(vh::read_route_signature_file(output_path).entries.empty());
        cleanup(output_path);
    }
    {
        const auto output_path = unique_path("stream_invalid");
        cleanup(output_path);
        {
            vh::RouteSignatureStreamWriterConfig config;
            config.output_path = output_path;
            vh::RouteSignatureStreamWriter writer(config);
            auto invalid = entry(1, 42);
            invalid.payload.pop_back();
            bool rejected = false;
            try {
                writer.append(invalid);
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            assert(rejected);
            assert(writer.entry_count() == 0);
            writer.checkpoint();
        }
        assert(vh::read_route_signature_file(partial_path(output_path)).entries.empty());
        cleanup(output_path);
    }
    {
        const auto output_path = unique_path("stream_collision");
        cleanup(output_path);
        vh::RouteSignatureFile route;
        vh::write_route_signature_file(output_path, route);
        bool rejected = false;
        try {
            vh::RouteSignatureStreamWriterConfig config;
            config.output_path = output_path;
            const vh::RouteSignatureStreamWriter writer(config);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        cleanup(output_path);
    }
    {
        bool rejected = false;
        try {
            vh::RouteSignatureStreamWriterConfig config;
            config.output_path = unique_path("stream_bad_config");
            config.checkpoint_interval_entries = 0;
            const vh::RouteSignatureStreamWriter writer(config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    return 0;
}
