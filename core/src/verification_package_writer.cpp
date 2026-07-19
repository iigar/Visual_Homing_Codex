#include "visual_homing/verification_package_writer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "visual_homing/artifact_digest.hpp"
#include "visual_homing/route_signature_stream_writer.hpp"

namespace vh {
namespace {

constexpr std::uint32_t manifest_v1_maximum_chunks = 4096;
constexpr std::uint32_t manifest_v1_maximum_layers = 16;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_config(bool condition, const char* message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

std::filesystem::path effective_parent(const std::filesystem::path& path) {
    return std::filesystem::weakly_canonical(
        path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path());
}

bool is_safe_relative_directory(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return false;
    }
    const auto generic = path.generic_string();
    if (generic.empty() || generic.find('\\') != std::string::npos
        || generic.find("//") != std::string::npos) {
        return false;
    }
    for (const auto& component : path) {
        if (component.empty() || component == "." || component == "..") {
            return false;
        }
    }
    return true;
}

bool path_has_prefix(
    const std::filesystem::path& candidate,
    const std::filesystem::path& base) {
    auto candidate_it = candidate.begin();
    for (auto base_it = base.begin(); base_it != base.end(); ++base_it, ++candidate_it) {
        if (candidate_it == candidate.end() || *candidate_it != *base_it) {
            return false;
        }
    }
    return true;
}

bool relative_directory_stays_within_base(
    const std::filesystem::path& relative,
    const std::filesystem::path& base) {
    auto current = base;
    for (const auto& component : relative) {
        current /= component;
        if (!std::filesystem::exists(current)) {
            return true;
        }
        current = std::filesystem::canonical(current);
        if (!path_has_prefix(current, base)) {
            return false;
        }
    }
    return true;
}

std::string four_digit_number(std::uint32_t value) {
    std::ostringstream output;
    output << std::setw(4) << std::setfill('0') << value;
    return output.str();
}

std::filesystem::path manifest_revision_path(
    const std::filesystem::path& base,
    std::uint32_t one_based_index) {
    return base.parent_path()
        / (base.stem().string() + "-" + four_digit_number(one_based_index)
            + base.extension().string());
}

std::filesystem::path partial_path(const std::filesystem::path& path) {
    auto result = path;
    result += ".partial";
    return result;
}

const RouteSearchIndexRecord& find_search_index(
    const RoutePackageManifest& manifest,
    const std::string& id) {
    const auto found = std::find_if(
        manifest.search_indexes.begin(),
        manifest.search_indexes.end(),
        [&id](const RouteSearchIndexRecord& index) { return index.id == id; });
    require_config(found != manifest.search_indexes.end(),
        "Verification search index ID is not present in the source manifest");
    return *found;
}

bool matching_decision(
    const VerificationKeyframeDecision& expected,
    const VerificationKeyframeDecision& supplied) {
    return expected.valid == supplied.valid
        && expected.request_native_capture == supplied.request_native_capture
        && expected.gate_candidate == supplied.gate_candidate
        && expected.trigger_mask == supplied.trigger_mask
        && expected.frame_id == supplied.frame_id
        && expected.timestamp_ns == supplied.timestamp_ns
        && expected.state_generation == supplied.state_generation;
}

void validate_gate_metadata(
    const VerificationCaptureMetadata& metadata,
    const RouteLayerRecord& layer) {
    const auto valid_direction_bits =
        route_gate_direction_forward | route_gate_direction_reverse;
    require((metadata.allowed_directions & valid_direction_bits) != 0
            && (metadata.allowed_directions & ~valid_direction_bits) == 0,
        "Verification gate directions are invalid");
    require(std::isfinite(metadata.minimum_altitude_m)
            && std::isfinite(metadata.maximum_altitude_m)
            && metadata.minimum_altitude_m >= layer.minimum_altitude_m
            && metadata.maximum_altitude_m <= layer.maximum_altitude_m
            && metadata.maximum_altitude_m >= metadata.minimum_altitude_m,
        "Verification gate altitude envelope is invalid");
    require(std::isfinite(metadata.minimum_scale_ratio)
            && std::isfinite(metadata.maximum_scale_ratio)
            && metadata.minimum_scale_ratio > 0.0
            && metadata.maximum_scale_ratio >= metadata.minimum_scale_ratio,
        "Verification gate scale envelope is invalid");
}

RouteGateKeyframeRecord make_gate_record(
    const std::string& id,
    const std::string& verification_layer_id,
    const std::string& chunk_id,
    const std::string& search_index_id,
    const VerificationKeyframeObservation& observation,
    const VerificationCaptureMetadata& metadata) {
    return {
        .id = id,
        .verification_layer_id = verification_layer_id,
        .chunk_id = chunk_id,
        .search_index_id = search_index_id,
        .frame_id = observation.frame_id,
        .route_segment_id = metadata.route_segment_id,
        .route_progress = observation.route_progress,
        .allowed_directions = metadata.allowed_directions,
        .has_local_pose = observation.has_local_pose,
        .local_x_m = observation.local_x_m,
        .local_y_m = observation.local_y_m,
        .local_z_m = observation.local_z_m,
        .local_yaw_rad = observation.local_yaw_rad,
        .local_position_uncertainty_m = observation.local_position_uncertainty_m,
        .approach_radius_m = observation.approach_radius_m,
        .minimum_altitude_m = metadata.minimum_altitude_m,
        .maximum_altitude_m = metadata.maximum_altitude_m,
        .minimum_scale_ratio = metadata.minimum_scale_ratio,
        .maximum_scale_ratio = metadata.maximum_scale_ratio,
    };
}

} // namespace

struct VerificationPackageWriter::Impl {
    explicit Impl(VerificationPackageWriterConfig writer_config)
        : config(std::move(writer_config)), selector(config.selector) {
        require_config(!config.source_manifest_path.empty(),
            "Verification source manifest path must not be empty");
        require_config(!config.output_manifest_base_path.empty(),
            "Verification output manifest base path must not be empty");
        require_config(config.output_manifest_base_path.extension() == ".vhrm"
                && !config.output_manifest_base_path.stem().empty(),
            "Verification output manifest base path must have a non-empty .vhrm filename");
        require_config(effective_parent(config.source_manifest_path)
                == effective_parent(config.output_manifest_base_path),
            "Verification source and output manifests must share the same package directory");
        require_config(!config.verification_layer.id.empty(),
            "Verification layer ID must not be empty");
        require_config(!config.search_index_id.empty(),
            "Verification search index ID must not be empty");
        require_config(is_safe_relative_directory(config.verification_relative_directory),
            "Verification artifact directory must be a safe relative path");
        require_config(config.maximum_keyframes > 0
                && config.maximum_keyframes <= manifest_v1_maximum_chunks,
            "Verification keyframe bound is outside VHRM v1 limits");
        require_config(std::filesystem::exists(config.source_manifest_path)
                && std::filesystem::is_regular_file(config.source_manifest_path),
            "Verification source manifest does not exist as a regular file");

        manifest = read_route_package_manifest(config.source_manifest_path);
        const auto source_verification = verify_route_package_files(
            config.source_manifest_path,
            manifest);
        require_config(source_verification.passed,
            "Verification source package failed artifact verification");
        metrics.package_files_checked = source_verification.files_checked;

        const auto& layer = config.verification_layer;
        require_config(layer.role == RouteLayerRole::Verification,
            "Configured native layer does not have the verification role");
        require_config(layer.camera_profile_id == manifest.camera.profile_id,
            "Verification layer camera profile does not match the source package");
        require_config(std::isfinite(layer.minimum_altitude_m)
                && std::isfinite(layer.maximum_altitude_m)
                && layer.minimum_altitude_m >= 0.0
                && layer.maximum_altitude_m >= layer.minimum_altitude_m,
            "Verification layer altitude envelope is invalid");
        require_config(layer.pixel_format == PixelFormat::Gray8
                && manifest.camera.pixel_format == PixelFormat::Gray8,
            "Verification writer currently requires Gray8 camera and layer formats");
        require_config(layer.width == manifest.camera.capture_width
                && layer.height == manifest.camera.capture_height,
            "Verification layer must use the native camera capture dimensions");
        (void)find_search_index(manifest, config.search_index_id);
        require_config(std::none_of(
                manifest.layers.begin(),
                manifest.layers.end(),
                [this](const RouteLayerRecord& existing) {
                    return existing.id == config.verification_layer.id
                        || existing.role == RouteLayerRole::Verification;
                }),
            "Verification writer v1 requires a source package without a verification layer");
        require_config(std::none_of(
                manifest.chunks.begin(),
                manifest.chunks.end(),
                [this](const RouteChunkRecord& chunk) {
                    return chunk.layer_id == config.verification_layer.id;
                }),
            "Verification writer v1 requires a source package without verification chunks");
        require_config(manifest.gates.empty(),
            "Verification writer v1 requires a source package without existing gates");
        require_config(manifest.chunks.size() + config.maximum_keyframes
                <= manifest_v1_maximum_chunks,
            "Verification keyframe bound would exceed the VHRM v1 chunk limit");
        require_config(manifest.layers.size() < manifest_v1_maximum_layers,
            "Verification layer would exceed the VHRM v1 layer limit");
    }

