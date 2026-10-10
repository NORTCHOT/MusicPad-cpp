#pragma once

#include "IMediaBackend.h"
#include <thread>
#include <atomic>
#include <mutex>

namespace musicpad::backend {

class MockMediaBackend : public IMediaBackend {
public:
    MockMediaBackend();
    ~MockMediaBackend() override;

    bool init() override;
    void shutdown() override;

    MediaInfo getNowPlaying() override;

    void play() override;
    void pause() override;
    void togglePlayPause() override;
    void next() override;
    void previous() override;
    void seek(int64_t positionMs) override;

    void setVolume(int volume) override;
    int getVolume() override;
    void setMute(bool mute) override;
    bool isMuted() override;

    void setOnChangeCallback(std::function<void(const MediaInfo&)> callback) override;

private:
    void notifyChange();
    void tickerLoop();

    mutable std::mutex mutex_;
    MediaInfo currentInfo_;
    std::function<void(const MediaInfo&)> changeCallback_;

    std::atomic<bool> running_{false};
    std::thread tickerThread_;
    int trackIndex_{0};
};

} // namespace musicpad::backend
