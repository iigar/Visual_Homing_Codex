#include <cassert>
#include <stdexcept>
#include <string>

#include "visual_homing/mavlink_telemetry_stream.hpp"

int main() {
    vh::MavlinkTelemetryByteBuffer buffer(8);
    buffer.append("abcd", 4);
    assert(buffer.bytes() == "abcd");
    assert(buffer.bytes_captured() == 4);
    assert(buffer.bytes_retained() == 4);
    assert(buffer.bytes_dropped() == 0);

    buffer.append("efgh", 4);
    assert(buffer.bytes() == "abcdefgh");
    assert(buffer.bytes_captured() == 8);
    assert(buffer.bytes_retained() == 8);
    assert(buffer.bytes_dropped() == 0);

    buffer.append("ij", 2);
    assert(buffer.bytes() == "cdefghij");
    assert(buffer.bytes_captured() == 10);
    assert(buffer.bytes_retained() == 8);
    assert(buffer.bytes_dropped() == 2);

    buffer.append("0123456789", 10);
    assert(buffer.bytes() == "23456789");
    assert(buffer.bytes_captured() == 20);
    assert(buffer.bytes_retained() == 8);
    assert(buffer.bytes_dropped() == 12);

    buffer.clear();
    assert(buffer.bytes().empty());
    assert(buffer.bytes_captured() == 0);
    assert(buffer.bytes_retained() == 0);
    assert(buffer.bytes_dropped() == 0);

    bool rejected = false;
    try {
        (void)vh::MavlinkTelemetryByteBuffer(0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    return 0;
}
