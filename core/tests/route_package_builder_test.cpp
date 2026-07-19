#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "visual_homing/route_package_builder.hpp"
#include "visual_homing/route_package_manifest.hpp"
#include "visual_homing/route_signature.hpp"
#include "visual_homing/route_signature_stream_writer.hpp"

namespace {

std::filesystem::path unique_directory(const std::string& label) {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("visual_homing_" + label + "_" + std::to_string(suffix));
}

vh::RoutePackageBuilderConfig config(const std::filesystem::path& directory) {
    vh::RoutePackageManifest manifest;
    manifest.route_id = "builder-route-01";
    manifest.route_frame = "ROUTE_FRD";
    manifest.camera.profile_id = "ov9281-160-wide";
    manifest.camera.sensor_type = vh::CameraSensorType::Visible;
    manifest.camera.pixel_format = vh::PixelFormat::Gray8;
    manifest.camera.capture_width = 1280;
    manifest.camera.capture_height = 800;
    manifest.camera.horizontal_fov_rad = 2.79;
    manifest.camera.vertical_fov_rad = 1.75;
    manifest.layers = {{
        .id = "tracking-4x3",
        .role = vh::RouteLayerRole::Tracking,
        .camera_profile_id = "ov9281-160-wide",
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 4,
        .height = 3,
        .minimum_altitude_m = 0.4,
        .maximum_altitude_m = 5.0,
    }};
    return {
        .package_directory = directory,
        .manifest_template = std::move(manifest),
        .tracking_layer_id = "tracking-4x3",
        .maximum_entries_per_chunk = 2,
        .checkpoint_interval_entries = 1,
        .recover_existing_recording = true,
    };
}

vh::RouteSignatureEntry entry(std::uint64_t id, std::uint8_t value) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = id * 1'000'000U;
    result.altitude_band_m = 1;
    result.heading_hint_rad = static_cast<float>(id) * 0.01F;
    result.width = 4;
    result.height = 3;
    result.format = vh::PixelFormat::Gray8;
    result.payload.assign(12, value);
    return result;
}

void cleanup(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

void append_byte(const std::filesystem::path& path, std::uint8_t value) {
    std::ofstream output(path, std::ios::binary | std::ios::app);
    assert(output);
    output.put(static_cast<char>(value));
    assert(output);
}

} // namespace

