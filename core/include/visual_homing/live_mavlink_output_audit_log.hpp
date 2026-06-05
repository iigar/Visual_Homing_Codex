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

class LiveMavlinkOutputAuditLog {
public:
    explicit LiveMavlinkOutputAuditLog(LiveMavlinkOutputAuditLogConfig config);

    bool start(const std::string& run_id);
    void stop(const std::string& reason);
    void record_command(const NavigationCommand& command, const LiveMavlinkOutputSafetyResult& safety_result);

    bool ready() const;
    const std::filesystem::path& path() const;

private:
    LiveMavlinkOutputAuditLogConfig config_;
    std::ofstream output_;
    bool ready_ = false;
};

} // namespace vh
