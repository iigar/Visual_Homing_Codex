#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "visual_homing/bounded_verification_publisher.hpp"
#include "visual_homing/health.hpp"
#include "visual_homing/route.hpp"

namespace vh {

struct LiveRouteVerificationScalarObservation {
    bool valid = false;
    Timestamp timestamp{};
    double value = 0.0;
};

struct LiveRouteVerificationLocalPoseObservation {
    bool valid = false;
    std::uint64_t frame_id = 0;
    Timestamp timestamp{};
    std::string frame_id_name;
    std::string frame_revision;
    std::string frame_convention;
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    double yaw_rad = 0.0;
    double position_uncertainty_m = 0.0;
    double approach_radius_m = 0.0;
};

struct LiveRouteVerificationObservation {
    RouteMatch match{};
    std::optional<double> tracked_route_progress;
    HealthSnapshot health{};
    LiveRouteVerificationScalarObservation altitude{};
    LiveRouteVerificationScalarObservation scale_ratio{};
    LiveRouteVerificationScalarObservation yaw{};
    std::optional<LiveRouteVerificationLocalPoseObservation> local_pose;
    VerificationCaptureMetadata publication_metadata{};
};

struct LiveRouteVerificationProducerConfig {
    double minimum_match_confidence = 0.8;
    double maximum_frame_context_age_ms = 500.0;
    double maximum_scalar_age_ms = 500.0;
    double maximum_local_pose_age_ms = 500.0;
    double match_health_confidence_tolerance = 1.0e-6;
    bool require_mavlink_health = false;
    std::string local_frame_id;
    std::string local_frame_revision;
    std::string local_frame_convention;
};

enum class LiveRouteVerificationStatus {
    Accepted,
    Backpressure,
    Rejected,
    NotRunning,
    Failed,
};

const char* live_route_verification_status_name(LiveRouteVerificationStatus status);

struct LiveRouteVerificationResult {
    LiveRouteVerificationStatus status = LiveRouteVerificationStatus::Rejected;
    std::string reason;
    std::optional<LiveVerificationFrameContext> context;
    std::optional<VerificationSubmissionResult> submission;
};

struct LiveRouteVerificationProducerMetrics {
    std::uint64_t observations = 0;
    std::uint64_t rejected = 0;
    std::uint64_t progress_only_contexts = 0;
    std::uint64_t local_pose_contexts = 0;
    std::uint64_t accepted = 0;
    std::uint64_t backpressure = 0;
    std::uint64_t not_running = 0;
    std::uint64_t failed = 0;
    std::string last_rejection_reason;
};

class LiveRouteVerificationProducer {
public:
    LiveRouteVerificationProducer(
        LiveRouteVerificationProducerConfig config,
        BoundedVerificationPublisher& publisher);

    LiveRouteVerificationResult submit(
        const Frame& native_frame,
        const LiveRouteVerificationObservation& observation);

    LiveRouteVerificationProducerMetrics metrics() const;

private:
    std::optional<LiveVerificationFrameContext> compose(
        const Frame& native_frame,
        const LiveRouteVerificationObservation& observation,
        std::string& rejection_reason) const;

    LiveRouteVerificationProducerConfig config_;
    BoundedVerificationPublisher& publisher_;
    LiveRouteVerificationProducerMetrics metrics_;
};

} // namespace vh
