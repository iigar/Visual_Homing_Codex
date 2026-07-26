#include "visual_homing/bounded_verification_publisher.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace vh {
namespace {

double elapsed_ms(
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point finish) {
    return std::chrono::duration<double, std::milli>(finish - start).count();
}

class LiveVerificationPublicationProcessor final : public VerificationPublicationProcessor {
public:
    explicit LiveVerificationPublicationProcessor(LiveVerificationCaptureConfig config)
        : session_(std::move(config)) {}

    LiveVerificationFrameResult process(
        const Frame& native_frame,
        const LiveVerificationFrameContext& context) override {
        return session_.observe(native_frame, context);
    }

    LiveVerificationCaptureMetrics capture_metrics() const override {
        return session_.metrics();
    }

    std::filesystem::path current_manifest_path() const override {
        return session_.current_manifest_path();
    }

private:
    LiveVerificationCaptureSession session_;
};

} // namespace

const char* verification_submission_status_name(VerificationSubmissionStatus status) {
    switch (status) {
    case VerificationSubmissionStatus::Accepted:
        return "accepted";
    case VerificationSubmissionStatus::Backpressure:
        return "backpressure";
    case VerificationSubmissionStatus::NotRunning:
        return "not_running";
    case VerificationSubmissionStatus::Failed:
        return "failed";
    }
    return "unknown";
}

LiveVerificationCaptureMetrics VerificationPublicationProcessor::capture_metrics() const {
    return {};
}

std::filesystem::path VerificationPublicationProcessor::current_manifest_path() const {
    return {};
}

struct BoundedVerificationPublisher::Impl {
    struct Job {
        std::uint64_t sequence = 0;
        Frame native_frame;
        LiveVerificationFrameContext context;
        std::chrono::steady_clock::time_point enqueued_at;
    };

    Impl(
        BoundedVerificationPublisherConfig publisher_config,
        std::unique_ptr<VerificationPublicationProcessor> publication_processor)
        : config(std::move(publisher_config)),
          processor(std::move(publication_processor)) {
        if (config.queue_capacity == 0) {
            throw std::invalid_argument(
                "Bounded verification publisher queue capacity must be positive");
        }
        if (!processor) {
            throw std::invalid_argument(
                "Bounded verification publisher processor must be provided");
        }
    }

    ~Impl() {
        stop(true);
    }

    std::size_t outstanding_jobs_locked() const {
        return queue.size() + (worker_busy ? 1U : 0U);
    }

    void refresh_metrics_locked() {
        publisher_metrics.queue_depth = queue.size();
        publisher_metrics.outstanding_jobs = outstanding_jobs_locked();
        publisher_metrics.running = running;
        publisher_metrics.worker_busy = worker_busy;
        publisher_metrics.failed = failed;
        publisher_metrics.stopped = stopped;
        publisher_metrics.failure_reason = failure_reason;
    }

    bool start() {
        std::lock_guard lock(mutex);
        if (started_once || failed || stopped) {
            return false;
        }
        started_once = true;
        running = true;
        worker = std::thread([this]() { worker_loop(); });
        refresh_metrics_locked();
        return true;
    }

    VerificationSubmissionResult submit(
        const Frame& native_frame,
        const LiveVerificationFrameContext& context) {
        std::lock_guard lock(mutex);
        ++publisher_metrics.submissions;
        if (failed) {
            ++publisher_metrics.rejected_failed;
            refresh_metrics_locked();
            return {
                .status = VerificationSubmissionStatus::Failed,
                .outstanding_jobs = publisher_metrics.outstanding_jobs,
                .reason = failure_reason.empty()
                    ? "verification_publisher_failed"
                    : failure_reason,
            };
        }
        if (!running || stop_requested) {
            ++publisher_metrics.rejected_not_running;
            refresh_metrics_locked();
            return {
                .status = VerificationSubmissionStatus::NotRunning,
                .outstanding_jobs = publisher_metrics.outstanding_jobs,
                .reason = "verification_publisher_not_running",
            };
        }
        const auto outstanding = outstanding_jobs_locked();
        if (outstanding >= config.queue_capacity) {
            ++publisher_metrics.rejected_backpressure;
            refresh_metrics_locked();
            return {
                .status = VerificationSubmissionStatus::Backpressure,
                .outstanding_jobs = publisher_metrics.outstanding_jobs,
                .reason = "verification_publisher_backpressure",
            };
        }

        const auto sequence = next_sequence++;
        queue.push_back({
            .sequence = sequence,
            .native_frame = native_frame,
            .context = context,
            .enqueued_at = std::chrono::steady_clock::now(),
        });
        ++publisher_metrics.accepted;
        publisher_metrics.maximum_outstanding_jobs = std::max(
            publisher_metrics.maximum_outstanding_jobs,
            outstanding_jobs_locked());
        refresh_metrics_locked();
        condition.notify_all();
        return {
            .status = VerificationSubmissionStatus::Accepted,
            .sequence = sequence,
            .outstanding_jobs = publisher_metrics.outstanding_jobs,
            .reason = "accepted",
        };
    }