int main() {
    {
        const auto directory = unique_directory("package_builder_rotation");
        cleanup(directory);
        auto builder_config = config(directory);
        vh::RoutePackageBuilder builder(builder_config);
        for (std::uint64_t id = 1; id <= 5; ++id) {
            builder.append(entry(id, static_cast<std::uint8_t>(id)));
        }
        builder.finalize();
        builder.finalize();

        const auto manifest_path = directory / "route.vhrm";
        assert(std::filesystem::exists(manifest_path));
        const auto manifest = vh::read_route_package_manifest(manifest_path);
        assert(manifest.chunks.size() == 3);
        assert(manifest.chunks[0].entry_count == 2);
        assert(manifest.chunks[1].entry_count == 2);
        assert(manifest.chunks[2].entry_count == 1);
        assert(manifest.chunks[0].relative_path == std::filesystem::path("tracking/chunk-0000.vhrs"));
        assert(manifest.chunks[2].last_frame_id == 5);
        assert(vh::verify_route_package_files(manifest_path, manifest).passed);
        const auto metrics = builder.metrics();
        assert(metrics.entries_appended == 5);
        assert(metrics.chunks_finalized == 3);
        assert(metrics.manifest_finalized);

        bool rejected_completed_resume = false;
        try {
            const vh::RoutePackageBuilder resumed(builder_config);
        } catch (const std::runtime_error&) {
            rejected_completed_resume = true;
        }
        assert(rejected_completed_resume);

        auto incompatible_config = builder_config;
        incompatible_config.manifest_template.camera.capture_width = 640;
        bool rejected_incompatible_manifest = false;
        try {
            (void)vh::recover_route_package_recording(incompatible_config);
        } catch (const std::runtime_error&) {
            rejected_incompatible_manifest = true;
        }
        assert(rejected_incompatible_manifest);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_recovery");
        cleanup(directory);
        auto builder_config = config(directory);
        builder_config.maximum_entries_per_chunk = 5;
        builder_config.checkpoint_interval_entries = 2;
        {
            vh::RoutePackageBuilder interrupted(builder_config);
            interrupted.append(entry(1, 11));
            interrupted.append(entry(2, 22));
            interrupted.append(entry(3, 33));
        }
        const auto partial_path = directory / "tracking/chunk-0000.vhrs.partial";
        assert(std::filesystem::exists(partial_path));

        const auto recovery = vh::recover_route_package_recording(builder_config);
        assert(!recovery.completed_manifest_found);
        assert(recovery.partial_chunks_recovered == 1);
        assert(recovery.checkpointed_entries_recovered == 2);
        assert(recovery.finalized_chunks_found == 1);
        assert(recovery.manifest.chunks.size() == 1);
        assert(recovery.manifest.chunks[0].entry_count == 2);
        assert(recovery.recovery_sources.size() == 1);
        assert(std::filesystem::exists(directory / recovery.recovery_sources[0]));
        assert(!std::filesystem::exists(partial_path));

        vh::RoutePackageBuilder resumed(builder_config);
        resumed.append(entry(4, 44));
        resumed.append(entry(5, 55));
        resumed.finalize();
        const auto manifest = vh::read_route_package_manifest(directory / "route.vhrm");
        assert(manifest.chunks.size() == 2);
        assert(manifest.chunks[0].first_frame_id == 1);
        assert(manifest.chunks[0].last_frame_id == 2);
        assert(manifest.chunks[1].first_frame_id == 4);
        assert(manifest.chunks[1].last_frame_id == 5);
        assert(resumed.metrics().chunks_recovered == 0);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_direct_resume");
        cleanup(directory);
        auto builder_config = config(directory);
        builder_config.maximum_entries_per_chunk = 5;
        builder_config.checkpoint_interval_entries = 2;
        {
            vh::RoutePackageBuilder interrupted(builder_config);
            interrupted.append(entry(1, 11));
            interrupted.append(entry(2, 22));
            interrupted.append(entry(3, 33));
        }
        vh::RoutePackageBuilder resumed(builder_config);
        assert(resumed.metrics().chunks_recovered == 1);
        assert(resumed.metrics().checkpointed_entries_recovered == 2);
        resumed.append(entry(4, 44));
        resumed.finalize();
        assert(resumed.manifest().chunks.size() == 2);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_empty_recovery");
        cleanup(directory);
        auto builder_config = config(directory);
        {
            std::filesystem::create_directories(directory / "tracking");
            const vh::RouteSignatureStreamWriter interrupted({
                .output_path = directory / "tracking/chunk-0000.vhrs",
                .checkpoint_interval_entries = 1,
            });
        }
        const auto recovery = vh::recover_route_package_recording(builder_config);
        assert(recovery.empty_partials_archived == 1);
        assert(recovery.recovery_sources.size() == 1);
        assert(std::filesystem::exists(directory / recovery.recovery_sources[0]));
        vh::RoutePackageBuilder builder(builder_config);
        builder.append(entry(1, 1));
        builder.finalize();
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_manifest_promotion");
        cleanup(directory);
        auto builder_config = config(directory);
        {
            vh::RoutePackageBuilder builder(builder_config);
            builder.append(entry(1, 1));
            builder.finalize();
        }
        const auto final_manifest = directory / "route.vhrm";
        auto partial_manifest = final_manifest;
        partial_manifest += ".partial";
        std::filesystem::rename(final_manifest, partial_manifest);
        const auto recovery = vh::recover_route_package_recording(builder_config);
        assert(recovery.completed_manifest_found);
        assert(recovery.manifest_partial_promoted);
        assert(std::filesystem::exists(final_manifest));
        assert(!std::filesystem::exists(partial_manifest));
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_uncheckpointed_tail");
        cleanup(directory);
        auto builder_config = config(directory);
        builder_config.maximum_entries_per_chunk = 5;
        builder_config.checkpoint_interval_entries = 2;
        {
            vh::RoutePackageBuilder interrupted(builder_config);
            interrupted.append(entry(1, 1));
            interrupted.append(entry(2, 2));
        }
        const auto partial = directory / "tracking/chunk-0000.vhrs.partial";
        append_byte(partial, 0x7f);
        const auto recovery = vh::recover_route_package_recording(builder_config);
        assert(recovery.partial_chunks_recovered == 1);
        assert(recovery.checkpointed_entries_recovered == 2);
        assert(!std::filesystem::exists(partial));
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_corrupt_partial");
        cleanup(directory);
        auto builder_config = config(directory);
        builder_config.maximum_entries_per_chunk = 5;
        builder_config.checkpoint_interval_entries = 2;
        {
            vh::RoutePackageBuilder interrupted(builder_config);
            interrupted.append(entry(1, 1));
            interrupted.append(entry(2, 2));
        }
        const auto partial = directory / "tracking/chunk-0000.vhrs.partial";
        std::filesystem::resize_file(partial, std::filesystem::file_size(partial) - 1);
        bool rejected = false;
        try {
            (void)vh::recover_route_package_recording(builder_config);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        assert(std::filesystem::exists(partial));
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_final_trailing_bytes");
        cleanup(directory);
        auto builder_config = config(directory);
        std::filesystem::create_directories(directory / "tracking");
        const auto final_chunk = directory / "tracking/chunk-0000.vhrs";
        {
            vh::RouteSignatureStreamWriter writer({
                .output_path = final_chunk,
                .checkpoint_interval_entries = 1,
            });
            writer.append(entry(1, 1));
            writer.finalize();
        }
        append_byte(final_chunk, 0x7f);
        bool rejected = false;
        try {
            (void)vh::recover_route_package_recording(builder_config);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_noncontiguous");
        cleanup(directory);
        auto builder_config = config(directory);
        std::filesystem::create_directories(directory / "tracking");
        {
            vh::RouteSignatureStreamWriter writer({
                .output_path = directory / "tracking/chunk-0001.vhrs",
                .checkpoint_interval_entries = 1,
            });
            writer.append(entry(1, 1));
            writer.finalize();
        }
        bool rejected = false;
        try {
            (void)vh::recover_route_package_recording(builder_config);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_cross_chunk_order");
        cleanup(directory);
        auto builder_config = config(directory);
        std::filesystem::create_directories(directory / "tracking");
        {
            vh::RouteSignatureStreamWriter first({
                .output_path = directory / "tracking/chunk-0000.vhrs",
                .checkpoint_interval_entries = 1,
            });
            first.append(entry(2, 2));
            first.finalize();
            vh::RouteSignatureStreamWriter second({
                .output_path = directory / "tracking/chunk-0001.vhrs",
                .checkpoint_interval_entries = 1,
            });
            second.append(entry(1, 1));
            second.finalize();
        }
        bool rejected = false;
        try {
            (void)vh::recover_route_package_recording(builder_config);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_final_partial_collision");
        cleanup(directory);
        auto builder_config = config(directory);
        std::filesystem::create_directories(directory / "tracking");
        const auto final_chunk = directory / "tracking/chunk-0000.vhrs";
        {
            vh::RouteSignatureStreamWriter writer({
                .output_path = final_chunk,
                .checkpoint_interval_entries = 1,
            });
            writer.append(entry(1, 1));
            writer.finalize();
        }
        std::filesystem::copy_file(
            final_chunk,
            directory / "tracking/chunk-0000.vhrs.partial");
        bool rejected = false;
        try {
            (void)vh::recover_route_package_recording(builder_config);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_invalid_entry");
        cleanup(directory);
        auto builder_config = config(directory);
        vh::RoutePackageBuilder builder(builder_config);
        auto invalid = entry(1, 1);
        invalid.width = 5;
        bool rejected = false;
        try {
            builder.append(invalid);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        assert(!std::filesystem::exists(directory / "tracking/chunk-0000.vhrs.partial"));
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("package_builder_bad_config");
        cleanup(directory);
        auto builder_config = config(directory);
        builder_config.checkpoint_interval_entries = 3;
        bool rejected = false;
        try {
            const vh::RoutePackageBuilder builder(builder_config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        cleanup(directory);
    }
    return 0;
}
