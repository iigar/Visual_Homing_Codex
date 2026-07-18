#include "visual_homing/route_signature_streaming_recorder.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include "visual_homing/route_signature_entry.hpp"
#include "visual_homing/route_signature_stream_writer.hpp"

namespace vh {

struct RouteSignatureStreamingRecorder::Impl {
    explicit Impl(RouteSignatureStreamingRecorderConfig recorder_config)
        : config(std::move(recorder_config)),
          writer({config.output_path, config.checkpoint_interval_entries}),
          worker(&Impl::write_loop, this) {}

    ~Impl() {
        request_stop();
        join_worker();
    }

    void request_stop() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex);
            accepting = false;
            stop_requested = true;
        }
        wake.notify_all();
    }

    void join_worker() noexcept {
        if (worker.joinable()) {
            worker.join();
        }
    }

    void write_loop() noexcept {
        for (;;) {
            RouteSignatureEntry entry;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this] { return stop_requested || !queue.empty(); });
                if (queue.empty()) {
                    if (stop_requested) {
                        return;
                    }
                    continue;
                }
                entry = std::move(queue.front());
                queue.pop_front();
                recorder_metrics.current_queue_depth = queue.size();
            }

            const auto started_at = std::chrono::steady_clock::now();
            try {
                writer.append(entry);
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex);
                worker_error = std::current_exception();
                ++recorder_metrics.write_failures;
                accepting = false;
                stop_requested = true;
                queue.clear();
                recorder_metrics.current_queue_depth = 0;
                wake.notify_all();
                return;
            }
            const auto finished_at = std::chrono::steady_clock::now();
            const auto latency_ms = std::chrono::duration<double, std::milli>(finished_at - started_at).count();
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++recorder_metrics.entries_written;
                recorder_metrics.total_write_latency_ms += latency_ms;
                recorder_metrics.max_write_latency_ms = std::max(recorder_metrics.max_write_latency_ms, latency_ms);
            }
        }
    }

    RouteSignatureStreamingRecorderConfig config;
    RouteSignatureStreamWriter writer;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<RouteSignatureEntry> queue;
    RouteSignatureStreamingRecorderMetrics recorder_metrics;
    std::exception_ptr worker_error;
    bool accepting = true;
    bool stop_requested = false;
    std::thread worker;
};

RouteSignatureStreamingRecorder::RouteSignatureStreamingRecorder(RouteSignatureStreamingRecorderConfig config) {
    if (config.queue_capacity_entries == 0) {
        throw std::invalid_argument("Route signature streaming recorder queue capacity must be positive");
    }
    if (config.checkpoint_interval_entries == 0) {
        throw std::invalid_argument("Route signature streaming recorder checkpoint interval must be positive");
    }
    impl_ = std::make_unique<Impl>(std::move(config));
}

RouteSignatureStreamingRecorder::~RouteSignatureStreamingRecorder() = default;

void RouteSignatureStreamingRecorder::observe(const Frame& frame, const NavigationEstimate& nav) {
    auto entry = make_route_signature_entry(frame, nav);
    std::exception_ptr worker_error;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        worker_error = impl_->worker_error;
        if (!worker_error) {
            if (!impl_->accepting) {
                throw std::runtime_error("Route signature streaming recorder is not accepting entries");
            }
            if (impl_->queue.size() >= impl_->config.queue_capacity_entries) {
                ++impl_->recorder_metrics.queue_full_events;
                throw std::runtime_error("Route signature streaming recorder queue is full");
            }
            impl_->queue.push_back(std::move(entry));
            ++impl_->recorder_metrics.entries_enqueued;
            impl_->recorder_metrics.current_queue_depth = impl_->queue.size();
            impl_->recorder_metrics.max_queue_depth = std::max(
                impl_->recorder_metrics.max_queue_depth,
                impl_->queue.size());
        }
    }
    if (worker_error) {
        std::rethrow_exception(worker_error);
    }
    impl_->wake.notify_one();
}

void RouteSignatureStreamingRecorder::finalize() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->recorder_metrics.finalized) {
            return;
        }
    }

    impl_->request_stop();
    impl_->join_worker();

    std::exception_ptr worker_error;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        worker_error = impl_->worker_error;
    }
    if (worker_error) {
        std::rethrow_exception(worker_error);
    }

    impl_->writer.finalize();
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->recorder_metrics.finalized = true;
    }
}

RouteSignatureStreamingRecorderMetrics RouteSignatureStreamingRecorder::metrics() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->recorder_metrics;
}

} // namespace vh
