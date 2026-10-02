#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "visual_homing/mavlink_telemetry_inspector.hpp"

namespace vh {

struct MavlinkTelemetryStreamConfig {
    std::string device_path;
    int baud_rate = 57600;
    std::uint64_t max_buffer_bytes = 65536;
    MavlinkTelemetrySourceId expected_source{};
};

struct MavlinkTelemetryReceipt {
    // Absolute exclusive byte position within one stream run, unaffected by tail eviction.
    std::uint64_t end_offset = 0;
    // Host receive time of the read completing this message; absent if unknown.
    std::optional<Timestamp> received_at;
};

struct MavlinkTelemetryReceipts {
    MavlinkTelemetryReceipt heartbeat;
    MavlinkTelemetryReceipt attitude;
    MavlinkTelemetryReceipt relative_altitude;
};

struct MavlinkTelemetryStreamSnapshot {
    bool supported = false;
    bool opened = false;
    bool running = false;
    std::uint64_t bytes_captured = 0;
    std::uint64_t bytes_retained = 0;
    std::uint64_t bytes_dropped = 0;
    MavlinkTelemetryInspectionSummary inspection{};
    MavlinkTelemetryReceipts receipts{};
};

class MavlinkTelemetryByteBuffer final {
public:
    explicit MavlinkTelemetryByteBuffer(std::uint64_t max_buffer_bytes, MavlinkTelemetrySourceId source = {});

    void append(const char* data, std::size_t size);
    void append(const char* data, std::size_t size, Timestamp received_at);
    void clear();

    const std::string& bytes() const;
    std::uint64_t bytes_captured() const;
    std::uint64_t bytes_retained() const;
    std::uint64_t bytes_dropped() const;
    const MavlinkTelemetryInspectionSummary& inspection() const;
    const MavlinkTelemetryReceipts& receipts() const;

private:
    const MavlinkTelemetrySourceId source_;
    std::uint64_t max_buffer_bytes_ = 0;
    std::uint64_t bytes_captured_ = 0;
    std::uint64_t bytes_dropped_ = 0;
    std::string bytes_;
    MavlinkTelemetryInspectionSummary inspection_{};
    MavlinkTelemetryReceipts receipts_{};
};

class MavlinkTelemetryStream final {
public:
    explicit MavlinkTelemetryStream(MavlinkTelemetryStreamConfig config);
    MavlinkTelemetryStream(const MavlinkTelemetryStream&) = delete;
    MavlinkTelemetryStream& operator=(const MavlinkTelemetryStream&) = delete;
    ~MavlinkTelemetryStream();

    bool start();
    void stop();
    MavlinkTelemetryStreamSnapshot snapshot() const;
    std::string last_error() const;

private:
    void read_loop();

    MavlinkTelemetryStreamConfig config_;
    mutable std::mutex mutex_;
    std::thread worker_;
    bool stop_requested_ = false;
    bool supported_ = false;
    bool opened_ = false;
    bool running_ = false;
    MavlinkTelemetryByteBuffer bytes_;
    std::string last_error_;
};

} // namespace vh
