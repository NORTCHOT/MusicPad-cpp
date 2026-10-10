#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace musicpad::utils {

inline std::string computeHash(const uint8_t* data, size_t length) {
    // 64-bit FNV-1a
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < length; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ULL;
    }

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

inline std::string computeHash(const std::vector<uint8_t>& data) {
    return computeHash(data.data(), data.size());
}

inline std::string computeHash(const std::string& str) {
    return computeHash(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

} // namespace musicpad::utils
