#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

#include "visual_homing/frame.hpp"
#include "visual_homing/verification_package_writer.hpp"

namespace vh {

struct LiveVerificationCaptureConfig {
    VerificationPackageWriterConfig writer;
};

struct LiveVerificationFrameContext {
    bool health_ready = false;
    double route_progress = 0.0;
    double altitude_m = 0.0;
    double scale_ratio = 1.0;
    double yaw_rad = 0.0;
    bool has_local_pose = false;
    double local_x_m = 0.0;
    double local_y_m = 0.0;
    double local_z_m = 0.0;
    double local_yaw_rad = 0.0;
    double local_position_uncertainty_m = 0.0;
    double approach_radius_m = 0.0;
    VerificationCaptureMetadata publication_metadata{};
};

struct LiveVerificationFrameResult {
    VerificationKeyframeDecision decision;
    std::optional<VerificationPackagePublication> publication;
    double descriptor_latency_ms = 0.0;
    double selector_latency_ms = 0.0;
    double publication_latency_ms = 0.0;
};

struct LiveVerificationCaptureMetrics {
    std::uint64_t frames_observed = 0;
    std::uint64_t invalid_observations = 0;
    std::uint64_t capture_requests = 0;
    std::uint64_t publications = 0;
    std::uint64_t processing_failures = 0;
    std::uint64_t publication_failures = 0;
    std::uint64_t gates_published = 0;
    std::uint64_t output_bytes_published = 0;
    std::uint64_t package_files_checked = 0;
    double total_descriptor_latency_ms = 0.0;
    double maximum_descriptor_latency_ms = 0.0;
    double total_selector_latency_ms = 0.0;
    double maximum_selector_latency_ms = 0.0;
    double total_publication_latency_ms = 0.0;
    double maximum_publication_latency_ms = 0.0;
};

class LiveVerificationCaptureSession {
public:
    explicit LiveVerificationCaptureSession(LiveVerificationCaptureConfig config);
    ~LiveVerificationCaptureSession();

    LiveVerificationCaptureSession(const LiveVerificationCaptureSession&) = delete;
    LiveVerificationCaptureSession& operator=(const LiveVerificationCaptureSession&) = delete;
    LiveVerificationCaptureSession(LiveVerificationCaptureSession&&) = delete;
    LiveVerificationCaptureSession& operator=(LiveVerificationCaptureSession&&) = delete;

    LiveVerificationFrameResult observe(
        const Frame& native_frame,
        const LiveVerificationFrameContext& context);

    LiveVerificationCaptureMetrics metrics() const;
    const RoutePackageManifest& manifest() const;
    const std::filesystem::path& current_manifest_path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vh
