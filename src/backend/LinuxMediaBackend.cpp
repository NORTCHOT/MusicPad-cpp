#if defined(__linux__)

#include "LinuxMediaBackend.h"
#include "../utils/Base64.h"
#include "../utils/Hash.h"
#include "../utils/Logger.h"

#include <unistd.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <array>
#include <chrono>

namespace musicpad::backend {

static std::string execCommand(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

LinuxMediaBackend::LinuxMediaBackend() {
    currentInfo_.hasMedia = false;
}

LinuxMediaBackend::~LinuxMediaBackend() {
    shutdown();
}

bool LinuxMediaBackend::init() {
    utils::Logger::info("Initializing LinuxMediaBackend (MPRIS2 / PulseAudio)...");

    // Ensure DBUS_SESSION_BUS_ADDRESS is set (especially important when run over SSH)
    if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
        std::string path = "unix:path=/run/user/" + std::to_string(getuid()) + "/bus";
        setenv("DBUS_SESSION_BUS_ADDRESS", path.c_str(), 0);
        utils::Logger::info("Set DBUS_SESSION_BUS_ADDRESS to " + path);
    }

    try {
        connection_ = sdbus::createSessionBusConnection();
        connection_->enterEventLoopAsync();

        dbusProxy_ = sdbus::createProxy(*connection_,
                                        sdbus::ServiceName{"org.freedesktop.DBus"},
                                        sdbus::ObjectPath{"/org/freedesktop/DBus"});

        dbusProxy_->uponSignal("NameOwnerChanged")
            .onInterface(sdbus::InterfaceName{"org.freedesktop.DBus"})
            .call([this](const std::string& name, const std::string& oldOwner, const std::string& newOwner) {
                if (name.rfind("org.mpris.MediaPlayer2.", 0) == 0) {
                    if (!newOwner.empty() && activePlayerName_.empty()) {
                        utils::Logger::info("New MPRIS player appeared: " + name);
                        attachPlayer(name);
                    } else if (name == activePlayerName_ && newOwner.empty()) {
                        utils::Logger::info("Current MPRIS player closed: " + name);
                        findAndAttachPlayer();
                    }
                }
            });

        findAndAttachPlayer();
    } catch (const sdbus::Error& e) {
        utils::Logger::error(std::string("sdbus-c++ connection error: ") + e.getMessage());
    }

    refreshSystemVolume();

    running_ = true;
    tickerThread_ = std::thread(&LinuxMediaBackend::tickerLoop, this);

    utils::Logger::info("LinuxMediaBackend initialized successfully.");
    return true;
}

void LinuxMediaBackend::shutdown() {
    if (running_.exchange(false)) {
        if (tickerThread_.joinable()) {
            tickerThread_.join();
        }
    }

    detachPlayer();

    if (dbusProxy_) {
        dbusProxy_.reset();
    }
    if (connection_) {
        connection_.reset();
    }
}

void LinuxMediaBackend::findAndAttachPlayer() {
    detachPlayer();

    if (!dbusProxy_) return;

    try {
        std::vector<std::string> names;
        dbusProxy_->callMethod("ListNames")
            .onInterface(sdbus::InterfaceName{"org.freedesktop.DBus"})
            .storeResultsTo(names);

        for (const auto& name : names) {
            if (name.rfind("org.mpris.MediaPlayer2.", 0) == 0) {
                attachPlayer(name);
                return;
            }
        }
    } catch (const sdbus::Error& e) {
        utils::Logger::warn(std::string("findAndAttachPlayer DBus error: ") + e.getMessage());
    }

    // No active player found
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.hasMedia = false;
        currentInfo_.isPlaying = false;
        currentInfo_.title = "";
        currentInfo_.artist = "";
        currentInfo_.album = "";
        currentInfo_.artwork = "";
        currentInfo_.artworkHash = "";
        currentInfo_.positionMs = 0;
        currentInfo_.durationMs = 0;
        lastArtworkUrl_.clear();
    }
    notifyChange();
}

