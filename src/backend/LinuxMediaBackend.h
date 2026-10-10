#pragma once

#if defined(__linux__)

#include "IMediaBackend.h"
#include <sdbus-c++/sdbus-c++.h>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <string>
#include <memory>
#include <map>

namespace musicpad::backend {

class LinuxMediaBackend : public IMediaBackend {
public:
    LinuxMediaBackend();
    ~LinuxMediaBackend() override;

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
    void findAndAttachPlayer();
    void attachPlayer(const std::string& busName);
    void detachPlayer();
    void refreshAllProperties();
    void updateMetadata(const std::map<std::string, sdbus::Variant>& metadata);
    void updatePlaybackStatus(const std::string& status);
    void updatePosition();
    void loadArtwork(const std::string& artUrl);

    void notifyChange();
    void tickerLoop();
    void refreshSystemVolume();

    mutable std::mutex mutex_;
    MediaInfo currentInfo_;
    std::function<void(const MediaInfo&)> changeCallback_;

    std::unique_ptr<sdbus::IConnection> connection_;
    std::unique_ptr<sdbus::IProxy> dbusProxy_;
    std::unique_ptr<sdbus::IProxy> playerProxy_;

    std::string activePlayerName_;
    sdbus::ObjectPath currentTrackPath_{"/"};
    std::string lastArtworkUrl_;

    // Position timing
    int64_t lastAuthoritativePosMs_{0};
    std::chrono::steady_clock::time_point lastPositionTime_;

    std::atomic<bool> running_{false};
    std::thread tickerThread_;
};

} // namespace musicpad::backend

#endif // __linux__
