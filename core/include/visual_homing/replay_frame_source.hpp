#pragma once

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <vector>

#include "visual_homing/interfaces.hpp"

namespace vh {

struct ReplayFrameEntry {
    std::uint64_t id = 0;
    std::uint64_t timestamp_ns = 0;
    std::filesystem::path image_path;
};

class ReplayFrameSource final : public CameraSource {
public:
    static ReplayFrameSource load_manifest(const std::filesystem::path& manifest_path);

    explicit ReplayFrameSource(std::filesystem::path base_dir, std::vector<ReplayFrameEntry> entries);

    bool start() override;
    void stop() override;
    std::optional<Frame> poll() override;

    std::size_t size() const;
    bool running() const;

private:
    std::filesystem::path base_dir_;
    std::vector<ReplayFrameEntry> entries_;
    std::size_t next_index_ = 0;
    bool running_ = false;
};

} // namespace vh
