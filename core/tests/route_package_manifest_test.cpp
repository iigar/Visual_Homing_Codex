#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "visual_homing/artifact_digest.hpp"
#include "visual_homing/route_package_manifest.hpp"

namespace {

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    assert(output);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    assert(output);
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

vh::RoutePackageManifest valid_manifest(
    const std::filesystem::path& tracking_name,
    const std::filesystem::path& verification_name,
    const std::filesystem::path& index_name,
    const std::vector<std::uint8_t>& tracking_bytes,
    const std::vector<std::uint8_t>& verification_bytes,
    const std::vector<std::uint8_t>& index_bytes) {
    vh::RoutePackageManifest manifest;
    manifest.route_id = "warehouse-route-01";
    manifest.local_frame_id = "site-local-enu";
    manifest.local_frame_revision = "rev-20260719";
    manifest.local_frame_convention = "LOCAL_ENU";
    manifest.camera.profile_id = "ov9281-160-wide";
    manifest.camera.sensor_type = vh::CameraSensorType::Visible;
    manifest.camera.pixel_format = vh::PixelFormat::Gray8;
    manifest.camera.capture_width = 1280;
    manifest.camera.capture_height = 800;
    manifest.camera.horizontal_fov_rad = 2.79;
    manifest.camera.vertical_fov_rad = 1.75;
    manifest.camera.camera_to_body_z_m = 0.04;
    manifest.camera.camera_to_body_pitch_rad = 3.141592653589793;
    manifest.layers = {
        {
            .id = "tracking-160x100",
            .role = vh::RouteLayerRole::Tracking,
            .camera_profile_id = "ov9281-160-wide",
            .pixel_format = vh::PixelFormat::Gray8,
            .width = 160,
            .height = 100,
            .minimum_altitude_m = 0.4,
            .maximum_altitude_m = 5.0,
        },
        {
            .id = "global-descriptor-v1",
            .role = vh::RouteLayerRole::GlobalDescriptor,
            .camera_profile_id = "ov9281-160-wide",
            .pixel_format = vh::PixelFormat::Gray8,
            .width = 32,
            .height = 20,
            .minimum_altitude_m = 0.4,
            .maximum_altitude_m = 10.0,
        },
        {
            .id = "verification-1280x800",
            .role = vh::RouteLayerRole::Verification,
            .camera_profile_id = "ov9281-160-wide",
            .pixel_format = vh::PixelFormat::Gray8,
            .width = 1280,
            .height = 800,
            .minimum_altitude_m = 0.4,
            .maximum_altitude_m = 10.0,
        },
    };
    manifest.chunks = {
        {
            .id = "tracking-chunk-0001",
            .layer_id = "tracking-160x100",
            .relative_path = tracking_name.filename(),
            .first_frame_id = 1,
            .last_frame_id = 3,
            .entry_count = 3,
            .byte_size = tracking_bytes.size(),
            .sha256 = vh::sha256_hex(tracking_bytes),
        },
        {
            .id = "verification-chunk-0001",
            .layer_id = "verification-1280x800",
            .relative_path = verification_name.filename(),
            .first_frame_id = 10,
            .last_frame_id = 10,
            .entry_count = 1,
            .byte_size = verification_bytes.size(),
            .sha256 = vh::sha256_hex(verification_bytes),
        },
    };
    manifest.search_indexes = {
        {
            .id = "coarse-index-v1",
            .layer_id = "global-descriptor-v1",
            .relative_path = index_name.filename(),
            .descriptor_type = "gray8-grid-v1",
            .descriptor_dimensions = 64,
            .item_count = 3,
            .byte_size = index_bytes.size(),
            .sha256 = vh::sha256_hex(index_bytes),
        },
    };
    manifest.gates = {
        {
            .id = "gate-loading-bay",
            .verification_layer_id = "verification-1280x800",
            .chunk_id = "verification-chunk-0001",
            .search_index_id = "coarse-index-v1",
            .frame_id = 10,
            .route_segment_id = 2,
            .route_progress = 0.42,
            .allowed_directions = vh::route_gate_direction_forward | vh::route_gate_direction_reverse,
            .has_local_pose = true,
            .local_x_m = 12.5,
            .local_y_m = -3.0,
            .local_z_m = -1.2,
            .local_yaw_rad = 0.75,
            .local_position_uncertainty_m = 0.4,
            .approach_radius_m = 3.0,
            .minimum_altitude_m = 0.8,
            .maximum_altitude_m = 3.0,
            .minimum_scale_ratio = 0.6,
            .maximum_scale_ratio = 1.8,
        },
    };
    return manifest;
}

void remove_if_present(const std::filesystem::path& path) {
    std::filesystem::remove(path);
    auto partial = path;
    partial += ".partial";
    std::filesystem::remove(partial);
}

} // namespace

