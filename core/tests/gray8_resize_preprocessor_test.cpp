#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "visual_homing/gray8_resize_preprocessor.hpp"

namespace {

vh::Frame make_gray8_frame(int width, int height, std::vector<std::uint8_t> data) {
    vh::Frame frame;
    frame.id = 42;
    frame.width = width;
    frame.height = height;
    frame.format = vh::PixelFormat::Gray8;
    frame.data = std::move(data);
    return frame;
}

} // namespace

int main() {
    vh::Gray8ResizePreprocessor preprocessor(2, 2);
    assert(preprocessor.target_width() == 2);
    assert(preprocessor.target_height() == 2);

    const auto input = make_gray8_frame(4, 4, {
        0, 10, 100, 110,
        20, 30, 120, 130,
        200, 210, 40, 50,
        220, 230, 60, 70,
    });

    const auto output = preprocessor.process(input);
    assert(output.id == input.id);
    assert(output.timestamp == input.timestamp);
    assert(output.width == 2);
    assert(output.height == 2);
    assert(output.format == vh::PixelFormat::Gray8);
    assert(output.data.size() == 4);
    assert(output.data[0] == 15);
    assert(output.data[1] == 115);
    assert(output.data[2] == 215);
    assert(output.data[3] == 55);

    auto malformed = input;
    malformed.data.pop_back();
    bool rejected = false;
    try {
        (void)preprocessor.process(malformed);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    return 0;
}
