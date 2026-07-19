#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "visual_homing/route_package_manifest.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

struct RoutePackageBuilderConfig {
    std::filesystem::path package_directory;
    RoutePackageManifest manifest_template;
    std::string tracking_layer_id;
    std::uint32_t maximum_entries_per_chunk = 1024;
    std::uint32_t checkpoint_interval_entries = 64;
    bool recover_existing_recording = true;
};

struct RoutePackageRecoveryResult {
    bool completed_manifest_found = false;
    bool manifest_partial_promoted = false;
    std::uint32_t finalized_chunks_found = 0;
    std::uint32_t partial_chunks_recovered = 0;
    std::uint32_t empty_partials_archived = 0;
    std::uint64_t checkpointed_entries_recovered = 0;
    std::vector<std::filesystem::path> recovery_sources;
    RoutePackageManifest manifest;
};

struct RoutePackageBuilderMetrics {
    std::uint64_t entries_appended = 0;
    std::uint32_t chunks_finalized = 0;
    std::uint32_t chunks_recovered = 0;
    std::uint64_t checkpointed_entries_recovered = 0;
    bool manifest_finalized = false;
};

RoutePackageRecoveryResult recover_route_package_recording(
    const RoutePackageBuilderConfig& config);

class RoutePackageBuilder {
public:
    explicit RoutePackageBuilder(RoutePackageBuilderConfig config);
    ~RoutePackageBuilder();

    RoutePackageBuilder(const RoutePackageBuilder&) = delete;
    RoutePackageBuilder& operator=(const RoutePackageBuilder&) = delete;
    RoutePackageBuilder(RoutePackageBuilder&&) = delete;
    RoutePackageBuilder& operator=(RoutePackageBuilder&&) = delete;

    void append(const RouteSignatureEntry& entry);
    void finalize();

    const RoutePackageManifest& manifest() const;
    RoutePackageBuilderMetrics metrics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vh
