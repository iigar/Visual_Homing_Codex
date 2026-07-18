#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>

#include "visual_homing/interfaces.hpp"

namespace vh {

struct RouteSignatureStreamingRecorderConfig {
    std::filesystem::path output_path;
    std::size_t queue_capacity_entries = 64;
    std::uint32_t checkpoint_interval_entries = 64;
};

struct RouteSignatureStreamingRecorderMetrics {
    std::uint64_t entries_enqueued = 0;
    std::uint64_t entries_written = 0;
    std::size_t current_queue_depth = 0;
    std::size_t max_queue_depth = 0;
    std::uint64_t queue_full_events = 0;
    std::uint64_t write_failures = 0;
    double total_write_latency_ms = 0.0;
    double max_write_latency_ms = 0.0;
    bool finalized = false;
};

class RouteSignatureStreamingRecorder final : public RouteRecorder {
public:
    explicit RouteSignatureStreamingRecorder(RouteSignatureStreamingRecorderConfig config);
    ~RouteSignatureStreamingRecorder() override;

    RouteSignatureStreamingRecorder(const RouteSignatureStreamingRecorder&) = delete;
    RouteSignatureStreamingRecorder& operator=(const RouteSignatureStreamingRecorder&) = delete;
    RouteSignatureStreamingRecorder(RouteSignatureStreamingRecorder&&) = delete;
    RouteSignatureStreamingRecorder& operator=(RouteSignatureStreamingRecorder&&) = delete;

    void observe(const Frame& frame, const NavigationEstimate& nav) override;
    void finalize();
    RouteSignatureStreamingRecorderMetrics metrics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vh
