#pragma once

#if defined(_WIN32)

#include "IMediaBackend.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>

namespace musicpad::backend {

class VolumeNotificationCallback;

class WindowsMediaBackend : public IMediaBackend {
public:
    WindowsMediaBackend();
    ~WindowsMediaBackend() override;

    bool init() override;
    void shutdown() override;

    MediaInfo getNowPlaying() override;
    MediaInfo getArtwork() override;

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
    void initAudioEndpoint();
    void cleanupAudioEndpoint();
    void refreshSession();
    void refreshMediaProperties();
    bool tryExtractThumbnail(const winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties& props);
    void refreshPlaybackInfo();
    void refreshTimeline();
    void notifyChange();
    void scheduleDebouncedNotify(int delayMs = 60);
    void debounceLoop();
    void tickerLoop();

    // Volume callback event
    void onVolumeNotification(float newVolume, bool muted);

    mutable std::mutex mutex_;
    MediaInfo currentInfo_;
    std::function<void(const MediaInfo&)> changeCallback_;

    // Debounce notification synchronization
    std::mutex debounceMutex_;
    std::condition_variable debounceCv_;
    std::chrono::steady_clock::time_point debounceTargetTime_;
    bool debouncePending_{false};
    std::thread debounceThread_;

    // WinRT GSMTC members
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager sessionManager_{nullptr};
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession currentSession_{nullptr};

    winrt::event_token sessionChangedToken_{};
    winrt::event_token mediaPropertiesToken_{};
    winrt::event_token playbackInfoToken_{};
    winrt::event_token timelineToken_{};

    // Cached artwork
    std::string lastArtworkTitle_;
    std::string lastArtworkArtist_;

    // Position timing
    int64_t lastAuthoritativePosMs_{0};
    std::chrono::steady_clock::time_point lastPositionTime_;

    // Core Audio WASAPI
    IMMDeviceEnumerator* deviceEnumerator_{nullptr};
    IMMDevice* defaultDevice_{nullptr};
    IAudioEndpointVolume* endpointVolume_{nullptr};
    VolumeNotificationCallback* volumeCallback_{nullptr};

    // GDI+ token for artwork optimization
    ULONG_PTR gdiplusToken_{0};

    std::atomic<bool> running_{false};
    std::thread tickerThread_;

    friend class VolumeNotificationCallback;
};

} // namespace musicpad::backend

#endif // _WIN32
