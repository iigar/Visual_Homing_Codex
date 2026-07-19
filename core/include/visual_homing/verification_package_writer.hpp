#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "visual_homing/route_package_manifest.hpp"
#include "visual_homing/route_signature.hpp"
#include "visual_homing/verification_keyframe_selector.hpp"

namespace vh {

struct VerificationPackageWriterConfig {
    std::filesystem::path source_manifest_path;
    std::filesystem::path output_manifest_base_path;
    RouteLayerRecord verification_layer;
    std::string search_index_id;
    std::filesystem::path verification_relative_directory = "verification";
    std::uint32_t maximum_keyframes = 4096;
    VerificationKeyframeSelectorConfig selector;
};

struct VerificationCaptureMetadata {
    std::uint32_t route_segment_id = 0;
    std::uint8_t allowed_directions =
        route_gate_direction_forward | route_gate_direction_reverse;
    double minimum_altitude_m = 0.0;
    double maximum_altitude_m = 0.0;
    double minimum_scale_ratio = 1.0;
    double maximum_scale_ratio = 1.0;
};

struct VerificationPackagePublication {
    std::uint32_t publication_index = 0;
    std::filesystem::path manifest_path;
    std::filesystem::path chunk_path;
    std::string chunk_id;
    bool gate_published = false;
    std::string gate_id;
    std::uint64_t package_files_checked = 0;
};

struct VerificationPackageWriterMetrics {
    std::uint32_t keyframes_published = 0;
    std::uint32_t gates_published = 0;
    std::uint32_t manifest_revisions_published = 0;
    std::uint64_t package_files_checked = 0;
};

class VerificationPackageWriter {
public:
    explicit VerificationPackageWriter(VerificationPackageWriterConfig config);
    ~VerificationPackageWriter();

    VerificationPackageWriter(const VerificationPackageWriter&) = delete;
    VerificationPackageWriter& operator=(const VerificationPackageWriter&) = delete;
    VerificationPackageWriter(VerificationPackageWriter&&) = delete;
    VerificationPackageWriter& operator=(VerificationPackageWriter&&) = delete;

    VerificationKeyframeDecision evaluate(
        const VerificationKeyframeObservation& observation) const;
    VerificationPackagePublication publish(
        const RouteSignatureEntry& native_entry,
        const VerificationKeyframeObservation& observation,
        const VerificationKeyframeDecision& decision,
        const VerificationCaptureMetadata& metadata = {});

    const RoutePackageManifest& manifest() const;
    const std::filesystem::path& current_manifest_path() const;
    VerificationPackageWriterMetrics metrics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vh
