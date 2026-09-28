#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>

namespace mavlink_test {

inline unsigned crc_extra(unsigned id) {
    switch (id) {
    case 0: return 50;
    case 30: return 39;
    case 33: return 104;
    case 100: return 175;
    case 106: return 138;
    case 132: return 85;
    case 141: return 47;
    default: return 0; // Unknown test message, never decoded by production.
    }
}

// Deliberately simple bitwise reference, independent of the production update.
inline void finish_crc(std::string& frame, unsigned extra) {
    std::uint16_t crc = 0xffff;
    const auto add = [&](unsigned byte) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
    };
    for (std::size_t i = 1; i < frame.size(); ++i) add(static_cast<unsigned char>(frame[i]));
    add(extra);
    frame.push_back(static_cast<char>(crc));
    frame.push_back(static_cast<char>(crc >> 8));
}

inline std::string from_hex(std::string_view hex) {
    assert(hex.size() % 2 == 0);
    std::string bytes;
    const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < hex.size(); i += 2)
        bytes.push_back(static_cast<char>((digit(hex[i]) << 4) | digit(hex[i + 1])));
    return bytes;
}

} // namespace mavlink_test
