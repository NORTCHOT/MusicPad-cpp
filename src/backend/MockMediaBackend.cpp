#include "MockMediaBackend.h"
#include "../utils/Hash.h"
#include "../utils/Logger.h"
#include <chrono>

namespace musicpad::backend {

struct DummyTrack {
    std::string title;
    std::string artist;
    std::string album;
    int64_t durationMs;
};

static const DummyTrack kTracks[] = {
    {"Starboy", "The Weeknd ft. Daft Punk", "Starboy", 230000},
    {"Blinding Lights", "The Weeknd", "After Hours", 200000},
    {"Get Lucky", "Daft Punk ft. Pharrell Williams", "Random Access Memories", 248000}
};
static const size_t kTrackCount = sizeof(kTracks) / sizeof(kTracks[0]);

static const char* kMockArtwork = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";

MockMediaBackend::MockMediaBackend() {
    currentInfo_.hasMedia = true;
    currentInfo_.title = kTracks[0].title;
    currentInfo_.artist = kTracks[0].artist;
    currentInfo_.album = kTracks[0].album;
    currentInfo_.durationMs = kTracks[0].durationMs;
    currentInfo_.positionMs = 15000;
    currentInfo_.isPlaying = true;
    currentInfo_.volume = 65;
    currentInfo_.isMuted = false;
    currentInfo_.source = "MockPlayer";
    currentInfo_.artwork = kMockArtwork;
    currentInfo_.artworkHash = utils::computeHash(std::string(kMockArtwork));
}

MockMediaBackend::~MockMediaBackend() {
    shutdown();
}

bool MockMediaBackend::init() {
    utils::Logger::info("Initializing MockMediaBackend...");
    running_ = true;
    tickerThread_ = std::thread(&MockMediaBackend::tickerLoop, this);
    return true;
}

void MockMediaBackend::shutdown() {
    if (running_.exchange(false)) {
        if (tickerThread_.joinable()) {
            tickerThread_.join();
        }
    }
}

MediaInfo MockMediaBackend::getNowPlaying() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_;
}

void MockMediaBackend::play() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentInfo_.isPlaying) return;
        currentInfo_.isPlaying = true;
    }
    notifyChange();
}

void MockMediaBackend::pause() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!currentInfo_.isPlaying) return;
        currentInfo_.isPlaying = false;
    }
    notifyChange();
}

void MockMediaBackend::togglePlayPause() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.isPlaying = !currentInfo_.isPlaying;
    }
    notifyChange();
}

void MockMediaBackend::next() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        trackIndex_ = (trackIndex_ + 1) % kTrackCount;
        currentInfo_.title = kTracks[trackIndex_].title;
        currentInfo_.artist = kTracks[trackIndex_].artist;
        currentInfo_.album = kTracks[trackIndex_].album;
        currentInfo_.durationMs = kTracks[trackIndex_].durationMs;
        currentInfo_.positionMs = 0;
    }
    notifyChange();
}

void MockMediaBackend::previous() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        trackIndex_ = (trackIndex_ == 0) ? (kTrackCount - 1) : (trackIndex_ - 1);
        currentInfo_.title = kTracks[trackIndex_].title;
        currentInfo_.artist = kTracks[trackIndex_].artist;
        currentInfo_.album = kTracks[trackIndex_].album;
        currentInfo_.durationMs = kTracks[trackIndex_].durationMs;
        currentInfo_.positionMs = 0;
    }
    notifyChange();
}

void MockMediaBackend::seek(int64_t positionMs) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (positionMs < 0) positionMs = 0;
        if (positionMs > currentInfo_.durationMs) positionMs = currentInfo_.durationMs;
        currentInfo_.positionMs = positionMs;
    }
    notifyChange();
}

void MockMediaBackend::setVolume(int volume) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        currentInfo_.volume = volume;
    }
    notifyChange();
}

int MockMediaBackend::getVolume() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_.volume;
}

void MockMediaBackend::setMute(bool mute) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.isMuted = mute;
    }
    notifyChange();
}

bool MockMediaBackend::isMuted() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_.isMuted;
}

void MockMediaBackend::setOnChangeCallback(std::function<void(const MediaInfo&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    changeCallback_ = std::move(callback);
}

void MockMediaBackend::notifyChange() {
    std::function<void(const MediaInfo&)> cb;
    MediaInfo info;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = changeCallback_;
        info = currentInfo_;
    }
    if (cb) {
        cb(info);
    }
}

void MockMediaBackend::tickerLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!running_) break;

        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (currentInfo_.isPlaying) {
                currentInfo_.positionMs += 1000;
                if (currentInfo_.positionMs >= currentInfo_.durationMs) {
                    currentInfo_.positionMs = 0;
                }
                changed = true;
            }
        }
        if (changed) {
            notifyChange();
        }
    }
}

} // namespace musicpad::backend
