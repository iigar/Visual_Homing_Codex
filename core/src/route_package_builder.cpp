#include "visual_homing/route_package_builder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "visual_homing/artifact_digest.hpp"
#include "visual_homing/route_signature_stream_writer.hpp"

namespace vh {
namespace {

constexpr std::array<char, 4> route_magic = {'V', 'H', 'R', 'S'};
constexpr std::uint16_t route_header_size = 16;
constexpr std::uint8_t little_endian = 1;
constexpr std::uint64_t route_entry_header_size = 34;
constexpr std::uint32_t route_v1_maximum_entries = 100000;
constexpr std::uint32_t manifest_v1_maximum_chunks = 4096;
constexpr const char* manifest_filename = "route.vhrm";
constexpr const char* tracking_directory_name = "tracking";

struct InspectedChunk {
    std::uint64_t entry_count = 0;
    std::uint64_t first_frame_id = 0;
    std::uint64_t last_frame_id = 0;
    std::uint64_t first_timestamp_ns = 0;
    std::uint64_t last_timestamp_ns = 0;
    std::uint64_t committed_byte_size = route_header_size;
};

struct RecoveredChunks {
    std::vector<RouteChunkRecord> records;
    std::uint32_t partial_chunks_recovered = 0;
    std::uint32_t empty_partials_archived = 0;
    std::uint64_t checkpointed_entries_recovered = 0;
    std::vector<std::filesystem::path> recovery_sources;
    std::optional<std::uint64_t> last_frame_id;
    std::optional<std::uint64_t> last_timestamp_ns;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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
    throw std::runtime_error("Unknown tracking-layer pixel format");
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
        throw std::runtime_error("Unknown VHRS pixel format in route package chunk");
    }
}

std::uint8_t read_u8(std::istream& input) {
    const auto value = input.get();
    if (value == std::char_traits<char>::eof()) {
        throw std::runtime_error("Truncated VHRS route package chunk");
    }
    return static_cast<std::uint8_t>(value);
}

std::uint16_t read_u16(std::istream& input) {
    const auto low = read_u8(input);
    const auto high = read_u8(input);
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8U));
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
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

const RouteLayerRecord& tracking_layer(const RoutePackageBuilderConfig& config) {
    const auto found = std::find_if(
        config.manifest_template.layers.begin(),
        config.manifest_template.layers.end(),
        [&config](const RouteLayerRecord& layer) {
            return layer.id == config.tracking_layer_id;
        });
    if (found == config.manifest_template.layers.end()) {
        throw std::invalid_argument("Route package tracking layer ID is not present in the manifest template");
    }
    return *found;
}

std::string chunk_number(std::uint32_t index) {
    std::ostringstream output;
    output << std::setw(4) << std::setfill('0') << index;
    return output.str();
}

std::filesystem::path chunk_relative_path(std::uint32_t index) {
    return std::filesystem::path(tracking_directory_name)
        / ("chunk-" + chunk_number(index) + ".vhrs");
}

std::string chunk_id(std::uint32_t index) {
    return "tracking-chunk-" + chunk_number(index);
}

std::optional<std::uint32_t> parse_chunk_index(
    const std::filesystem::path& path,
    bool partial) {
    const auto filename = path.filename().string();
    const std::string suffix = partial ? ".vhrs.partial" : ".vhrs";
    constexpr std::size_t prefix_size = 6;
    constexpr std::size_t digit_count = 4;
    if (filename.size() != prefix_size + digit_count + suffix.size()
        || filename.substr(0, prefix_size) != "chunk-"
        || filename.substr(prefix_size + digit_count) != suffix) {
        return std::nullopt;
    }
    const auto digits = filename.substr(prefix_size, digit_count);
    if (!std::all_of(digits.begin(), digits.end(), [](char value) {
            return value >= '0' && value <= '9';
        })) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::stoul(digits));
}

