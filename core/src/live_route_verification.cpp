#include "visual_homing/live_route_verification.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "visual_homing/time.hpp"

namespace vh {
namespace {

void require_config(bool condition, const char* message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

bool finite_unit_interval(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool fresh_at(
    Timestamp source_timestamp,
    Timestamp evaluation_timestamp,
    double maximum_age_ms) {
    if (source_timestamp == Timestamp{} || evaluation_timestamp == Timestamp{}) {
        return false;
    }
    const auto age_ms = milliseconds_between(source_timestamp, evaluation_timestamp);
    return std::isfinite(age_ms) && age_ms >= 0.0 && age_ms <= maximum_age_ms;
}

bool complete_local_frame_contract(const LiveRouteVerificationProducerConfig& config) {
    return !config.local_frame_id.empty()
        && !config.local_frame_revision.empty()
        && !config.local_frame_convention.empty();
}

LiveRouteVerificationStatus mapped_status(VerificationSubmissionStatus status) {
    switch (status) {
    case VerificationSubmissionStatus::Accepted:
        return LiveRouteVerificationStatus::Accepted;
    case VerificationSubmissionStatus::Backpressure:
        return LiveRouteVerificationStatus::Backpressure;
    case VerificationSubmissionStatus::NotRunning:
        return LiveRouteVerificationStatus::NotRunning;
    case VerificationSubmissionStatus::Failed:
        return LiveRouteVerificationStatus::Failed;
    }
    return LiveRouteVerificationStatus::Failed;
}

} // namespace

const char* live_route_verification_status_name(LiveRouteVerificationStatus status) {
    switch (status) {
    case LiveRouteVerificationStatus::Accepted:
        return "accepted";
    case LiveRouteVerificationStatus::Backpressure:
        return "backpressure";
    case LiveRouteVerificationStatus::Rejected:
        return "rejected";
    case LiveRouteVerificationStatus::NotRunning:
        return "not_running";
    case LiveRouteVerificationStatus::Failed:
        return "failed";
    }
    return "unknown";
}

LiveRouteVerificationProducer::LiveRouteVerificationProducer(
    LiveRouteVerificationProducerConfig config,
    BoundedVerificationPublisher& publisher)
    : config_(std::move(config)), publisher_(publisher) {
    require_config(finite_unit_interval(config_.minimum_match_confidence),
        "Live route verification minimum match confidence must be within zero to one");
    require_config(std::isfinite(config_.maximum_frame_context_age_ms)
            && config_.maximum_frame_context_age_ms >= 0.0,
        "Live route verification maximum frame context age must be finite and non-negative");
    require_config(std::isfinite(config_.maximum_scalar_age_ms)
            && config_.maximum_scalar_age_ms >= 0.0,
        "Live route verification maximum scalar age must be finite and non-negative");
    require_config(std::isfinite(config_.maximum_local_pose_age_ms)
            && config_.maximum_local_pose_age_ms >= 0.0,
        "Live route verification maximum local pose age must be finite and non-negative");
    require_config(std::isfinite(config_.match_health_confidence_tolerance)
            && config_.match_health_confidence_tolerance >= 0.0,
        "Live route verification confidence tolerance must be finite and non-negative");
    const auto local_frame_fields = static_cast<unsigned>(!config_.local_frame_id.empty())
        + static_cast<unsigned>(!config_.local_frame_revision.empty())
        + static_cast<unsigned>(!config_.local_frame_convention.empty());
    require_config(local_frame_fields == 0U || local_frame_fields == 3U,
        "Live route verification local frame contract must be empty or complete");
}

std::optional<LiveVerificationFrameContext> LiveRouteVerificationProducer::compose(
    const Frame& native_frame,
    const LiveRouteVerificationObservation& observation,
    std::string& rejection_reason) const {
    const auto reject = [&rejection_reason](const char* reason)
        -> std::optional<LiveVerificationFrameContext> {
        rejection_reason = reason;
        return std::nullopt;
    };

    if (!observation.match.valid) {
        return reject("route_match_invalid");
    }
    if (observation.match.timestamp != native_frame.timestamp) {
        return reject("route_match_frame_mismatch");
    }
    if (!finite_unit_interval(observation.match.confidence)
        || observation.match.confidence < config_.minimum_match_confidence) {
        return reject("route_match_confidence_low");
    }
    if (!observation.tracked_route_progress
        || !finite_unit_interval(*observation.tracked_route_progress)) {
        return reject("tracked_route_progress_invalid");
    }
    if (!fresh_at(
            native_frame.timestamp,
            observation.health.timestamp,
            config_.maximum_frame_context_age_ms)) {
        return reject("health_frame_context_stale");
    }
    if (!observation.health.camera_ok || !observation.health.navigation_ok
        || (config_.require_mavlink_health && !observation.health.mavlink_ok)) {
        return reject("health_not_ready");
    }
    if (!std::isfinite(observation.health.route_match_confidence)
        || std::abs(observation.health.route_match_confidence - observation.match.confidence)
            > config_.match_health_confidence_tolerance) {
        return reject("health_match_confidence_mismatch");
    }

    const auto validate_scalar = [&](const LiveRouteVerificationScalarObservation& scalar,
                                     const char* invalid_reason,
                                     bool positive) -> bool {
        if (!scalar.valid || !std::isfinite(scalar.value)
            || (positive && scalar.value <= 0.0)
            || !fresh_at(
                scalar.timestamp,
                observation.health.timestamp,
                config_.maximum_scalar_age_ms)) {
            rejection_reason = invalid_reason;
            return false;
        }
        return true;
    };
    if (!validate_scalar(observation.altitude, "altitude_observation_invalid", false)
        || !validate_scalar(observation.scale_ratio, "scale_observation_invalid", true)
        || !validate_scalar(observation.yaw, "yaw_observation_invalid", false)) {
        return std::nullopt;
    }

    LiveVerificationFrameContext context;
    context.health_ready = true;
    context.route_progress = *observation.tracked_route_progress;
    context.altitude_m = observation.altitude.value;
    context.scale_ratio = observation.scale_ratio.value;
    context.yaw_rad = observation.yaw.value;
    context.publication_metadata = observation.publication_metadata;

    if (observation.local_pose) {
        const auto& pose = *observation.local_pose;
        if (!complete_local_frame_contract(config_)) {
            return reject("local_pose_frame_not_configured");
        }
        if (!pose.valid) {
            return reject("local_pose_invalid");
        }
        if (pose.frame_id != native_frame.id) {
            return reject("local_pose_frame_mismatch");
        }
        if (!fresh_at(
                pose.timestamp,
                observation.health.timestamp,
                config_.maximum_local_pose_age_ms)) {
            return reject("local_pose_stale");
        }
        if (pose.frame_id_name != config_.local_frame_id
            || pose.frame_revision != config_.local_frame_revision
            || pose.frame_convention != config_.local_frame_convention) {
            return reject("local_pose_coordinate_frame_mismatch");
        }
        if (!std::isfinite(pose.x_m) || !std::isfinite(pose.y_m)
            || !std::isfinite(pose.z_m) || !std::isfinite(pose.yaw_rad)
            || !std::isfinite(pose.position_uncertainty_m)
            || !std::isfinite(pose.approach_radius_m)
            || pose.position_uncertainty_m < 0.0
            || pose.approach_radius_m < 0.0
            || pose.approach_radius_m < pose.position_uncertainty_m) {
            return reject("local_pose_quality_invalid");
        }
        context.has_local_pose = true;
        context.local_x_m = pose.x_m;
        context.local_y_m = pose.y_m;
        context.local_z_m = pose.z_m;
        context.local_yaw_rad = pose.yaw_rad;
        context.local_position_uncertainty_m = pose.position_uncertainty_m;
        context.approach_radius_m = pose.approach_radius_m;
    }

    return context;
}

LiveRouteVerificationResult LiveRouteVerificationProducer::submit(
    const Frame& native_frame,
    const LiveRouteVerificationObservation& observation) {
    ++metrics_.observations;
    LiveRouteVerificationResult result;
    std::string rejection_reason;
    result.context = compose(native_frame, observation, rejection_reason);
    if (!result.context) {
        ++metrics_.rejected;
        metrics_.last_rejection_reason = rejection_reason;
        result.status = LiveRouteVerificationStatus::Rejected;
        result.reason = std::move(rejection_reason);
        return result;
    }

    if (result.context->has_local_pose) {
        ++metrics_.local_pose_contexts;
    } else {
        ++metrics_.progress_only_contexts;
    }
    result.submission = publisher_.submit(native_frame, *result.context);
    result.status = mapped_status(result.submission->status);
    result.reason = result.submission->reason;
    switch (result.status) {
    case LiveRouteVerificationStatus::Accepted:
        ++metrics_.accepted;
        break;
    case LiveRouteVerificationStatus::Backpressure:
        ++metrics_.backpressure;
        break;
    case LiveRouteVerificationStatus::NotRunning:
        ++metrics_.not_running;
        break;
    case LiveRouteVerificationStatus::Failed:
        ++metrics_.failed;
        break;
    case LiveRouteVerificationStatus::Rejected:
        break;
    }
    return result;
}

LiveRouteVerificationProducerMetrics LiveRouteVerificationProducer::metrics() const {
    return metrics_;
}

} // namespace vh
