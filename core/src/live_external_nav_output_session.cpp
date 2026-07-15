#include "visual_homing/live_external_nav_output_session.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace vh {
namespace {

bool finite_number(const double value) {
    return std::isfinite(value);
}

bool estimate_ready(const ExternalNavEstimate& estimate) {
    return estimate.valid_for_fc &&
           estimate.reason == "valid" &&
           estimate.route_match_valid &&
           estimate.telemetry_fresh &&
           estimate.altitude_valid &&
           estimate.scale_known &&
           estimate.pose_frame == LocalCoordinateFrame::local_ned &&
           estimate.frame_alignment_known &&
           estimate.altitude_origin_aligned &&
           finite_number(estimate.x_m) &&
           finite_number(estimate.y_m) &&
           finite_number(estimate.z_m) &&
           finite_number(estimate.yaw_rad) &&
           finite_number(estimate.confidence) &&
           finite_number(estimate.route_progress) &&
           finite_number(estimate.relative_altitude_m);
}

} // namespace

LiveExternalNavOutputSession::LiveExternalNavOutputSession(
    LiveExternalNavOutputSessionConfig config,
    LiveExternalNavOutputAuditSink& audit,
    ExternalNavWriter& writer)
    : config_(std::move(config)),
      audit_(audit),
      writer_(writer) {
    if (!std::isfinite(config_.max_duration_ms) || config_.max_duration_ms < 0.0) {
        throw std::invalid_argument("LiveExternalNavOutputSession max_duration_ms must be finite and non-negative");
    }
}

bool LiveExternalNavOutputSession::start() {
    running_ = false;
    writer_started_ = false;
    if (!audit_.start(config_.run_id) || !audit_.ready()) {
        return false;
    }
    started_at_ = now();
    messages_sent_ = 0;
    running_ = true;
    started_once_ = true;
    return true;
}

void LiveExternalNavOutputSession::stop(const std::string& reason) {
    if (writer_started_) {
        writer_.stop();
        writer_started_ = false;
    }
    running_ = false;
    audit_.stop(reason);
}

LiveExternalNavOutputResult LiveExternalNavOutputSession::process(
    const LiveExternalNavOutputSnapshot& snapshot) {
    if (!running_) {
        if (!started_once_) {
            throw std::runtime_error("LiveExternalNavOutputSession process called before start");
        }
        return {false, false, "external_nav_output_session_stopped"};
    }

    if (config_.max_messages > 0 && messages_sent_ >= config_.max_messages) {
        LiveExternalNavOutputResult result{false, false, "max_message_count_reached"};
        record_or_stop(snapshot, result);
        stop("max_message_count_reached");
        return result;
    }

    if (config_.max_duration_ms > 0.0) {
        const auto elapsed_ms = milliseconds_between(started_at_, snapshot.now);
        if (!std::isfinite(elapsed_ms) || elapsed_ms < 0.0 || elapsed_ms > config_.max_duration_ms) {
            LiveExternalNavOutputResult result{false, false, "max_duration_reached"};
            record_or_stop(snapshot, result);
            stop("max_duration_reached");
            return result;
        }
    }

    auto result = evaluate(snapshot);
    if (!result.allowed) {
        record_or_stop(snapshot, result);
        return result;
    }

    if (!writer_started_) {
        if (!writer_.start()) {
            result = LiveExternalNavOutputResult{false, false, "writer_start_failed"};
            record_or_stop(snapshot, result);
            stop("writer_start_failed");
            return result;
        }
        writer_started_ = true;
    }

    try {
        writer_.send_vision_position_estimate(snapshot.estimate, snapshot.time_usec);
    } catch (const std::exception&) {
        result = LiveExternalNavOutputResult{false, false, "send_failed"};
        record_or_stop(snapshot, result);
        stop("send_failed");
        return result;
    }

    ++messages_sent_;
    result.sent = true;
    record_or_stop(snapshot, result);
    return result;
}

bool LiveExternalNavOutputSession::running() const {
    return running_;
}

LiveExternalNavOutputResult LiveExternalNavOutputSession::evaluate(
    const LiveExternalNavOutputSnapshot& snapshot) const {
    if (!config_.output_available) {
        return {false, false, "external_nav_output_unavailable"};
    }
    if (!config_.runtime_enabled) {
        return {false, false, "runtime_disabled"};
    }
    if (!config_.operator_confirmed) {
        return {false, false, "operator_not_confirmed"};
    }
    if (!config_.audit_log_enabled) {
        return {false, false, "audit_log_disabled"};
    }
    if (!audit_.ready()) {
        return {false, false, "audit_log_not_ready"};
    }
    if (!config_.single_writer) {
        return {false, false, "multiple_writers"};
    }
    if (!estimate_ready(snapshot.estimate)) {
        return {false, false, "external_nav_estimate_not_ready"};
    }
    return {true, false, "allowed"};
}

void LiveExternalNavOutputSession::record_or_stop(
    const LiveExternalNavOutputSnapshot& snapshot,
    const LiveExternalNavOutputResult& result) {
    try {
        audit_.record_estimate(snapshot, result);
    } catch (...) {
        stop("audit_record_failed");
        throw;
    }
}

} // namespace vh
