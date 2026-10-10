#pragma once

#include "MediaTypes.h"
#include <functional>

namespace musicpad::backend {

class IMediaBackend {
public:
    virtual ~IMediaBackend() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual MediaInfo getNowPlaying() = 0;
    virtual MediaInfo getArtwork() { return getNowPlaying(); }

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void togglePlayPause() = 0;
    virtual void next() = 0;
    virtual void previous() = 0;
    virtual void seek(int64_t positionMs) = 0;

    virtual void setVolume(int volume) = 0;
    virtual int getVolume() = 0;
    virtual void setMute(bool mute) = 0;
    virtual bool isMuted() = 0;

    virtual void setOnChangeCallback(std::function<void(const MediaInfo&)> callback) = 0;
};

} // namespace musicpad::backend