void LinuxMediaBackend::attachPlayer(const std::string& busName) {
    if (!connection_) return;

    try {
        activePlayerName_ = busName;
        playerProxy_ = sdbus::createProxy(*connection_,
                                          sdbus::ServiceName{busName},
                                          sdbus::ObjectPath{"/org/mpris/MediaPlayer2"});

        playerProxy_->uponSignal("PropertiesChanged")
            .onInterface(sdbus::InterfaceName{"org.freedesktop.DBus.Properties"})
            .call([this](const std::string& interfaceName,
                           const std::map<std::string, sdbus::Variant>& changedProperties,
                           const std::vector<std::string>& /*invalidated*/) {
                if (interfaceName == "org.mpris.MediaPlayer2.Player") {
                    auto itMeta = changedProperties.find("Metadata");
                    if (itMeta != changedProperties.end()) {
                        try {
                            auto meta = itMeta->second.get<std::map<std::string, sdbus::Variant>>();
                            updateMetadata(meta);
                        } catch (...) {}
                    }

                    auto itStatus = changedProperties.find("PlaybackStatus");
                    if (itStatus != changedProperties.end()) {
                        try {
                            std::string status = itStatus->second.get<std::string>();
                            updatePlaybackStatus(status);
                        } catch (...) {}
                    }

                    notifyChange();
                }
            });

        playerProxy_->uponSignal("Seeked")
            .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"})
            .call([this](int64_t positionUs) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    currentInfo_.positionMs = positionUs / 1000;
                    lastAuthoritativePosMs_ = positionUs / 1000;
                    lastPositionTime_ = std::chrono::steady_clock::now();
                }
                notifyChange();
            });

        utils::Logger::info("Attached to MPRIS player: " + busName);
        refreshAllProperties();
        notifyChange();
    } catch (const sdbus::Error& e) {
        utils::Logger::warn("Failed to attach to player " + busName + ": " + e.getMessage());
        detachPlayer();
    }
}

void LinuxMediaBackend::detachPlayer() {
    activePlayerName_.clear();
    playerProxy_.reset();
}

void LinuxMediaBackend::refreshAllProperties() {
    if (!playerProxy_) return;

    try {
        // Query PlaybackStatus
        auto statusVar = playerProxy_->getProperty("PlaybackStatus")
                             .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
        updatePlaybackStatus(statusVar.get<std::string>());

        // Query Metadata
        auto metaVar = playerProxy_->getProperty("Metadata")
                           .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
        auto meta = metaVar.get<std::map<std::string, sdbus::Variant>>();
        updateMetadata(meta);

        // Capabilities
        bool canNext = false;
        bool canPrev = false;
        bool canPlay = false;
        bool canPause = false;
        bool canSeek = false;

        try {
            canNext = playerProxy_->getProperty("CanGoNext")
                          .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"}).get<bool>();
            canPrev = playerProxy_->getProperty("CanGoPrevious")
                          .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"}).get<bool>();
            canPlay = playerProxy_->getProperty("CanPlay")
                          .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"}).get<bool>();
            canPause = playerProxy_->getProperty("CanPause")
                           .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"}).get<bool>();
            canSeek = playerProxy_->getProperty("CanSeek")
                          .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"}).get<bool>();
        } catch (...) {}

        updatePosition();

        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.hasMedia = true;
        currentInfo_.source = activePlayerName_;
        currentInfo_.capabilities.canNext = canNext;
        currentInfo_.capabilities.canPrev = canPrev;
        currentInfo_.capabilities.canPlayPause = canPlay || canPause;
        currentInfo_.capabilities.canSeek = canSeek;
    } catch (const sdbus::Error& e) {
        utils::Logger::warn(std::string("refreshAllProperties error: ") + e.getMessage());
    }
}

void LinuxMediaBackend::updatePlaybackStatus(const std::string& status) {
    bool isPlaying = (status == "Playing");
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentInfo_.isPlaying && !isPlaying) {
        // Pausing: lock position so it does not jump backwards
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPositionTime_).count();
        int64_t calculated = lastAuthoritativePosMs_ + elapsedMs;
        if (currentInfo_.durationMs > 0 && calculated > currentInfo_.durationMs) {
            calculated = currentInfo_.durationMs;
        }
        currentInfo_.positionMs = calculated;
        lastAuthoritativePosMs_ = calculated;
        lastPositionTime_ = now;
    } else if (!currentInfo_.isPlaying && isPlaying) {
        lastPositionTime_ = std::chrono::steady_clock::now();
    }
    currentInfo_.isPlaying = isPlaying;
}