void validate_builder_config(const RoutePackageBuilderConfig& config) {
    if (config.package_directory.empty()) {
        throw std::invalid_argument("Route package directory must not be empty");
    }
    if (config.tracking_layer_id.empty()) {
        throw std::invalid_argument("Route package tracking layer ID must not be empty");
    }
    if (config.maximum_entries_per_chunk == 0
        || config.maximum_entries_per_chunk > route_v1_maximum_entries) {
        throw std::invalid_argument("Route package maximum entries per chunk is outside VHRS v1 limits");
    }
    if (config.checkpoint_interval_entries == 0
        || config.checkpoint_interval_entries > config.maximum_entries_per_chunk) {
        throw std::invalid_argument("Route package checkpoint interval must be within the chunk entry limit");
    }
    if (!config.manifest_template.chunks.empty()
        || !config.manifest_template.search_indexes.empty()
        || !config.manifest_template.gates.empty()) {
        throw std::invalid_argument("Tracking package builder requires an artifact-free manifest template");
    }
    if (config.manifest_template.layers.size() != 1) {
        throw std::invalid_argument("Tracking package builder v1 requires exactly one tracking layer");
    }
    const auto& layer = tracking_layer(config);
    if (layer.role != RouteLayerRole::Tracking) {
        throw std::invalid_argument("Selected route package layer is not a tracking layer");
    }

    RoutePackageManifest validation_probe = config.manifest_template;
    validation_probe.chunks.push_back({
        .id = "builder-validation-probe",
        .layer_id = config.tracking_layer_id,
        .relative_path = std::filesystem::path(tracking_directory_name) / "probe.vhrs",
        .artifact_format_version = route_signature_format_version,
        .first_frame_id = 1,
        .last_frame_id = 1,
        .entry_count = 1,
        .byte_size = 1,
        .sha256 = std::string(64, '0'),
    });
    validate_route_package_manifest(validation_probe);

    const auto payload_size = static_cast<std::uint64_t>(layer.width)
        * static_cast<std::uint64_t>(layer.height)
        * bytes_per_pixel(layer.pixel_format);
    require(payload_size <= 64U * 1024U * 1024U, "Tracking layer payload exceeds VHRS v1 safety limit");
}

void validate_package_directory(const std::filesystem::path& directory) {
    if (std::filesystem::exists(directory)) {
        require(std::filesystem::is_directory(directory), "Route package path exists and is not a directory");
        require(!std::filesystem::is_symlink(std::filesystem::symlink_status(directory)),
            "Route package directory must not be a symlink");
    } else {
        std::filesystem::create_directories(directory);
    }
    const auto tracking_directory = directory / tracking_directory_name;
    if (std::filesystem::exists(tracking_directory)) {
        require(std::filesystem::is_directory(tracking_directory), "Route package tracking path is not a directory");
        require(!std::filesystem::is_symlink(std::filesystem::symlink_status(tracking_directory)),
            "Route package tracking directory must not be a symlink");
    } else {
        std::filesystem::create_directory(tracking_directory);
    }
}