    bool wait_until_idle(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex);
        return condition.wait_for(lock, timeout, [this]() {
            return queue.empty() && !worker_busy;
        });
    }

    void stop(bool drain) {
        {
            std::lock_guard lock(mutex);
            if (!started_once) {
                refresh_metrics_locked();
                return;
            }
            if (!stopped) {
                running = false;
                stop_requested = true;
                drain_on_stop = drain;
                if (!drain_on_stop) {
                    publisher_metrics.discarded_on_stop += queue.size();
                    queue.clear();
                }
            }
            refresh_metrics_locked();
        }
        condition.notify_all();
        if (worker.joinable()) {
            worker.join();
        }
        {
            std::lock_guard lock(mutex);
            stopped = true;
            refresh_metrics_locked();
        }
        condition.notify_all();
    }

    void worker_loop() {
        for (;;) {
            Job job;
            {
                std::unique_lock lock(mutex);
                condition.wait(lock, [this]() {
                    return stop_requested || !queue.empty();
                });
                if (stop_requested && (!drain_on_stop || queue.empty())) {
                    break;
                }
                if (queue.empty()) {
                    continue;
                }
                job = std::move(queue.front());
                queue.pop_front();
                worker_busy = true;
                refresh_metrics_locked();
            }

            const auto processing_started = std::chrono::steady_clock::now();
            const auto queue_wait_ms = elapsed_ms(job.enqueued_at, processing_started);
            try {
                auto result = processor->process(job.native_frame, job.context);
                const auto processing_finished = std::chrono::steady_clock::now();
                const auto processing_ms = elapsed_ms(
                    processing_started, processing_finished);
                {
                    std::lock_guard lock(mutex);
                    ++publisher_metrics.completed;
                    publisher_metrics.last_completed_sequence = job.sequence;
                    if (result.publication) {
                        ++publisher_metrics.publication_results;
                        latest_publication = result.publication;
                    }
                    publisher_metrics.total_queue_wait_ms += queue_wait_ms;
                    publisher_metrics.maximum_queue_wait_ms = std::max(
                        publisher_metrics.maximum_queue_wait_ms,
                        queue_wait_ms);
                    publisher_metrics.total_processing_ms += processing_ms;
                    publisher_metrics.maximum_processing_ms = std::max(
                        publisher_metrics.maximum_processing_ms,
                        processing_ms);
                    latest_result = std::move(result);
                    worker_busy = false;
                    refresh_metrics_locked();
                }
                condition.notify_all();
            } catch (const std::exception& error) {
                fail(error.what());
                break;
            } catch (...) {
                fail("verification_publisher_unknown_processing_failure");
                break;
            }
        }

        {
            std::lock_guard lock(mutex);
            running = false;
            worker_busy = false;
            stopped = true;
            refresh_metrics_locked();
        }
        condition.notify_all();
    }

    void fail(std::string reason) {
        {
            std::lock_guard lock(mutex);
            failed = true;
            running = false;
            stop_requested = true;
            drain_on_stop = false;
            failure_reason = reason.empty()
                ? "verification_publisher_processing_failure"
                : std::move(reason);
            ++publisher_metrics.processing_failures;
            publisher_metrics.abandoned_after_failure += queue.size();
            queue.clear();
            worker_busy = false;
            refresh_metrics_locked();
        }
        condition.notify_all();
    }

    BoundedVerificationPublisherConfig config;
    std::unique_ptr<VerificationPublicationProcessor> processor;
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::deque<Job> queue;
    std::thread worker;
    BoundedVerificationPublisherMetrics publisher_metrics;
    std::optional<LiveVerificationFrameResult> latest_result;
    std::optional<VerificationPackagePublication> latest_publication;
    std::uint64_t next_sequence = 1;
    bool started_once = false;
    bool running = false;
    bool worker_busy = false;
    bool stop_requested = false;
    bool drain_on_stop = true;
    bool failed = false;
    bool stopped = false;
    std::string failure_reason;
};

BoundedVerificationPublisher::BoundedVerificationPublisher(
    BoundedVerificationPublisherConfig config)
    : impl_(nullptr) {
    auto capture_config = std::move(config.capture);
    impl_ = std::make_unique<Impl>(
        std::move(config),
        std::make_unique<LiveVerificationPublicationProcessor>(
            std::move(capture_config)));
}

BoundedVerificationPublisher::BoundedVerificationPublisher(
    BoundedVerificationPublisherConfig config,
    std::unique_ptr<VerificationPublicationProcessor> processor)
    : impl_(std::make_unique<Impl>(
        std::move(config),
        std::move(processor))) {}

BoundedVerificationPublisher::~BoundedVerificationPublisher() = default;

bool BoundedVerificationPublisher::start() {
    return impl_->start();
}

VerificationSubmissionResult BoundedVerificationPublisher::submit(
    const Frame& native_frame,
    const LiveVerificationFrameContext& context) {
    return impl_->submit(native_frame, context);
}

bool BoundedVerificationPublisher::wait_until_idle(std::chrono::milliseconds timeout) {
    return impl_->wait_until_idle(timeout);
}

void BoundedVerificationPublisher::stop(bool drain) {
    impl_->stop(drain);
}

BoundedVerificationPublisherMetrics BoundedVerificationPublisher::metrics() const {
    std::lock_guard lock(impl_->mutex);
    impl_->refresh_metrics_locked();
    return impl_->publisher_metrics;
}

std::optional<LiveVerificationFrameResult> BoundedVerificationPublisher::last_result() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->latest_result;
}

std::optional<VerificationPackagePublication>
BoundedVerificationPublisher::last_publication() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->latest_publication;
}

LiveVerificationCaptureMetrics BoundedVerificationPublisher::capture_metrics() const {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->queue.empty() || impl_->worker_busy) {
        throw std::logic_error(
            "Bounded verification publisher capture metrics require an idle worker");
    }
    return impl_->processor->capture_metrics();
}

std::filesystem::path BoundedVerificationPublisher::current_manifest_path() const {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->queue.empty() || impl_->worker_busy) {
        throw std::logic_error(
            "Bounded verification publisher manifest path requires an idle worker");
    }
    return impl_->processor->current_manifest_path();
}

} // namespace vh