void LinuxMediaBackend::updatePosition() {
    if (!playerProxy_) return;
    try {
        int64_t posUs = playerProxy_->getProperty("Position")
                            .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"}).get<int64_t>();
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.positionMs = posUs / 1000;
        lastAuthoritativePosMs_ = posUs / 1000;
        lastPositionTime_ = std::chrono::steady_clock::now();
    } catch (...) {}
}

void LinuxMediaBackend::updateMetadata(const std::map<std::string, sdbus::Variant>& meta) {
    std::string title;
    std::string artist;
    std::string album;
    std::string artUrl;
    int64_t durationMs = 0;

    auto itTitle = meta.find("xesam:title");
    if (itTitle != meta.end()) {
        try { title = itTitle->second.get<std::string>(); } catch (...) {}
    }

    auto itArtist = meta.find("xesam:artist");
    if (itArtist != meta.end()) {
        try {
            auto artistList = itArtist->second.get<std::vector<std::string>>();
            for (size_t i = 0; i < artistList.size(); ++i) {
                if (i > 0) artist += ", ";
                artist += artistList[i];
            }
        } catch (...) {
            try { artist = itArtist->second.get<std::string>(); } catch (...) {}
        }
    }

    auto itAlbum = meta.find("xesam:album");
    if (itAlbum != meta.end()) {
        try { album = itAlbum->second.get<std::string>(); } catch (...) {}
    }

    auto itLength = meta.find("mpris:length");
    if (itLength != meta.end()) {
        try {
            int64_t lengthUs = itLength->second.get<int64_t>();
            durationMs = lengthUs / 1000;
        } catch (...) {
            try {
                uint64_t lengthUs = itLength->second.get<uint64_t>();
                durationMs = static_cast<int64_t>(lengthUs / 1000);
            } catch (...) {}
        }
    }

    auto itTrackId = meta.find("mpris:trackid");
    if (itTrackId != meta.end()) {
        try { 
            currentTrackPath_ = itTrackId->second.get<sdbus::ObjectPath>(); 
        } catch (...) {
            try {
                currentTrackPath_ = sdbus::ObjectPath(itTrackId->second.get<std::string>());
            } catch (...) {
                currentTrackPath_ = sdbus::ObjectPath("/");
            }
        }
    }

    auto itArtUrl = meta.find("mpris:artUrl");
    if (itArtUrl != meta.end()) {
        try { artUrl = itArtUrl->second.get<std::string>(); } catch (...) {}
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool trackChanged = (title != currentInfo_.title || artist != currentInfo_.artist);
        currentInfo_.hasMedia = true;
        currentInfo_.title = title;
        currentInfo_.artist = artist;
        currentInfo_.album = album;
        if (trackChanged) {
            currentInfo_.durationMs = durationMs;
        } else if (durationMs > 0) {
            currentInfo_.durationMs = durationMs;
        }
    }

    loadArtwork(artUrl);
    utils::Logger::info("Track info: \"" + title + "\" by " + artist + " [" + album + "]");
}

void LinuxMediaBackend::loadArtwork(const std::string& artUrl) {
    if (artUrl.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.artwork.clear();
        currentInfo_.artworkHash.clear();
        lastArtworkUrl_.clear();
        return;
    }

    if (artUrl == lastArtworkUrl_) return;
    lastArtworkUrl_ = artUrl;

    if (artUrl.rfind("file://", 0) == 0) {
        std::string filePath = artUrl.substr(7);
        std::ifstream file(filePath, std::ios::binary);
        if (file) {
            std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
            if (!buffer.empty()) {
                std::string mime = "image/jpeg";
                if (filePath.ends_with(".png")) mime = "image/png";
                else if (filePath.ends_with(".webp")) mime = "image/webp";

                std::lock_guard<std::mutex> lock(mutex_);
                currentInfo_.artwork = "data:" + mime + ";base64," + utils::base64Encode(buffer);
                currentInfo_.artworkHash = utils::computeHash(buffer);
                return;
            }
        }
    } else if (artUrl.rfind("data:", 0) == 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.artwork = artUrl;
        currentInfo_.artworkHash = utils::computeHash(artUrl);
        return;
    } else if (artUrl.rfind("http://", 0) == 0 || artUrl.rfind("https://", 0) == 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.artwork = artUrl;
        currentInfo_.artworkHash = utils::computeHash(artUrl);
        return;
    }
}

