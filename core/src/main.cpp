#include <iostream>

#include "visual_homing/time.hpp"

int main() {
    const auto started = vh::now();
    std::cout << "Visual Homing Core boot\n";
    std::cout << "Realtime C++ core skeleton ready\n";
    std::cout << "uptime_ms=" << vh::milliseconds_between(started, vh::now()) << "\n";
    return 0;
}
