#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace musicpad::utils {

inline std::string base64Encode(const uint8_t* data, size_t length) {
    static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((length + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < length) {
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16) |
                          (static_cast<uint32_t>(data[i + 1]) << 8) |
                          static_cast<uint32_t>(data[i + 2]);
        result.push_back(lookup[(triple >> 18) & 0x3F]);
        result.push_back(lookup[(triple >> 12) & 0x3F]);
        result.push_back(lookup[(triple >> 6) & 0x3F]);
        result.push_back(lookup[triple & 0x3F]);
        i += 3;
    }

    if (i < length) {
        size_t rem = length - i;
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16);
        if (rem == 2) {
            triple |= (static_cast<uint32_t>(data[i + 1]) << 8);
        }
        result.push_back(lookup[(triple >> 18) & 0x3F]);
        result.push_back(lookup[(triple >> 12) & 0x3F]);
        result.push_back(rem == 2 ? lookup[(triple >> 6) & 0x3F] : '=');
        result.push_back('=');
    }

    return result;
}

inline std::string base64Encode(const std::vector<uint8_t>& data) {
    return base64Encode(data.data(), data.size());
}

} // namespace musicpad::utils
