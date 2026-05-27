#pragma once

#include "visual_homing/interfaces.hpp"

namespace vh {

class Gray8ResizePreprocessor final : public FramePreprocessor {
public:
    Gray8ResizePreprocessor(int target_width, int target_height);

    Frame process(const Frame& input) override;

    int target_width() const;
    int target_height() const;

private:
    int target_width_ = 0;
    int target_height_ = 0;
};

} // namespace vh
