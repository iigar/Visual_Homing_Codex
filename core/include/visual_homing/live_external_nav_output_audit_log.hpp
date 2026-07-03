#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "visual_homing/live_external_nav_output_session.hpp"

namespace vh {

struct LiveExternalNavOutputAuditLogConfig {
    std::filesystem::path path;
    bool append = true;
};

class LiveExternalNavOutputAuditLog final : public LiveExternalNavOutputAuditSink {
public:
    explicit LiveExternalNavOutputAuditLog(LiveExternalNavOutputAuditLogConfig config);

    bool start(const std::string& run_id) override;
    void stop(const std::string& reason) override;
    void record_estimate(
        const LiveExternalNavOutputSnapshot& snapshot,
        const LiveExternalNavOutputResult& result) override;
    bool ready() const override;
    const std::filesystem::path& path() const;

private:
    LiveExternalNavOutputAuditLogConfig config_;
    std::ofstream output_;
    bool ready_ = false;
};

} // namespace vh
