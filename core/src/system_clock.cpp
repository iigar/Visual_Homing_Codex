#include "visual_homing/time.hpp"

namespace vh {

Timestamp now() {
    return Clock::now();
}

double milliseconds_between(Timestamp start, Timestamp end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace vh
