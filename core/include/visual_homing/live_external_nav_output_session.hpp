#pragma once

#include <cstdint>
#include <string>

#include "visual_homing/external_nav_estimator.hpp"
#include "visual_homing/live_mavlink_external_nav_writer.hpp"

namespace vh {

struct LiveExternalNavOutputSessionConfig {
    std::string run_id;
    bool output_available = false;
    bool runtime_enabled = false;
    bool operator_confirmed = false;
    bool audit_log_enabled = false;
    bool single_writer = false;
    std::uint64_t max_messages = 0;
    double max_duration_ms = 0.0;
};

struct LiveExternalNavOutputSnapshot {
    Timestamp now{};
    ExternalNavEstimate estimate;
    std::uint64_t time_usec = 0;
};

struct LiveExternalNavOutputResult {
    bool allowed = false;
    bool sent = false;
    std::string reason;
};

class LiveExternalNavOutputAuditSink {
public:
    virtual ~LiveExternalNavOutputAuditSink() = default;
    virtual bool start(const std::string& run_id) = 0;
    virtual void stop(const std::string& reason) = 0;
    virtual void record_estimate(
        const LiveExternalNavOutputSnapshot& snapshot,
        const LiveExternalNavOutputResult& result) = 0;
    virtual bool ready() const = 0;
};

class LiveExternalNavOutputSession final {
public:
    LiveExternalNavOutputSession(
        LiveExternalNavOutputSessionConfig config,
        LiveExternalNavOutputAuditSink& audit,
        ExternalNavWriter& writer);

    bool start();
    void stop(const std::string& reason);
    LiveExternalNavOutputResult process(const LiveExternalNavOutputSnapshot& snapshot);
    bool running() const;

private:
    LiveExternalNavOutputResult evaluate(const LiveExternalNavOutputSnapshot& snapshot) const;
    void record_or_stop(
        const LiveExternalNavOutputSnapshot& snapshot,
        const LiveExternalNavOutputResult& result);

    LiveExternalNavOutputSessionConfig config_;
    LiveExternalNavOutputAuditSink& audit_;
    ExternalNavWriter& writer_;
    Timestamp started_at_{};
    std::uint64_t messages_sent_ = 0;
    bool writer_started_ = false;
    bool running_ = false;
    bool started_once_ = false;
};

} // namespace vh