MediaInfo LinuxMediaBackend::getNowPlaying() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_;
}

void LinuxMediaBackend::play() {
    if (!playerProxy_) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.isPlaying = true;
        lastPositionTime_ = std::chrono::steady_clock::now();
    }
    notifyChange();
    try {
        playerProxy_->callMethod("Play").onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
    } catch (const sdbus::Error& e) {
        utils::Logger::error("MPRIS Play error: " + e.getMessage());
    }
}

void LinuxMediaBackend::pause() {
    if (!playerProxy_) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentInfo_.isPlaying) {
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPositionTime_).count();
            int64_t calculated = lastAuthoritativePosMs_ + elapsedMs;
            if (currentInfo_.durationMs > 0 && calculated > currentInfo_.durationMs) {
                calculated = currentInfo_.durationMs;
            }
            currentInfo_.positionMs = calculated;
            lastAuthoritativePosMs_ = calculated;
            lastPositionTime_ = now;
        }
        currentInfo_.isPlaying = false;
    }
    notifyChange();
    try {
        playerProxy_->callMethod("Pause").onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
    } catch (const sdbus::Error& e) {
        utils::Logger::error("MPRIS Pause error: " + e.getMessage());
    }
}

void LinuxMediaBackend::togglePlayPause() {
    if (!playerProxy_) return;
    bool newPlaying = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        newPlaying = !currentInfo_.isPlaying;
        currentInfo_.isPlaying = newPlaying;
        if (!newPlaying) {
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPositionTime_).count();
            int64_t calculated = lastAuthoritativePosMs_ + elapsedMs;
            if (currentInfo_.durationMs > 0 && calculated > currentInfo_.durationMs) {
                calculated = currentInfo_.durationMs;
            }
            currentInfo_.positionMs = calculated;
            lastAuthoritativePosMs_ = calculated;
            lastPositionTime_ = now;
        } else {
            lastPositionTime_ = std::chrono::steady_clock::now();
        }
    }
    notifyChange();
    try {
        playerProxy_->callMethod("PlayPause").onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
    } catch (const sdbus::Error& e) {
        utils::Logger::error("MPRIS PlayPause error: " + e.getMessage());
    }
}

void LinuxMediaBackend::next() {
    if (!playerProxy_) return;
    try {
        playerProxy_->callMethod("Next").onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
    } catch (const sdbus::Error& e) {
        utils::Logger::error("MPRIS Next error: " + e.getMessage());
    }
}

void LinuxMediaBackend::previous() {
    if (!playerProxy_) return;
    try {
        playerProxy_->callMethod("Previous").onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
    } catch (const sdbus::Error& e) {
        utils::Logger::error("MPRIS Previous error: " + e.getMessage());
    }
}

void LinuxMediaBackend::seek(int64_t positionMs) {
    if (!playerProxy_) return;
    
    int64_t currentPosMs = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentPosMs = currentInfo_.positionMs;
        currentInfo_.positionMs = positionMs;
        lastAuthoritativePosMs_ = positionMs;
        lastPositionTime_ = std::chrono::steady_clock::now();
    }
    notifyChange();

    int64_t offsetUs = (positionMs - currentPosMs) * 1000;
    int64_t positionUs = positionMs * 1000;

    // Web browsers (Firefox/Chromium) often have issues with SetPosition on HTML5 SPA players (e.g. YouTube),
    // and their MPRIS Seek implementation triggers fixed 5-second steps per call.
    bool isBrowser = (activePlayerName_.find("firefox") != std::string::npos || 
                      activePlayerName_.find("chromium") != std::string::npos ||
                      activePlayerName_.find("chrome") != std::string::npos);

    if (!isBrowser && !currentTrackPath_.empty() && currentTrackPath_ != "/") {
        try {
            playerProxy_->callMethod("SetPosition")
                .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"})
                .withArguments(currentTrackPath_, positionUs);
            utils::Logger::info("MPRIS SetPosition called for " + std::to_string(positionMs) + "ms");
            return;
        } catch (const sdbus::Error& e) {
            utils::Logger::warn("MPRIS SetPosition error: " + e.getMessage() + ", falling back to Seek");
        }
    }

    // For browsers or players where SetPosition is unsupported:
    // In Firefox, each Seek call jumps by 5 seconds (5,000,000 us).
    // Calculate how many 5s steps are needed to cover the requested offset.
    int64_t stepCount = std::llround(static_cast<double>(offsetUs) / 5000000.0);
    if (stepCount == 0 && offsetUs != 0) {
        stepCount = (offsetUs > 0) ? 1 : -1;
    }

    // Bound maximum steps to prevent runaway loops (max +/- 30 steps = 150 seconds)
    if (stepCount > 30) stepCount = 30;
    if (stepCount < -30) stepCount = -30;

    int absSteps = std::abs(static_cast<int>(stepCount));
    int64_t singleStepUs = (stepCount > 0) ? 5000000 : -5000000;

    for (int i = 0; i < absSteps; ++i) {
        try {
            playerProxy_->callMethod("Seek")
                .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"})
                .withArguments(singleStepUs);
        } catch (const sdbus::Error& e) {
            utils::Logger::warn("MPRIS Seek step failed: " + e.getMessage());
            break;
        }
    }
    utils::Logger::info("MPRIS Seek performed " + std::to_string(absSteps) + " steps of " + 
                        std::to_string(singleStepUs / 1000) + "ms (total offset: " + 
                        std::to_string(offsetUs / 1000) + "ms)");
}

