#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "visual_homing/route_descriptor_index.hpp"
#include "visual_homing/route_package_builder.hpp"
#include "visual_homing/route_package_manifest.hpp"
#include "visual_homing/route_signature.hpp"
#include "visual_homing/verification_package_writer.hpp"

namespace {

std::filesystem::path unique_directory(const std::string& label) {
    return std::filesystem::temp_directory_path()
        / ("visual_homing_verification_writer_" + label + "_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void cleanup(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

void write_byte(const std::filesystem::path& path, std::uint8_t value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    assert(output);
    output.put(static_cast<char>(value));
    assert(output);
}

vh::RouteSignatureEntry tracking_entry(std::uint64_t id, std::uint8_t value) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = id * 1'000U;
    result.altitude_band_m = 1;
    result.heading_hint_rad = 0.0F;
    result.width = 4;
    result.height = 4;
    result.format = vh::PixelFormat::Gray8;
    result.payload.assign(16, value);
    return result;
}

std::filesystem::path make_source_package(const std::filesystem::path& directory) {
    cleanup(directory);
    std::filesystem::create_directories(directory);

    vh::RoutePackageManifest manifest;
    manifest.route_id = "verification-writer-route";
    manifest.route_frame = "ROUTE_FRD";
    manifest.local_frame_id = "test-local-enu";
    manifest.local_frame_revision = "rev-1";
    manifest.local_frame_convention = "LOCAL_ENU";
    manifest.camera.profile_id = "ov9281-test";
    manifest.camera.sensor_type = vh::CameraSensorType::Visible;
    manifest.camera.pixel_format = vh::PixelFormat::Gray8;
    manifest.camera.capture_width = 8;
    manifest.camera.capture_height = 6;
    manifest.camera.horizontal_fov_rad = 1.5;
    manifest.camera.vertical_fov_rad = 1.0;
    manifest.layers = {{
        .id = "tracking-4x4",
        .role = vh::RouteLayerRole::Tracking,
        .camera_profile_id = "ov9281-test",
        .pixel_format = vh::PixelFormat::Gray8,
        .width = 4,
        .height = 4,
        .minimum_altitude_m = 0.5,
        .maximum_altitude_m = 5.0,
    }};

    vh::RoutePackageBuilder builder({
        .package_directory = directory,
        .manifest_template = manifest,
        .tracking_layer_id = "tracking-4x4",
        .maximum_entries_per_chunk = 2,
        .checkpoint_interval_entries = 1,
        .recover_existing_recording = true,
    });
    builder.append(tracking_entry(1, 10));
    builder.append(tracking_entry(2, 20));
    builder.append(tracking_entry(3, 30));
    builder.finalize();

    vh::RouteDescriptorIndexBuildConfig index_config;
    index_config.input_manifest_path = directory / "route.vhrm";
    index_config.output_manifest_path = directory / "route-indexed.vhrm";
    index_config.index_relative_path = "index/coarse-v1.vhix";
    index_config.grid_width = 2;
    index_config.grid_height = 2;
    (void)vh::build_route_descriptor_index_package(index_config);

    return index_config.output_manifest_path;
}

vh::VerificationKeyframeSelectorConfig selector_config() {
    return {
        .descriptor_dimensions = 4,
        .nominal_route_length_m = 100.0,
        .minimum_capture_interval_ns = 100,
        .maximum_capture_interval_ns = 1'000,
        .minimum_displacement_m = 10.0,
        .minimum_altitude_change_m = 2.0,
        .minimum_scale_log_change = std::log(2.0),
        .minimum_yaw_change_rad = 0.5,
        .minimum_scene_novelty = 0.2,
        .minimum_gate_separation_m = 20.0,
        .minimum_gate_scene_novelty = 0.3,
        .maximum_gate_position_uncertainty_m = 1.0,
        .minimum_gate_approach_margin_m = 2.0,
    };
}

vh::VerificationPackageWriterConfig writer_config(
    const std::filesystem::path& source_manifest,
    std::uint32_t maximum_keyframes = 4) {
    return {
        .source_manifest_path = source_manifest,
        .output_manifest_base_path = source_manifest.parent_path() / "route-verification.vhrm",
        .verification_layer = {
            .id = "verification-8x6",
            .role = vh::RouteLayerRole::Verification,
            .camera_profile_id = "ov9281-test",
            .pixel_format = vh::PixelFormat::Gray8,
            .width = 8,
            .height = 6,
            .minimum_altitude_m = 0.5,
            .maximum_altitude_m = 5.0,
        },
        .search_index_id = "coarse-index-v1",
        .verification_relative_directory = "verification",
        .maximum_keyframes = maximum_keyframes,
        .selector = selector_config(),
    };
}

vh::VerificationKeyframeObservation observation(
    std::uint64_t id,
    std::uint64_t timestamp_ns,
    double progress,
    std::vector<std::int8_t> descriptor) {
    return {
        .frame_id = id,
        .timestamp_ns = timestamp_ns,
        .health_ready = true,
        .route_progress = progress,
        .altitude_m = 1.0,
        .scale_ratio = 1.0,
        .yaw_rad = 0.0,
        .descriptor = std::move(descriptor),
    };
}

void add_local_pose(vh::VerificationKeyframeObservation& value, double x_m) {
    value.has_local_pose = true;
    value.local_x_m = x_m;
    value.local_y_m = 1.0;
    value.local_z_m = -1.0;
    value.local_yaw_rad = 0.25;
    value.local_position_uncertainty_m = 0.1;
    value.approach_radius_m = 3.0;
}

vh::RouteSignatureEntry native_entry(
    const vh::VerificationKeyframeObservation& value,
    std::uint8_t pixel) {
    vh::RouteSignatureEntry result;
    result.frame_id = value.frame_id;
    result.timestamp_ns = value.timestamp_ns;
    result.altitude_band_m = 1;
    result.heading_hint_rad = static_cast<float>(value.yaw_rad);
    result.width = 8;
    result.height = 6;
    result.format = vh::PixelFormat::Gray8;
    result.payload.assign(48, pixel);
    return result;
}

vh::VerificationCaptureMetadata gate_metadata() {
    return {
        .route_segment_id = 7,
        .allowed_directions =
            vh::route_gate_direction_forward | vh::route_gate_direction_reverse,
        .minimum_altitude_m = 0.8,
        .maximum_altitude_m = 2.0,
        .minimum_scale_ratio = 0.7,
        .maximum_scale_ratio = 1.4,
    };
}

} // namespace

int main() {
    {
        const auto directory = unique_directory("success");
        const auto source = make_source_package(directory);
        const auto source_before = vh::read_route_package_manifest(source);
        vh::VerificationPackageWriter writer(writer_config(source));

        auto first = observation(10, 10'000, 0.5, {0, 0, 0, 0});
        const auto first_decision = writer.evaluate(first);
        assert(first_decision.request_native_capture && !first_decision.gate_candidate);
        const auto first_publication = writer.publish(
            native_entry(first, 40), first, first_decision);
        assert(first_publication.publication_index == 1);
        assert(!first_publication.gate_published);
        assert(first_publication.manifest_path.filename() == "route-verification-0001.vhrm");
        assert(first_publication.chunk_path.filename() == "chunk-0000.vhrs");
        assert(std::filesystem::exists(first_publication.chunk_path));
        assert(vh::inspect_route_signature_file(first_publication.chunk_path).entry_count == 1);
        auto revision = vh::read_route_package_manifest(first_publication.manifest_path);
        assert(revision.chunks.size() == source_before.chunks.size() + 1);
        assert(revision.gates.empty());
        assert(vh::verify_route_package_files(first_publication.manifest_path, revision).passed);
        assert(vh::read_route_package_manifest(source).chunks.size() == source_before.chunks.size());

        auto second = observation(11, 10'100, 0.7, {127, 127, 127, 127});
        add_local_pose(second, 30.0);
        auto second_decision = writer.evaluate(second);
        assert(second_decision.request_native_capture && second_decision.gate_candidate);

        auto invalid_metadata = gate_metadata();
        invalid_metadata.minimum_altitude_m = 0.1;
        bool rejected_metadata = false;
        try {
            (void)writer.publish(
                native_entry(second, 80), second, second_decision, invalid_metadata);
        } catch (const std::runtime_error&) {
            rejected_metadata = true;
        }
        assert(rejected_metadata);
        assert(writer.metrics().keyframes_published == 1);
        assert(writer.evaluate(second).state_generation == second_decision.state_generation);
        assert(!std::filesystem::exists(directory / "verification/chunk-0001.vhrs"));

        const auto second_publication = writer.publish(
            native_entry(second, 80), second, second_decision, gate_metadata());
        assert(second_publication.publication_index == 2);
        assert(second_publication.gate_published && second_publication.gate_id == "gate-0000");
        assert(second_publication.manifest_path.filename() == "route-verification-0002.vhrm");
        revision = vh::read_route_package_manifest(second_publication.manifest_path);
        assert(revision.gates.size() == 1);
        assert(revision.gates[0].chunk_id == "verification-chunk-0001");
        assert(revision.gates[0].search_index_id == "coarse-index-v1");
        assert(revision.gates[0].route_segment_id == 7);
        assert(revision.gates[0].local_x_m == 30.0);
        assert(vh::verify_route_package_files(second_publication.manifest_path, revision).passed);

        auto third = observation(12, 10'200, 0.9, {0, 0, 0, 0});
        add_local_pose(third, 60.0);
        auto fourth = observation(13, 10'300, 1.0, {0, 0, 0, 0});
        add_local_pose(fourth, 90.0);
        const auto third_decision = writer.evaluate(third);
        const auto stale_fourth_decision = writer.evaluate(fourth);
        assert(third_decision.gate_candidate && stale_fourth_decision.gate_candidate);
        (void)writer.publish(native_entry(third, 120), third, third_decision, gate_metadata());
        bool rejected_stale = false;
        try {
            (void)writer.publish(
                native_entry(fourth, 120), fourth, stale_fourth_decision, gate_metadata());
        } catch (const std::runtime_error&) {
            rejected_stale = true;
        }
        assert(rejected_stale);
        assert(writer.metrics().keyframes_published == 3);
        assert(writer.metrics().gates_published == 2);
        assert(writer.metrics().manifest_revisions_published == 3);
        assert(writer.current_manifest_path().filename() == "route-verification-0003.vhrm");
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("collision");
        const auto source = make_source_package(directory);
        vh::VerificationPackageWriter writer(writer_config(source));
        const auto first = observation(10, 10'000, 0.5, {0, 0, 0, 0});
        const auto decision = writer.evaluate(first);

        auto wrong_identity = native_entry(first, 10);
        ++wrong_identity.frame_id;
        bool rejected_identity = false;
        try {
            (void)writer.publish(wrong_identity, first, decision);
        } catch (const std::runtime_error&) {
            rejected_identity = true;
        }
        assert(rejected_identity);

        const auto chunk_collision = directory / "verification/chunk-0000.vhrs";
        write_byte(chunk_collision, 1);
        bool rejected_chunk = false;
        try {
            (void)writer.publish(native_entry(first, 10), first, decision);
        } catch (const std::runtime_error&) {
            rejected_chunk = true;
        }
        assert(rejected_chunk);
        assert(writer.metrics().keyframes_published == 0);
        assert(writer.evaluate(first).state_generation == decision.state_generation);
        std::filesystem::remove(chunk_collision);

        const auto manifest_collision = directory / "route-verification-0001.vhrm";
        write_byte(manifest_collision, 2);
        bool rejected_manifest = false;
        try {
            (void)writer.publish(native_entry(first, 10), first, decision);
        } catch (const std::runtime_error&) {
            rejected_manifest = true;
        }
        assert(rejected_manifest);
        assert(!std::filesystem::exists(chunk_collision));
        assert(writer.metrics().keyframes_published == 0);
        std::filesystem::remove(manifest_collision);

        (void)writer.publish(native_entry(first, 10), first, decision);
        assert(writer.metrics().keyframes_published == 1);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("bound");
        const auto source = make_source_package(directory);
        vh::VerificationPackageWriter writer(writer_config(source, 1));
        const auto first = observation(10, 10'000, 0.5, {0, 0, 0, 0});
        const auto first_decision = writer.evaluate(first);
        (void)writer.publish(native_entry(first, 10), first, first_decision);

        const auto second = observation(11, 10'100, 0.7, {127, 127, 127, 127});
        const auto second_decision = writer.evaluate(second);
        bool rejected_bound = false;
        try {
            (void)writer.publish(native_entry(second, 20), second, second_decision);
        } catch (const std::runtime_error&) {
            rejected_bound = true;
        }
        assert(rejected_bound);
        assert(writer.metrics().keyframes_published == 1);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("symlink_escape");
        const auto outside = unique_directory("symlink_outside");
        const auto source = make_source_package(directory);
        cleanup(outside);
        std::filesystem::create_directories(outside);
        std::error_code symlink_error;
        std::filesystem::create_directory_symlink(
            outside,
            directory / "verification",
            symlink_error);
        if (!symlink_error) {
            vh::VerificationPackageWriter writer(writer_config(source));
            const auto first = observation(10, 10'000, 0.5, {0, 0, 0, 0});
            const auto decision = writer.evaluate(first);
            bool rejected_escape = false;
            try {
                (void)writer.publish(native_entry(first, 10), first, decision);
            } catch (const std::runtime_error&) {
                rejected_escape = true;
            }
            assert(rejected_escape);
            assert(writer.metrics().keyframes_published == 0);
            assert(!std::filesystem::exists(outside / "chunk-0000.vhrs"));
        }
        cleanup(directory);
        cleanup(outside);
    }
    {
        const auto directory = unique_directory("bad_source");
        const auto source = make_source_package(directory);
        write_byte(directory / "index/coarse-v1.vhix", 0xff);
        bool rejected_source = false;
        try {
            vh::VerificationPackageWriter writer(writer_config(source));
        } catch (const std::invalid_argument&) {
            rejected_source = true;
        }
        assert(rejected_source);
        cleanup(directory);
    }
    {
        const auto directory = unique_directory("bad_output_parent");
        const auto source = make_source_package(directory);
        auto bad = writer_config(source);
        bad.output_manifest_base_path = directory.parent_path() / "route-verification.vhrm";
        bool rejected_parent = false;
        try {
            vh::VerificationPackageWriter writer(bad);
        } catch (const std::invalid_argument&) {
            rejected_parent = true;
        }
        assert(rejected_parent);
        cleanup(directory);
    }
    return 0;
}