InspectedChunk inspect_chunk(
    const std::filesystem::path& path,
    const RouteLayerRecord& layer,
    std::uint32_t maximum_entries,
    bool allow_uncheckpointed_tail) {
    const auto file_size = std::filesystem::file_size(path);
    const auto payload_size = static_cast<std::uint64_t>(layer.width)
        * static_cast<std::uint64_t>(layer.height)
        * bytes_per_pixel(layer.pixel_format);
    const auto maximum_file_size = static_cast<std::uint64_t>(route_header_size)
        + static_cast<std::uint64_t>(maximum_entries) * (route_entry_header_size + payload_size);
    require(file_size >= route_header_size, "VHRS route package chunk is smaller than its header");
    require(file_size <= maximum_file_size, "VHRS route package chunk exceeds configured size bound");

    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Could not open VHRS route package chunk: " + path.string());
    std::array<char, 4> magic{};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    require(magic == route_magic, "Invalid VHRS route package chunk magic");
    require(read_u16(input) == route_signature_format_version, "Unsupported VHRS route package chunk version");
    require(read_u16(input) == route_header_size, "Unsupported VHRS route package chunk header size");
    require(read_u8(input) == little_endian, "Unsupported VHRS route package chunk endian policy");
    (void)read_u8(input);
    (void)read_u16(input);
    const auto entry_count = read_u32(input);
    require(entry_count <= maximum_entries, "VHRS route package chunk entry count exceeds configured bound");

    InspectedChunk result;
    result.entry_count = entry_count;
    std::uint64_t previous_frame_id = 0;
    std::uint64_t previous_timestamp_ns = 0;
    for (std::uint32_t index = 0; index < entry_count; ++index) {
        const auto frame_id = read_u64(input);
        const auto timestamp_ns = read_u64(input);
        (void)read_u16(input);
        const auto heading_hint_rad = read_f32(input);
        const auto width = read_u16(input);
        const auto height = read_u16(input);
        const auto format = pixel_format_from_u8(read_u8(input));
        (void)read_u8(input);
        (void)read_u16(input);
        const auto stored_payload_size = read_u32(input);

        require(std::isfinite(heading_hint_rad), "VHRS route package chunk contains non-finite heading");
        require(width == layer.width && height == layer.height && format == layer.pixel_format,
            "VHRS route package chunk entry is incompatible with the tracking layer");
        require(stored_payload_size == payload_size,
            "VHRS route package chunk payload size does not match the tracking layer");
        if (index > 0) {
            require(frame_id > previous_frame_id, "VHRS route package chunk frame IDs are not strictly increasing");
            require(timestamp_ns >= previous_timestamp_ns, "VHRS route package chunk timestamps are not monotonic");
        }

        const auto payload_position = input.tellg();
        require(payload_position >= 0, "Could not determine VHRS route package payload position");
        const auto payload_end = static_cast<std::uint64_t>(payload_position) + stored_payload_size;
        require(payload_end <= file_size, "Truncated VHRS route package chunk payload");
        input.seekg(static_cast<std::streamoff>(stored_payload_size), std::ios::cur);
        require(static_cast<bool>(input), "Could not skip VHRS route package chunk payload");

        if (index == 0) {
            result.first_frame_id = frame_id;
            result.first_timestamp_ns = timestamp_ns;
        }
        result.last_frame_id = frame_id;
        result.last_timestamp_ns = timestamp_ns;
        previous_frame_id = frame_id;
        previous_timestamp_ns = timestamp_ns;
    }

    const auto committed_position = input.tellg();
    require(committed_position >= 0, "Could not determine committed VHRS route package size");
    result.committed_byte_size = static_cast<std::uint64_t>(committed_position);
    if (!allow_uncheckpointed_tail) {
        require(result.committed_byte_size == file_size, "Finalized VHRS route package chunk has trailing bytes");
    }
    return result;
}

