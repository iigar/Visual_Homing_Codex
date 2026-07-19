#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace vh {

std::string sha256_hex(std::span<const std::uint8_t> bytes);
std::string sha256_file_hex(const std::filesystem::path& path);
bool is_sha256_hex(const std::string& value);

} // namespace vh
