#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>

#include "visual_homing/route_signature.hpp"

namespace vh {

struct RouteSignatureStreamWriterConfig {
    std::filesystem::path output_path;
    std::uint32_t checkpoint_interval_entries = 64;
};

class RouteSignatureStreamWriter {
public:
    explicit RouteSignatureStreamWriter(RouteSignatureStreamWriterConfig config);
    ~RouteSignatureStreamWriter();

    RouteSignatureStreamWriter(const RouteSignatureStreamWriter&) = delete;
    RouteSignatureStreamWriter& operator=(const RouteSignatureStreamWriter&) = delete;
    RouteSignatureStreamWriter(RouteSignatureStreamWriter&&) = delete;
    RouteSignatureStreamWriter& operator=(RouteSignatureStreamWriter&&) = delete;

    void append(const RouteSignatureEntry& entry);
    void checkpoint();
    void finalize();

    const std::filesystem::path& output_path() const;
    const std::filesystem::path& partial_path() const;
    std::uint32_t entry_count() const;
    std::uint32_t checkpointed_entry_count() const;
    bool finalized() const;

private:
    void require_active() const;

    RouteSignatureStreamWriterConfig config_;
    std::filesystem::path partial_path_;
    std::fstream output_;
    std::uint32_t entry_count_ = 0;
    std::uint32_t checkpointed_entry_count_ = 0;
    bool finalized_ = false;
};

} // namespace vh