std::filesystem::path next_recovery_source_path(const std::filesystem::path& partial_path) {
    for (std::uint32_t index = 0; index < 10000; ++index) {
        auto candidate = partial_path;
        candidate += ".recovery-source-" + chunk_number(index);
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Route package recovery source archive limit reached");
}

RouteChunkRecord make_chunk_record(
    const RoutePackageBuilderConfig& config,
    std::uint32_t index,
    const std::filesystem::path& final_path,
    const InspectedChunk& inspected) {
    require(inspected.entry_count > 0, "Route package cannot publish an empty tracking chunk");
    return {
        .id = chunk_id(index),
        .layer_id = config.tracking_layer_id,
        .relative_path = chunk_relative_path(index),
        .artifact_format_version = route_signature_format_version,
        .first_frame_id = inspected.first_frame_id,
        .last_frame_id = inspected.last_frame_id,
        .entry_count = inspected.entry_count,
        .byte_size = std::filesystem::file_size(final_path),
        .sha256 = sha256_file_hex(final_path),
    };
}

RecoveredChunks recover_chunks(const RoutePackageBuilderConfig& config) {
    const auto& layer = tracking_layer(config);
    const auto directory = config.package_directory / tracking_directory_name;
    std::map<std::uint32_t, std::filesystem::path> finals;
    std::map<std::uint32_t, std::filesystem::path> partials;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        require(!entry.is_symlink(), "Route package tracking directory contains a symlink");
        if (!entry.is_regular_file()) {
            throw std::runtime_error("Route package tracking directory contains a non-file entry");
        }
        if (const auto index = parse_chunk_index(entry.path(), false)) {
            require(index.value() < manifest_v1_maximum_chunks, "Route package chunk index exceeds VHRM v1 limit");
            require(finals.emplace(index.value(), entry.path()).second, "Duplicate finalized route package chunk index");
            continue;
        }
        if (const auto index = parse_chunk_index(entry.path(), true)) {
            require(index.value() < manifest_v1_maximum_chunks, "Route package partial chunk index exceeds VHRM v1 limit");
            require(partials.emplace(index.value(), entry.path()).second, "Duplicate partial route package chunk index");
            continue;
        }
        const auto filename = entry.path().filename().string();
        if (filename.find(".vhrs.partial.recovery-source-") != std::string::npos) {
            continue;
        }
        throw std::runtime_error("Unexpected file in route package tracking directory: " + filename);
    }

    require(partials.size() <= 1, "Route package contains more than one active partial chunk");
    for (std::uint32_t expected = 0; expected < finals.size(); ++expected) {
        require(finals.contains(expected), "Route package finalized chunks are not contiguous from zero");
    }

    RecoveredChunks result;
    if (!partials.empty()) {
        const auto [partial_index, partial_path] = *partials.begin();
        require(partial_index == finals.size(), "Route package partial chunk is not the next contiguous chunk");
        require(!finals.contains(partial_index), "Route package has both final and partial forms of one chunk");
        const auto inspected = inspect_chunk(
            partial_path, layer, config.maximum_entries_per_chunk, true);
        const auto archive_path = next_recovery_source_path(partial_path);
        if (inspected.entry_count == 0) {
            std::filesystem::rename(partial_path, archive_path);
            ++result.empty_partials_archived;
            result.recovery_sources.push_back(std::filesystem::relative(archive_path, config.package_directory));
        } else {
            std::filesystem::copy_file(partial_path, archive_path);
            std::filesystem::resize_file(partial_path, inspected.committed_byte_size);
            const auto final_path = config.package_directory / chunk_relative_path(partial_index);
            require(!std::filesystem::exists(final_path), "Route package final chunk appeared during recovery");
            std::filesystem::rename(partial_path, final_path);
            finals.emplace(partial_index, final_path);
            ++result.partial_chunks_recovered;
            result.checkpointed_entries_recovered += inspected.entry_count;
            result.recovery_sources.push_back(std::filesystem::relative(archive_path, config.package_directory));
        }
    }

    std::optional<std::uint64_t> previous_frame_id;
    std::optional<std::uint64_t> previous_timestamp_ns;
    for (const auto& [index, path] : finals) {
        const auto inspected = inspect_chunk(path, layer, config.maximum_entries_per_chunk, false);
        require(inspected.entry_count > 0, "Route package contains an empty finalized tracking chunk");
        if (previous_frame_id) {
            require(inspected.first_frame_id > *previous_frame_id,
                "Route package frame IDs are not strictly increasing across chunks");
            require(inspected.first_timestamp_ns >= *previous_timestamp_ns,
                "Route package timestamps are not monotonic across chunks");
        }
        result.records.push_back(make_chunk_record(config, index, path, inspected));
        previous_frame_id = inspected.last_frame_id;
        previous_timestamp_ns = inspected.last_timestamp_ns;
    }
    result.last_frame_id = previous_frame_id;
    result.last_timestamp_ns = previous_timestamp_ns;
    return result;
}

void verify_manifest_matches_template(
    const RoutePackageBuilderConfig& config,
    const RoutePackageManifest& manifest) {
    const auto& expected = config.manifest_template;
    const auto camera_matches = expected.camera.profile_id == manifest.camera.profile_id
        && expected.camera.sensor_type == manifest.camera.sensor_type
        && expected.camera.pixel_format == manifest.camera.pixel_format
        && expected.camera.capture_width == manifest.camera.capture_width
        && expected.camera.capture_height == manifest.camera.capture_height
        && expected.camera.horizontal_fov_rad == manifest.camera.horizontal_fov_rad
        && expected.camera.vertical_fov_rad == manifest.camera.vertical_fov_rad
        && expected.camera.camera_to_body_x_m == manifest.camera.camera_to_body_x_m
        && expected.camera.camera_to_body_y_m == manifest.camera.camera_to_body_y_m
        && expected.camera.camera_to_body_z_m == manifest.camera.camera_to_body_z_m
        && expected.camera.camera_to_body_roll_rad == manifest.camera.camera_to_body_roll_rad
        && expected.camera.camera_to_body_pitch_rad == manifest.camera.camera_to_body_pitch_rad
        && expected.camera.camera_to_body_yaw_rad == manifest.camera.camera_to_body_yaw_rad;
    require(expected.version == manifest.version
        && expected.route_id == manifest.route_id
        && expected.route_frame == manifest.route_frame
        && expected.local_frame_id == manifest.local_frame_id
        && expected.local_frame_revision == manifest.local_frame_revision
        && expected.local_frame_convention == manifest.local_frame_convention
        && camera_matches,
        "Recovered route package manifest identity does not match the builder template");
    require(manifest.layers.size() == 1 && expected.layers.size() == 1,
        "Recovered route package manifest has an unexpected layer count");
    const auto& expected_layer = expected.layers.front();
    const auto& recovered_layer = manifest.layers.front();
    require(expected_layer.id == recovered_layer.id
        && expected_layer.role == recovered_layer.role
        && expected_layer.camera_profile_id == recovered_layer.camera_profile_id
        && expected_layer.pixel_format == recovered_layer.pixel_format
        && expected_layer.width == recovered_layer.width
        && expected_layer.height == recovered_layer.height
        && expected_layer.minimum_altitude_m == recovered_layer.minimum_altitude_m
        && expected_layer.maximum_altitude_m == recovered_layer.maximum_altitude_m
        && recovered_layer.id == config.tracking_layer_id
        && manifest.search_indexes.empty()
        && manifest.gates.empty(),
        "Recovered route package manifest tracking layer does not match the builder config");
}

