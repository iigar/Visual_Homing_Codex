#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "visual_homing/live_mavlink_output_safety_gate.hpp"

namespace vh {

struct LiveMavlinkOutputAuditLogConfig {
    std::filesystem::path path;
    bool append = true;
};

class LiveMavlinkOutputAuditSink {
public:
    virtual ~LiveMavlinkOutputAuditSink() = default;
    virtual bool start(const std::string& run_id) = 0;
    virtual void stop(const std::string& reason) = 0;
    virtual void record_command(
        const LiveMavlinkOutputSafetySnapshot& snapshot,
        const LiveMavlinkOutputSafetyResult& safety_result) = 0;
    virtual bool ready() const = 0;
};

class LiveMavlinkOutputAuditLog final : public LiveMavlinkOutputAuditSink {
public:
    explicit LiveMavlinkOutputAuditLog(LiveMavlinkOutputAuditLogConfig config);

    bool start(const std::string& run_id) override;
    void stop(const std::string& reason) override;
    void record_command(
        const LiveMavlinkOutputSafetySnapshot& snapshot,
        const LiveMavlinkOutputSafetyResult& safety_result) override;

    bool ready() const override;
    const std::filesystem::path& path() const;

private:
    LiveMavlinkOutputAuditLogConfig config_;
    std::ofstream output_;
    bool ready_ = false;
};

} // namespace vh
