#pragma once

#include <cstdint>
#include <vector>

#include "visual_homing/time.hpp"

namespace vh {

enum class PixelFormat {
    Gray8,
    Bgr8,
    Thermal16
};

struct Frame {
    std::uint64_t id = 0;
    Timestamp timestamp{};
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Gray8;
    std::vector<std::uint8_t> data;
};

} // namespace vh