RoutePackageRecoveryResult recover_impl(const RoutePackageBuilderConfig& config) {
    validate_builder_config(config);
    validate_package_directory(config.package_directory);

    const auto final_manifest_path = config.package_directory / manifest_filename;
    auto partial_manifest_path = final_manifest_path;
    partial_manifest_path += ".partial";
    require(!(std::filesystem::exists(final_manifest_path)
        && std::filesystem::exists(partial_manifest_path)),
        "Route package contains both final and partial manifests");

    RoutePackageRecoveryResult result;
    if (std::filesystem::exists(final_manifest_path)) {
        result.manifest = read_route_package_manifest(final_manifest_path);
        verify_manifest_matches_template(config, result.manifest);
        const auto verification = verify_route_package_files(final_manifest_path, result.manifest);
        require(verification.passed, "Existing route package manifest failed artifact verification");
        result.completed_manifest_found = true;
        result.finalized_chunks_found = static_cast<std::uint32_t>(result.manifest.chunks.size());
        return result;
    }
    if (std::filesystem::exists(partial_manifest_path)) {
        result.manifest = read_route_package_manifest(partial_manifest_path);
        verify_manifest_matches_template(config, result.manifest);
        const auto verification = verify_route_package_files(partial_manifest_path, result.manifest);
        require(verification.passed, "Partial route package manifest failed artifact verification");
        std::filesystem::rename(partial_manifest_path, final_manifest_path);
        result.completed_manifest_found = true;
        result.manifest_partial_promoted = true;
        result.finalized_chunks_found = static_cast<std::uint32_t>(result.manifest.chunks.size());
        return result;
    }

    const auto recovered = recover_chunks(config);
    result.manifest = config.manifest_template;
    result.manifest.chunks = recovered.records;
    result.finalized_chunks_found = static_cast<std::uint32_t>(recovered.records.size());
    result.partial_chunks_recovered = recovered.partial_chunks_recovered;
    result.empty_partials_archived = recovered.empty_partials_archived;
    result.checkpointed_entries_recovered = recovered.checkpointed_entries_recovered;
    result.recovery_sources = recovered.recovery_sources;
    return result;
}

} // namespace

RoutePackageRecoveryResult recover_route_package_recording(
    const RoutePackageBuilderConfig& config) {
    return recover_impl(config);
}

struct RoutePackageBuilder::Impl {
    explicit Impl(RoutePackageBuilderConfig builder_config)
        : config(std::move(builder_config)) {
        validate_builder_config(config);
        validate_package_directory(config.package_directory);
        if (!config.recover_existing_recording) {
            const auto tracking_directory = config.package_directory / tracking_directory_name;
            auto partial_manifest_path = config.package_directory / manifest_filename;
            partial_manifest_path += ".partial";
            require(std::filesystem::is_empty(tracking_directory)
                && !std::filesystem::exists(config.package_directory / manifest_filename)
                && !std::filesystem::exists(partial_manifest_path),
                "Route package recording already contains artifacts and recovery is disabled");
            manifest = config.manifest_template;
            return;
        }

        const auto recovery = recover_impl(config);
        require(!recovery.completed_manifest_found, "Route package recording is already finalized");
        manifest = recovery.manifest;
        metrics.chunks_recovered = recovery.partial_chunks_recovered;
        metrics.checkpointed_entries_recovered = recovery.checkpointed_entries_recovered;
        next_chunk_index = static_cast<std::uint32_t>(manifest.chunks.size());
        if (!manifest.chunks.empty()) {
            last_frame_id = manifest.chunks.back().last_frame_id;
            const auto inspected = inspect_chunk(
                config.package_directory / manifest.chunks.back().relative_path,
                tracking_layer(config),
                config.maximum_entries_per_chunk,
                false);
            last_timestamp_ns = inspected.last_timestamp_ns;
        }
    }

