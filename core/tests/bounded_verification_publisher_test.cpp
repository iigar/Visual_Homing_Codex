#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "visual_homing/bounded_verification_publisher.hpp"

namespace {

using namespace std::chrono_literals;

vh::Frame frame(std::uint64_t id) {
    vh::Frame result;
    result.id = id;
    result.timestamp = vh::Timestamp(std::chrono::nanoseconds(id * 1000));
    result.width = 1;
    result.height = 1;
    result.format = vh::PixelFormat::Gray8;
    result.data = {static_cast<std::uint8_t>(id)};
    return result;
}

class ControlledProcessor final : public vh::VerificationPublicationProcessor {
public:
    explicit ControlledProcessor(bool fail_first = false)
        : fail_first_(fail_first) {}

    vh::LiveVerificationFrameResult process(
        const vh::Frame& native_frame,
        const vh::LiveVerificationFrameContext&) override {
        {
            std::unique_lock lock(mutex_);
            entered_ = true;
            condition_.notify_all();
            condition_.wait(lock, [this]() { return released_; });
        }
        if (fail_first_) {
            fail_first_ = false;
            throw std::runtime_error("controlled_publication_failure");
        }
        {
            std::lock_guard lock(mutex_);
            processed_.push_back(native_frame.id);
        }
        vh::LiveVerificationFrameResult result;
        result.decision.valid = true;
        result.decision.request_native_capture = true;
        return result;
    }

    bool wait_until_entered(std::chrono::milliseconds timeout = 1000ms) {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, timeout, [this]() { return entered_; });
    }

    void release() {
        {
            std::lock_guard lock(mutex_);
            released_ = true;
        }
        condition_.notify_all();
    }

    std::vector<std::uint64_t> processed() const {
        std::lock_guard lock(mutex_);
        return processed_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool entered_ = false;
    bool released_ = false;
    bool fail_first_ = false;
    std::vector<std::uint64_t> processed_;
};

vh::BoundedVerificationPublisherConfig config(std::size_t capacity) {
    vh::BoundedVerificationPublisherConfig result;
    result.queue_capacity = capacity;
    return result;
}

} // namespace

int main() {
    {
        bool rejected = false;
        try {
            auto processor = std::make_unique<ControlledProcessor>();
            vh::BoundedVerificationPublisher publisher(config(0), std::move(processor));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto processor = std::make_unique<ControlledProcessor>();
        auto* controlled = processor.get();
        vh::BoundedVerificationPublisher publisher(config(2), std::move(processor));
        const auto before_start = publisher.submit(frame(1), {});
        assert(before_start.status == vh::VerificationSubmissionStatus::NotRunning);
        assert(publisher.start());
        assert(!publisher.start());

        const auto first = publisher.submit(frame(1), {});
        assert(first.status == vh::VerificationSubmissionStatus::Accepted);
        assert(first.sequence == 1);
        assert(controlled->wait_until_entered());
        const auto second = publisher.submit(frame(2), {});
        assert(second.status == vh::VerificationSubmissionStatus::Accepted);
        assert(second.sequence == 2);
        const auto third = publisher.submit(frame(3), {});
        assert(third.status == vh::VerificationSubmissionStatus::Backpressure);
        assert(third.reason == "verification_publisher_backpressure");

        controlled->release();
        assert(publisher.wait_until_idle(2000ms));
        publisher.stop(true);
        const auto metrics = publisher.metrics();
        assert(metrics.submissions == 4);
        assert(metrics.accepted == 2);
        assert(metrics.rejected_not_running == 1);
        assert(metrics.rejected_backpressure == 1);
        assert(metrics.completed == 2);
        assert(metrics.processing_failures == 0);
        assert(metrics.maximum_outstanding_jobs == 2);
        assert(metrics.queue_depth == 0);
        assert(metrics.outstanding_jobs == 0);
        assert(metrics.stopped);
        assert((controlled->processed() == std::vector<std::uint64_t>{1, 2}));
        assert(std::string(vh::verification_submission_status_name(
            vh::VerificationSubmissionStatus::Backpressure)) == "backpressure");
    }
    {
        auto processor = std::make_unique<ControlledProcessor>(true);
        auto* controlled = processor.get();
        vh::BoundedVerificationPublisher publisher(config(2), std::move(processor));
        assert(publisher.start());
        assert(publisher.submit(frame(10), {}).status
            == vh::VerificationSubmissionStatus::Accepted);
        assert(controlled->wait_until_entered());
        assert(publisher.submit(frame(11), {}).status
            == vh::VerificationSubmissionStatus::Accepted);
        controlled->release();
        assert(publisher.wait_until_idle(2000ms));
        const auto failed = publisher.metrics();
        assert(failed.failed);
        assert(failed.processing_failures == 1);
        assert(failed.abandoned_after_failure == 1);
        assert(failed.failure_reason == "controlled_publication_failure");
        const auto after_failure = publisher.submit(frame(12), {});
        assert(after_failure.status == vh::VerificationSubmissionStatus::Failed);
        assert(after_failure.reason == "controlled_publication_failure");
        publisher.stop(true);
    }
    {
        auto processor = std::make_unique<ControlledProcessor>();
        auto* controlled = processor.get();
        vh::BoundedVerificationPublisher publisher(config(2), std::move(processor));
        assert(publisher.start());
        assert(publisher.submit(frame(20), {}).status
            == vh::VerificationSubmissionStatus::Accepted);
        assert(controlled->wait_until_entered());
        assert(publisher.submit(frame(21), {}).status
            == vh::VerificationSubmissionStatus::Accepted);

        std::thread stopper([&publisher]() { publisher.stop(false); });
        for (int attempt = 0; attempt < 100; ++attempt) {
            if (publisher.metrics().discarded_on_stop == 1) {
                break;
            }
            std::this_thread::sleep_for(5ms);
        }
        assert(publisher.metrics().discarded_on_stop == 1);
        controlled->release();
        stopper.join();

        const auto metrics = publisher.metrics();
        assert(metrics.completed == 1);
        assert(metrics.discarded_on_stop == 1);
        assert((controlled->processed() == std::vector<std::uint64_t>{20}));
    }
    return 0;
}
