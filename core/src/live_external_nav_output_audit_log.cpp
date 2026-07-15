#include "visual_homing/live_external_nav_output_audit_log.hpp"

#include <stdexcept>
#include <utility>

namespace vh {

LiveExternalNavOutputAuditLog::LiveExternalNavOutputAuditLog(LiveExternalNavOutputAuditLogConfig config)
    : config_(std::move(config)) {}

bool LiveExternalNavOutputAuditLog::start(const std::string& run_id) {
    ready_ = false;
    if (output_.is_open()) {
        output_.close();
    }
    if (config_.path.empty()) {
        return false;
    }

    const auto parent = config_.path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const auto mode = std::ios::out | (config_.append ? std::ios::app : std::ios::trunc);
    output_.open(config_.path, mode);
    if (!output_.is_open() || !output_.good()) {
        return false;
    }

    output_ << "external_nav_output_audit event=start run_id=" << run_id << "\n";
    output_.flush();
    ready_ = output_.good();
    if (!ready_) {
        output_.close();
    }
    return ready_;
}

void LiveExternalNavOutputAuditLog::stop(const std::string& reason) {
    if (!ready_) {
        return;
    }
    output_ << "external_nav_output_audit event=stop reason=" << reason << "\n";
    output_.flush();
    output_.close();
    ready_ = false;
}

void LiveExternalNavOutputAuditLog::record_estimate(
    const LiveExternalNavOutputSnapshot& snapshot,
    const LiveExternalNavOutputResult& result) {
    if (!ready_) {
        throw std::runtime_error("LiveExternalNavOutputAuditLog record_estimate called before ready");
    }

    const auto& estimate = snapshot.estimate;
    output_ << "external_nav_output_audit event=estimate"
            << " allowed=" << (result.allowed ? "true" : "false")
            << " sent=" << (result.sent ? "true" : "false")
            << " decision=" << (result.allowed ? "allowed" : "blocked")
            << " reason=" << result.reason
            << " time_usec=" << snapshot.time_usec
            << " valid_for_fc=" << (estimate.valid_for_fc ? "true" : "false")
            << " estimate_reason=" << estimate.reason
            << " source=" << estimate.source_tag
            << " x_m=" << estimate.x_m
            << " y_m=" << estimate.y_m
            << " z_m=" << estimate.z_m
            << " yaw_rad=" << estimate.yaw_rad
            << " yaw_source_independent=" << (estimate.yaw_source_independent ? "true" : "false")
            << " pose_frame=" << coordinate_frame_name(estimate.pose_frame)
            << " frame_alignment_known=" << (estimate.frame_alignment_known ? "true" : "false")
            << " route_origin_ned_m=" << estimate.route_origin_ned_m.x
            << "/" << estimate.route_origin_ned_m.y
            << "/" << estimate.route_origin_ned_m.z
            << " route_heading_ned_rad=" << estimate.route_heading_ned_rad
            << " altitude_origin_aligned=" << (estimate.altitude_origin_aligned ? "true" : "false")
            << " confidence=" << estimate.confidence
            << " progress=" << estimate.route_progress
            << " route_index=" << estimate.route_index
            << " route_entries=" << estimate.route_entries
            << " relative_altitude_seen=" << (estimate.relative_altitude_seen ? "true" : "false")
            << " relative_altitude_m=" << estimate.relative_altitude_m
            << " route_match_valid=" << (estimate.route_match_valid ? "true" : "false")
            << " telemetry_fresh=" << (estimate.telemetry_fresh ? "true" : "false")
            << " altitude_valid=" << (estimate.altitude_valid ? "true" : "false")
            << " scale_known=" << (estimate.scale_known ? "true" : "false")
            << " visual_scale_valid=" << (estimate.visual_scale_valid ? "true" : "false")
            << " visual_scale_ratio=" << estimate.visual_scale_ratio
            << "\n";
    output_.flush();
    if (!output_.good()) {
        ready_ = false;
        throw std::runtime_error("LiveExternalNavOutputAuditLog write failed");
    }
}

bool LiveExternalNavOutputAuditLog::ready() const {
    return ready_;
}

const std::filesystem::path& LiveExternalNavOutputAuditLog::path() const {
    return config_.path;
}

} // namespace vh