    void open_chunk() {
        require(next_chunk_index < manifest_v1_maximum_chunks, "Route package chunk count exceeds VHRM v1 limit");
        const auto path = config.package_directory / chunk_relative_path(next_chunk_index);
        writer = std::make_unique<RouteSignatureStreamWriter>(RouteSignatureStreamWriterConfig{
            .output_path = path,
            .checkpoint_interval_entries = config.checkpoint_interval_entries,
        });
    }

    void finalize_chunk() {
        if (!writer) {
            return;
        }
        require(writer->entry_count() > 0, "Route package builder cannot finalize an empty chunk");
        const auto final_path = writer->output_path();
        writer->finalize();
        const auto inspected = inspect_chunk(
            final_path,
            tracking_layer(config),
            config.maximum_entries_per_chunk,
            false);
        manifest.chunks.push_back(make_chunk_record(config, next_chunk_index, final_path, inspected));
        ++metrics.chunks_finalized;
        ++next_chunk_index;
        writer.reset();
    }

    RoutePackageBuilderConfig config;
    RoutePackageManifest manifest;
    std::unique_ptr<RouteSignatureStreamWriter> writer;
    RoutePackageBuilderMetrics metrics;
    std::uint32_t next_chunk_index = 0;
    std::optional<std::uint64_t> last_frame_id;
    std::optional<std::uint64_t> last_timestamp_ns;
    bool finalized = false;
};

RoutePackageBuilder::RoutePackageBuilder(RoutePackageBuilderConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

RoutePackageBuilder::~RoutePackageBuilder() = default;

void RoutePackageBuilder::append(const RouteSignatureEntry& entry) {
    require(!impl_->finalized, "Route package builder is already finalized");
    require(!impl_->last_frame_id || entry.frame_id > *impl_->last_frame_id,
        "Route package frame IDs must be strictly increasing");
    require(!impl_->last_timestamp_ns || entry.timestamp_ns >= *impl_->last_timestamp_ns,
        "Route package timestamps must be monotonic");
    const auto& layer = tracking_layer(impl_->config);
    require(entry.width == layer.width && entry.height == layer.height && entry.format == layer.pixel_format,
        "Route package entry is incompatible with the tracking layer");
    require(std::isfinite(entry.heading_hint_rad), "Route package entry heading must be finite");

    if (impl_->writer
        && impl_->writer->entry_count() >= impl_->config.maximum_entries_per_chunk) {
        impl_->finalize_chunk();
    }
    if (!impl_->writer) {
        impl_->open_chunk();
    }
    impl_->writer->append(entry);
    impl_->last_frame_id = entry.frame_id;
    impl_->last_timestamp_ns = entry.timestamp_ns;
    ++impl_->metrics.entries_appended;
}

void RoutePackageBuilder::finalize() {
    if (impl_->finalized) {
        return;
    }
    impl_->finalize_chunk();
    require(!impl_->manifest.chunks.empty(), "Route package recording cannot finalize without entries");
    validate_route_package_manifest(impl_->manifest);
    write_route_package_manifest(
        impl_->config.package_directory / manifest_filename,
        impl_->manifest);
    const auto verification = verify_route_package_files(
        impl_->config.package_directory / manifest_filename,
        impl_->manifest);
    require(verification.passed, "Finalized route package failed artifact verification");
    impl_->finalized = true;
    impl_->metrics.manifest_finalized = true;
}

const RoutePackageManifest& RoutePackageBuilder::manifest() const {
    return impl_->manifest;
}

RoutePackageBuilderMetrics RoutePackageBuilder::metrics() const {
    return impl_->metrics;
}

} // namespace vh
