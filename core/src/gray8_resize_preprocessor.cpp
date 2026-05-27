#include "visual_homing/gray8_resize_preprocessor.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace vh {

Gray8ResizePreprocessor::Gray8ResizePreprocessor(int target_width, int target_height)
    : target_width_(target_width), target_height_(target_height) {
    if (target_width_ <= 0 || target_height_ <= 0) {
        throw std::invalid_argument("Preprocessor target dimensions must be positive");
    }
}

Frame Gray8ResizePreprocessor::process(const Frame& input) {
    if (input.format != PixelFormat::Gray8) {
        throw std::runtime_error("Gray8ResizePreprocessor only accepts Gray8 input");
    }
    if (input.width <= 0 || input.height <= 0) {
        throw std::runtime_error("Gray8ResizePreprocessor input dimensions must be positive");
    }

    const auto expected_size = static_cast<std::size_t>(input.width) * static_cast<std::size_t>(input.height);
    if (input.data.size() != expected_size) {
        throw std::runtime_error("Gray8ResizePreprocessor input payload size does not match dimensions");
    }

    Frame output;
    output.id = input.id;
    output.timestamp = input.timestamp;
    output.width = target_width_;
    output.height = target_height_;
    output.format = PixelFormat::Gray8;
    output.data.resize(static_cast<std::size_t>(target_width_) * static_cast<std::size_t>(target_height_));

    for (int oy = 0; oy < target_height_; ++oy) {
        const int y0 = oy * input.height / target_height_;
        const int y1 = std::max(y0 + 1, (oy + 1) * input.height / target_height_);

        for (int ox = 0; ox < target_width_; ++ox) {
            const int x0 = ox * input.width / target_width_;
            const int x1 = std::max(x0 + 1, (ox + 1) * input.width / target_width_);

            std::uint32_t sum = 0;
            std::uint32_t count = 0;
            for (int iy = y0; iy < std::min(y1, input.height); ++iy) {
                for (int ix = x0; ix < std::min(x1, input.width); ++ix) {
                    const auto index = static_cast<std::size_t>(iy) * static_cast<std::size_t>(input.width)
                        + static_cast<std::size_t>(ix);
                    sum += input.data[index];
                    ++count;
                }
            }

            const auto output_index = static_cast<std::size_t>(oy) * static_cast<std::size_t>(target_width_)
                + static_cast<std::size_t>(ox);
            output.data[output_index] = static_cast<std::uint8_t>((sum + count / 2U) / count);
        }
    }

    return output;
}

int Gray8ResizePreprocessor::target_width() const {
    return target_width_;
}

int Gray8ResizePreprocessor::target_height() const {
    return target_height_;
}

} // namespace vh