int main() {
    const std::string abc = "abc";
    const auto abc_bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size());
    assert(vh::sha256_hex(abc_bytes) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(vh::sha256_hex({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string multi_block = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    assert(vh::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(multi_block.data()), multi_block.size()))
        == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    assert(vh::is_sha256_hex(std::string(64, 'a')));
    assert(!vh::is_sha256_hex(std::string(64, 'A')));

    const auto base = std::filesystem::temp_directory_path();
    const auto manifest_path = base / "visual_homing_route_package_manifest_test.vhrm";
    const auto second_manifest_path = base / "visual_homing_route_package_manifest_test_copy.vhrm";
    const auto partial_collision_path = base / "visual_homing_route_package_manifest_partial_collision.vhrm";
    const auto tracking_path = base / "visual_homing_route_package_tracking_test.vhrs";
    const auto verification_path = base / "visual_homing_route_package_verification_test.vhrs";
    const auto index_path = base / "visual_homing_route_package_index_test.bin";
    remove_if_present(manifest_path);
    remove_if_present(second_manifest_path);
    remove_if_present(partial_collision_path);
    std::filesystem::remove(tracking_path);
    std::filesystem::remove(verification_path);
    std::filesystem::remove(index_path);

    const std::vector<std::uint8_t> tracking_bytes = {1, 2, 3, 4, 5, 6};
    const std::vector<std::uint8_t> verification_bytes = {10, 20, 30, 40};
    const std::vector<std::uint8_t> index_bytes = {9, 8, 7, 6, 5};
    write_bytes(tracking_path, tracking_bytes);
    write_bytes(verification_path, verification_bytes);
    write_bytes(index_path, index_bytes);

    const auto manifest = valid_manifest(
        tracking_path, verification_path, index_path,
        tracking_bytes, verification_bytes, index_bytes);
    vh::validate_route_package_manifest(manifest);
    vh::write_route_package_manifest(manifest_path, manifest);
    assert(std::filesystem::exists(manifest_path));
    auto manifest_partial = manifest_path;
    manifest_partial += ".partial";
    assert(!std::filesystem::exists(manifest_partial));

    const auto loaded = vh::read_route_package_manifest(manifest_path);
    assert(loaded.version == vh::route_package_manifest_format_version);
    assert(loaded.route_id == manifest.route_id);
    assert(loaded.route_frame == "ROUTE_FRD");
    assert(loaded.local_frame_id == "site-local-enu");
    assert(loaded.local_frame_convention == "LOCAL_ENU");
    assert(loaded.camera.capture_width == 1280);
    assert(loaded.camera.camera_to_body_z_m == 0.04);
    assert(loaded.camera.camera_to_body_pitch_rad == 3.141592653589793);
    assert(loaded.layers.size() == 3);
    assert(loaded.chunks.size() == 2);
    assert(loaded.chunks[0].artifact_format_version == 1);
    assert(loaded.search_indexes.size() == 1);
    assert(loaded.gates.size() == 1);
    assert(loaded.gates[0].id == "gate-loading-bay");
    assert(loaded.gates[0].has_local_pose);
    assert(loaded.gates[0].local_x_m == 12.5);
    assert(loaded.gates[0].route_progress == 0.42);
    assert(vh::to_string(loaded.layers[2].role) == "verification");

    vh::write_route_package_manifest(second_manifest_path, loaded);
    assert(read_bytes(manifest_path) == read_bytes(second_manifest_path));

    const auto verified = vh::verify_route_package_files(manifest_path, loaded);
    assert(verified.passed);
    assert(verified.files_checked == 3);
    assert(verified.errors.empty());
    assert(vh::sha256_file_hex(tracking_path) == loaded.chunks[0].sha256);

    write_bytes(verification_path, {11, 20, 30, 40});
    const auto corrupted = vh::verify_route_package_files(manifest_path, loaded);
    assert(!corrupted.passed);
    assert(corrupted.files_checked == 3);
    assert(corrupted.errors.size() == 1);
    assert(corrupted.errors[0].find("sha256_mismatch:") == 0);

    bool rejected_existing = false;
    try {
        vh::write_route_package_manifest(manifest_path, loaded);
    } catch (const std::runtime_error&) {
        rejected_existing = true;
    }
    assert(rejected_existing);

    auto partial_collision_file = partial_collision_path;
    partial_collision_file += ".partial";
    write_bytes(partial_collision_file, {1});
    bool rejected_partial = false;
    try {
        vh::write_route_package_manifest(partial_collision_path, loaded);
    } catch (const std::runtime_error&) {
        rejected_partial = true;
    }
    assert(rejected_partial);

    auto unsafe_path = loaded;
    unsafe_path.chunks[0].relative_path = "../escape.vhrs";
    bool rejected_path = false;
    try {
        vh::validate_route_package_manifest(unsafe_path);
    } catch (const std::invalid_argument&) {
        rejected_path = true;
    }
    assert(rejected_path);

    auto missing_local_frame = loaded;
    missing_local_frame.local_frame_id.clear();
    missing_local_frame.local_frame_revision.clear();
    missing_local_frame.local_frame_convention.clear();
    bool rejected_local_gate = false;
    try {
        vh::validate_route_package_manifest(missing_local_frame);
    } catch (const std::invalid_argument&) {
        rejected_local_gate = true;
    }
    assert(rejected_local_gate);

    auto invalid_local_convention = loaded;
    invalid_local_convention.local_frame_convention = "UNKNOWN_LOCAL";
    bool rejected_local_convention = false;
    try {
        vh::validate_route_package_manifest(invalid_local_convention);
    } catch (const std::invalid_argument&) {
        rejected_local_convention = true;
    }
    assert(rejected_local_convention);

    auto wrong_gate_layer = loaded;
    wrong_gate_layer.gates[0].verification_layer_id = "tracking-160x100";
    wrong_gate_layer.gates[0].chunk_id = "tracking-chunk-0001";
    wrong_gate_layer.gates[0].frame_id = 1;
    bool rejected_gate_layer = false;
    try {
        vh::validate_route_package_manifest(wrong_gate_layer);
    } catch (const std::invalid_argument&) {
        rejected_gate_layer = true;
    }
    assert(rejected_gate_layer);

    auto invalid_digest = loaded;
    invalid_digest.chunks[0].sha256 = std::string(64, 'A');
    bool rejected_digest = false;
    try {
        vh::validate_route_package_manifest(invalid_digest);
    } catch (const std::invalid_argument&) {
        rejected_digest = true;
    }
    assert(rejected_digest);

    auto invalid_chunk_version = loaded;
    invalid_chunk_version.chunks[0].artifact_format_version = 2;
    bool rejected_chunk_version = false;
    try {
        vh::validate_route_package_manifest(invalid_chunk_version);
    } catch (const std::invalid_argument&) {
        rejected_chunk_version = true;
    }
    assert(rejected_chunk_version);

    {
        std::ofstream append(manifest_path, std::ios::binary | std::ios::app);
        append.put('x');
    }
    bool rejected_trailing = false;
    try {
        (void)vh::read_route_package_manifest(manifest_path);
    } catch (const std::runtime_error&) {
        rejected_trailing = true;
    }
    assert(rejected_trailing);

    remove_if_present(manifest_path);
    remove_if_present(second_manifest_path);
    remove_if_present(partial_collision_path);
    std::filesystem::remove(tracking_path);
    std::filesystem::remove(verification_path);
    std::filesystem::remove(index_path);
    return 0;
}
