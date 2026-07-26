#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "visual_homing/live_verification_capture.hpp"

namespace vh {

enum class VerificationSubmissionStatus {
    Accepted,
    Backpressure,
    NotRunning,
    Failed,
};

const char* verification_submission_status_name(VerificationSubmissionStatus status);

struct BoundedVerificationPublisherConfig {
    LiveVerificationCaptureConfig capture;
    std::size_t queue_capacity = 2;
};

struct VerificationSubmissionResult {
    VerificationSubmissionStatus status = VerificationSubmissionStatus::NotRunning;
    std::uint64_t sequence = 0;
    std::size_t outstanding_jobs = 0;
    std::string reason;
};

struct BoundedVerificationPublisherMetrics {
    std::uint64_t submissions = 0;
    std::uint64_t accepted = 0;
    std::uint64_t rejected_backpressure = 0;
    std::uint64_t rejected_not_running = 0;
    std::uint64_t rejected_failed = 0;
    std::uint64_t completed = 0;
    std::uint64_t publication_results = 0;
    std::uint64_t processing_failures = 0;
    std::uint64_t abandoned_after_failure = 0;
    std::uint64_t discarded_on_stop = 0;
    std::uint64_t last_completed_sequence = 0;
    std::size_t queue_depth = 0;
    std::size_t outstanding_jobs = 0;
    std::size_t maximum_outstanding_jobs = 0;
    bool running = false;
    bool worker_busy = false;
    bool failed = false;
    bool stopped = false;
    double total_queue_wait_ms = 0.0;
    double maximum_queue_wait_ms = 0.0;
    double total_processing_ms = 0.0;
    double maximum_processing_ms = 0.0;
    std::string failure_reason;
};

class VerificationPublicationProcessor {
public:
    virtual ~VerificationPublicationProcessor() = default;

    virtual LiveVerificationFrameResult process(
        const Frame& native_frame,
        const LiveVerificationFrameContext& context) = 0;
    virtual LiveVerificationCaptureMetrics capture_metrics() const;
    virtual std::filesystem::path current_manifest_path() const;
};

class BoundedVerificationPublisher {
public:
    explicit BoundedVerificationPublisher(BoundedVerificationPublisherConfig config);
    BoundedVerificationPublisher(
        BoundedVerificationPublisherConfig config,
        std::unique_ptr<VerificationPublicationProcessor> processor);
    ~BoundedVerificationPublisher();

    BoundedVerificationPublisher(const BoundedVerificationPublisher&) = delete;
    BoundedVerificationPublisher& operator=(const BoundedVerificationPublisher&) = delete;
    BoundedVerificationPublisher(BoundedVerificationPublisher&&) = delete;
    BoundedVerificationPublisher& operator=(BoundedVerificationPublisher&&) = delete;

    bool start();
    VerificationSubmissionResult submit(
        const Frame& native_frame,
        const LiveVerificationFrameContext& context);
    bool wait_until_idle(std::chrono::milliseconds timeout);
    void stop(bool drain);

    BoundedVerificationPublisherMetrics metrics() const;
    std::optional<LiveVerificationFrameResult> last_result() const;
    std::optional<VerificationPackagePublication> last_publication() const;
    LiveVerificationCaptureMetrics capture_metrics() const;
    std::filesystem::path current_manifest_path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vh