    const RouteLayerRecord& verification_layer() const {
        return config.verification_layer;
    }

    VerificationPackageWriterConfig config;
    VerificationKeyframeSelector selector;
    RoutePackageManifest manifest;
    std::filesystem::path current_manifest;
    VerificationPackageWriterMetrics metrics;
};

VerificationPackageWriter::VerificationPackageWriter(
    VerificationPackageWriterConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

VerificationPackageWriter::~VerificationPackageWriter() = default;

VerificationKeyframeDecision VerificationPackageWriter::evaluate(
    const VerificationKeyframeObservation& observation) const {
    return impl_->selector.evaluate(observation);
}

VerificationPackagePublication VerificationPackageWriter::publish(
    const RouteSignatureEntry& native_entry,
    const VerificationKeyframeObservation& observation,
    const VerificationKeyframeDecision& decision,
    const VerificationCaptureMetadata& metadata) {
    require(impl_->metrics.keyframes_published < impl_->config.maximum_keyframes,
        "Verification keyframe publication bound reached");
    const auto expected = impl_->selector.evaluate(observation);
    require(expected.valid && expected.request_native_capture
            && matching_decision(expected, decision),
        "Verification publication decision is stale, invalid, or does not match selector state");
    require(native_entry.frame_id == observation.frame_id
            && native_entry.timestamp_ns == observation.timestamp_ns,
        "Verification native entry does not match observation identity");
    const auto& layer = impl_->verification_layer();
    require(native_entry.width == layer.width
            && native_entry.height == layer.height
            && native_entry.format == layer.pixel_format,
        "Verification native entry is incompatible with the native-resolution layer");
    if (decision.gate_candidate) {
        require(observation.has_local_pose,
            "Verification gate candidate requires a local pose");
        validate_gate_metadata(metadata, layer);
    }

    const auto zero_based_index = impl_->metrics.keyframes_published;
    const auto one_based_index = zero_based_index + 1U;
    const auto number = four_digit_number(zero_based_index);
    const auto relative_chunk_path = impl_->config.verification_relative_directory
        / ("chunk-" + number + ".vhrs");
    const auto chunk_path = impl_->config.source_manifest_path.parent_path()
        / relative_chunk_path;
    const auto revision_path = manifest_revision_path(
        impl_->config.output_manifest_base_path,
        one_based_index);
    require(!std::filesystem::exists(chunk_path)
            && !std::filesystem::exists(partial_path(chunk_path)),
        "Verification chunk output or partial path already exists");
    require(!std::filesystem::exists(revision_path)
            && !std::filesystem::exists(partial_path(revision_path)),
        "Verification manifest revision or partial path already exists");
    const auto package_directory = effective_parent(impl_->config.source_manifest_path);
    require(relative_directory_stays_within_base(
                impl_->config.verification_relative_directory,
                package_directory),
        "Verification chunk directory resolves outside the package");

    std::filesystem::create_directories(chunk_path.parent_path());
    RouteSignatureStreamWriter chunk_writer({
        .output_path = chunk_path,
        .checkpoint_interval_entries = 1,
    });
    chunk_writer.append(native_entry);
    chunk_writer.finalize();

    const auto summary = inspect_route_signature_file(chunk_path);
    require(summary.entry_count == 1
            && summary.first_frame_id == native_entry.frame_id
            && summary.last_frame_id == native_entry.frame_id
            && summary.width == layer.width
            && summary.height == layer.height
            && summary.uniform_dimensions
            && summary.uniform_payload_size
            && summary.all_gray8,
        "Published verification chunk failed native-entry inspection");

    const auto chunk_id = "verification-chunk-" + number;
    require(std::none_of(
                impl_->manifest.chunks.begin(),
                impl_->manifest.chunks.end(),
                [&chunk_id, &relative_chunk_path](const RouteChunkRecord& chunk) {
                    return chunk.id == chunk_id || chunk.relative_path == relative_chunk_path;
                }),
        "Verification chunk ID or relative path collides with the source package");
    RoutePackageManifest candidate = impl_->manifest;
    if (impl_->metrics.keyframes_published == 0) {
        candidate.layers.push_back(impl_->config.verification_layer);
    }
    candidate.chunks.push_back({
        .id = chunk_id,
        .layer_id = impl_->config.verification_layer.id,
        .relative_path = relative_chunk_path,
        .artifact_format_version = route_signature_format_version,
        .first_frame_id = native_entry.frame_id,
        .last_frame_id = native_entry.frame_id,
        .entry_count = 1,
        .byte_size = std::filesystem::file_size(chunk_path),
        .sha256 = sha256_file_hex(chunk_path),
    });

    std::string gate_id;
    if (decision.gate_candidate) {
        gate_id = "gate-" + four_digit_number(impl_->metrics.gates_published);
        candidate.gates.push_back(make_gate_record(
            gate_id,
            impl_->config.verification_layer.id,
            chunk_id,
            impl_->config.search_index_id,
            observation,
            metadata));
    }
    validate_route_package_manifest(candidate);
    write_route_package_manifest(revision_path, candidate);
    const auto package_verification = verify_route_package_files(
        revision_path,
        candidate);
    require(package_verification.passed,
        "Published verification manifest revision failed package verification");

    impl_->selector.commit(observation, decision);
    impl_->manifest = std::move(candidate);
    impl_->current_manifest = revision_path;
    ++impl_->metrics.keyframes_published;
    ++impl_->metrics.manifest_revisions_published;
    if (decision.gate_candidate) {
        ++impl_->metrics.gates_published;
    }
    impl_->metrics.package_files_checked += package_verification.files_checked;

    return {
        .publication_index = one_based_index,
        .manifest_path = revision_path,
        .chunk_path = chunk_path,
        .chunk_id = chunk_id,
        .gate_published = decision.gate_candidate,
        .gate_id = std::move(gate_id),
        .package_files_checked = package_verification.files_checked,
    };
}

const RoutePackageManifest& VerificationPackageWriter::manifest() const {
    return impl_->manifest;
}

const std::filesystem::path& VerificationPackageWriter::current_manifest_path() const {
    return impl_->current_manifest;
}

VerificationPackageWriterMetrics VerificationPackageWriter::metrics() const {
    return impl_->metrics;
}

} // namespace vh
