#include <cassert>
#include <stdexcept>

#include "visual_homing/pi_camera_source.hpp"

int main() {
    vh::PiCameraSource source({.width = 160, .height = 120, .frame_rate_hz = 10, .format = vh::PixelFormat::Gray8});

    assert(source.config().width == 160);
    assert(source.config().height == 120);
    assert(source.config().frame_rate_hz == 10);
    assert(!source.running());

    assert(!source.start());
    assert(!source.running());
    assert(!source.last_error().empty());
    assert(!source.poll().has_value());

    source.stop();
    assert(!source.running());

    bool rejected_dimensions = false;
    try {
        (void)vh::PiCameraSource({.width = 0, .height = 120, .frame_rate_hz = 10});
    } catch (const std::invalid_argument&) {
        rejected_dimensions = true;
    }
    assert(rejected_dimensions);

    bool rejected_rate = false;
    try {
        (void)vh::PiCameraSource({.width = 160, .height = 120, .frame_rate_hz = 0});
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    assert(rejected_rate);

    bool rejected_format = false;
    try {
        (void)vh::PiCameraSource({
            .width = 160,
            .height = 120,
            .frame_rate_hz = 10,
            .format = vh::PixelFormat::Bgr8,
        });
    } catch (const std::invalid_argument&) {
        rejected_format = true;
    }
    assert(rejected_format);

    return 0;
}