void LinuxMediaBackend::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    execCommand("pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(volume) + "%");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.volume = volume;
    }
    notifyChange();
}

int LinuxMediaBackend::getVolume() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_.volume;
}

void LinuxMediaBackend::setMute(bool mute) {
    execCommand(std::string("pactl set-sink-mute @DEFAULT_SINK@ ") + (mute ? "1" : "0"));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.isMuted = mute;
    }
    notifyChange();
}

bool LinuxMediaBackend::isMuted() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_.isMuted;
}

void LinuxMediaBackend::setOnChangeCallback(std::function<void(const MediaInfo&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    changeCallback_ = std::move(callback);
}

void LinuxMediaBackend::notifyChange() {
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

void LinuxMediaBackend::refreshSystemVolume() {
    std::string volOut = execCommand("pactl get-sink-volume @DEFAULT_SINK@");
    std::string muteOut = execCommand("pactl get-sink-mute @DEFAULT_SINK@");

    int vol = 50;
    std::smatch m;
    std::regex reg(R"(/\s*(\d+)%\s*/)");
    if (std::regex_search(volOut, m, reg) && m.size() > 1) {
        try { vol = std::stoi(m[1].str()); } catch (...) {}
    }

    bool muted = (muteOut.find("yes") != std::string::npos);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.volume = vol;
        currentInfo_.isMuted = muted;
    }
}

void LinuxMediaBackend::tickerLoop() {
    int counter = 0;
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!running_) break;

        bool needsNotify = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (currentInfo_.hasMedia && currentInfo_.isPlaying) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPositionTime_).count();
                int64_t calculated = lastAuthoritativePosMs_ + elapsedMs;
                if (currentInfo_.durationMs > 0 && calculated > currentInfo_.durationMs) {
                    calculated = currentInfo_.durationMs;
                }
                if (calculated / 1000 != currentInfo_.positionMs / 1000) {
                    needsNotify = true;
                }
                currentInfo_.positionMs = calculated;
            }
        }

        // If duration is 0 while playing, re-query Metadata (Fix for Issue 7: 0:00/0:00 in browsers)
        if (playerProxy_) {
            bool needDurationCheck = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                needDurationCheck = (currentInfo_.hasMedia && currentInfo_.durationMs <= 0);
            }
            if (needDurationCheck) {
                try {
                    auto metaVar = playerProxy_->getProperty("Metadata")
                                       .onInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2.Player"});
                    auto meta = metaVar.get<std::map<std::string, sdbus::Variant>>();
                    updateMetadata(meta);
                    needsNotify = true;
                } catch (...) {}
            }
        }

        // Check external volume changes every 1 second (every 2 iterations)
        if (++counter >= 2) {
            counter = 0;
            int oldVol = 0;
            bool oldMute = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                oldVol = currentInfo_.volume;
                oldMute = currentInfo_.isMuted;
            }
            refreshSystemVolume();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (currentInfo_.volume != oldVol || currentInfo_.isMuted != oldMute) {
                    needsNotify = true;
                }
            }
        }

        if (needsNotify) {
            notifyChange();
        }
    }
}

} // namespace musicpad::backend

#endif // __linux__
