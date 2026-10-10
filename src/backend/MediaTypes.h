#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace musicpad::backend {

struct MediaCapabilities {
    bool canPlayPause = true;
    bool canNext = true;
    bool canPrev = true;
    bool canSeek = true;
    bool canSetVolume = true;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MediaCapabilities,
        canPlayPause,
        canNext,
        canPrev,
        canSeek,
        canSetVolume
    )
};

struct MediaInfo {
    bool hasMedia = false;
    std::string title;
    std::string artist;
    std::string album;
    std::string artwork;      // Base64 data URL e.g. "data:image/jpeg;base64,..."
    std::string artworkHash;  // FNV-1a hash of the artwork bytes
    int64_t positionMs = 0;
    int64_t durationMs = 0;
    bool isPlaying = false;
    int volume = 50;          // 0 - 100
    bool isMuted = false;
    std::string source;       // App name / session ID
    MediaCapabilities capabilities;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MediaInfo,
        hasMedia,
        title,
        artist,
        album,
        artwork,
        artworkHash,
        positionMs,
        durationMs,
        isPlaying,
        volume,
        isMuted,
        source,
        capabilities
    )
};

} // namespace musicpad::backend
