#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/route_descriptor_index.hpp"
#include "visual_homing/route_package_builder.hpp"

namespace {

std::filesystem::path unique_directory(const std::string& label) {
    return std::filesystem::temp_directory_path()
        / ("visual_homing_" + label + "_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void cleanup(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

vh::RouteSignatureEntry entry(std::uint64_t id, std::uint8_t offset = 0) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = id * 1'000'000U;
    result.width = 4;
    result.height = 4;
    result.format = vh::PixelFormat::Gray8;
    result.payload = {
        static_cast<std::uint8_t>(10 + offset), static_cast<std::uint8_t>(10 + offset),
        static_cast<std::uint8_t>(30 + offset), static_cast<std::uint8_t>(30 + offset),
        static_cast<std::uint8_t>(10 + offset), static_cast<std::uint8_t>(10 + offset),
        static_cast<std::uint8_t>(30 + offset), static_cast<std::uint8_t>(30 + offset),
        static_cast<std::uint8_t>(50 + offset), static_cast<std::uint8_t>(50 + offset),
        static_cast<std::uint8_t>(70 + offset), static_cast<std::uint8_t>(70 + offset),
        static_cast<std::uint8_t>(50 + offset), static_cast<std::uint8_t>(50 + offset),
        static_cast<std::uint8_t>(70 + offset), static_cast<std::uint8_t>(70 + offset),
    };
    return result;
}

vh::RoutePackageBuilderConfig package_config(const std::filesystem::path& directory) {
    vh::RoutePackageManifest manifest;
    manifest.route_id = "descriptor-route";
    manifest.camera.profile_id = "ov9281-test";
    manifest.camera.sensor_type = vh::CameraSensorType::Visible;
    manifest.camera.pixel_format = vh::PixelFormat::Gray8;
    manifest.camera.capture_width = 4;
    manifest.camera.capture_height = 4;
    manifest.camera.horizontal_fov_rad = 1.0;
    manifest.camera.vertical_fov_rad = 1.0;
    manifest.layers = {{
        .id = "tracking-4x4",
        .role = vh::RouteLayerRole::Tracking,
        .camera_profile_id = "ov9281-test",
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 4,
        .height = 4,
        .minimum_altitude_m = 0.5,
        .maximum_altitude_m = 3.0,
    }};
    return {
        .package_directory = directory,
        .manifest_template = std::move(manifest),
        .tracking_layer_id = "tracking-4x4",
        .maximum_entries_per_chunk = 2,
        .checkpoint_interval_entries = 1,
    };
}

std::vector<std::uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void append_byte(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::app);
    assert(output);
    output.put(static_cast<char>(0x7f));
}

} // namespace

int main() {
    const auto first_descriptor = vh::make_gray8_centered_block_mean_descriptor(entry(1), 2, 2);
    const auto brighter_descriptor = vh::make_gray8_centered_block_mean_descriptor(entry(1, 20), 2, 2);
    assert(first_descriptor == brighter_descriptor);
    assert((first_descriptor == std::vector<std::int8_t>{-30, -10, 10, 30}));

    {
        const auto directory = unique_directory("descriptor_roundtrip");
        cleanup(directory);
        std::filesystem::create_directories(directory);
        vh::RouteDescriptorIndex index{
            .source_width = 4,
            .source_height = 4,
            .grid_width = 2,
            .grid_height = 2,
            .entries = {
                {.frame_id = 1, .chunk_index = 0, .entry_index = 0, .descriptor = first_descriptor},
                {.frame_id = 3, .chunk_index = 1, .entry_index = 0, .descriptor = {1, 2, 3, 4}},
            },
        };
        const auto first = directory / "first.vhix";
        const auto second = directory / "second.vhix";
        vh::write_route_descriptor_index(first, index);
        const auto loaded = vh::read_route_descriptor_index(first);
        assert(loaded.entries.size() == 2);
        assert(loaded.entries[0].descriptor == first_descriptor);
        vh::write_route_descriptor_index(second, loaded);
        assert(bytes(first) == bytes(second));

        append_byte(first);
        bool rejected_trailing = false;
        try {
            (void)vh::read_route_descriptor_index(first);
        } catch (const std::runtime_error&) {
            rejected_trailing = true;
        }
        assert(rejected_trailing);

        auto invalid_order = index;
        invalid_order.entries[1].frame_id = 1;
        bool rejected_order = false;
        try {
            vh::validate_route_descriptor_index(invalid_order);
        } catch (const std::runtime_error&) {
            rejected_order = true;
        }
        assert(rejected_order);

        auto empty = index;
        empty.entries.clear();
        bool rejected_empty = false;
        try {
            vh::validate_route_descriptor_index(empty);
        } catch (const std::runtime_error&) {
            rejected_empty = true;
        }
        assert(rejected_empty);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("descriptor_package");
        cleanup(directory);
        auto builder_config = package_config(directory);
        {
            vh::RoutePackageBuilder builder(builder_config);
            for (std::uint64_t id = 1; id <= 5; ++id) {
                builder.append(entry(id, static_cast<std::uint8_t>(id)));
            }
            builder.finalize();
        }
        const vh::RouteDescriptorIndexBuildConfig build_config{
            .input_manifest_path = directory / "route.vhrm",
            .output_manifest_path = directory / "route-indexed.vhrm",
            .index_relative_path = "index/coarse-v1.vhix",
            .index_id = "coarse-index-v1",
            .descriptor_layer_id = "global-descriptor-v1",
            .grid_width = 2,
            .grid_height = 2,
            .sample_stride = 2,
            .maximum_chunk_bytes = 1024 * 1024,
            .maximum_index_entries = 10,
        };
        const auto result = vh::build_route_descriptor_index_package(build_config);
        assert(result.source_entries_scanned == 5);
        assert(result.descriptors_written == 3);
        assert(result.manifest.layers.size() == 2);
        assert(result.manifest.search_indexes.size() == 1);
        assert(result.index_record.item_count == 3);
        assert(result.index_record.descriptor_dimensions == 4);
        assert(vh::verify_route_package_files(build_config.output_manifest_path, result.manifest).passed);
        const auto index = vh::read_route_descriptor_index(directory / build_config.index_relative_path);
        assert(index.entries.size() == 3);
        assert(index.entries[0].frame_id == 1 && index.entries[0].chunk_index == 0);
        assert(index.entries[1].frame_id == 3 && index.entries[1].chunk_index == 1);
        assert(index.entries[2].frame_id == 5 && index.entries[2].chunk_index == 2);

        bool rejected_overwrite = false;
        try {
            (void)vh::build_route_descriptor_index_package(build_config);
        } catch (const std::runtime_error&) {
            rejected_overwrite = true;
        }
        assert(rejected_overwrite);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("descriptor_bad_path");
        cleanup(directory);
        auto builder_config = package_config(directory);
        {
            vh::RoutePackageBuilder builder(builder_config);
            builder.append(entry(1));
            builder.finalize();
        }
        vh::RouteDescriptorIndexBuildConfig build_config;
        build_config.input_manifest_path = directory / "route.vhrm";
        build_config.output_manifest_path = directory / "route-indexed.vhrm";
        build_config.index_relative_path = "../escape.vhix";
        build_config.grid_width = 2;
        build_config.grid_height = 2;
        bool rejected = false;
        try {
            (void)vh::build_route_descriptor_index_package(build_config);
        } catch (const std::exception&) {
            rejected = true;
        }
        assert(rejected);
        assert(!std::filesystem::exists(directory.parent_path() / "escape.vhix"));
        cleanup(directory);
    }
    return 0;
}
