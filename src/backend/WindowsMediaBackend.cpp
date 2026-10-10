#if defined(_WIN32)

#include "WindowsMediaBackend.h"
#include "../utils/Base64.h"
#include "../utils/Hash.h"
#include "../utils/Logger.h"

#include <winrt/Windows.Storage.Streams.h>
#include <chrono>
#include <algorithm>
#include <gdiplus.h>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

namespace {

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    std::vector<uint8_t> buffer(size);
    Gdiplus::ImageCodecInfo* pImageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            return static_cast<int>(j);
        }
    }
    return -1;
}

std::vector<uint8_t> optimizeArtworkImage(const std::vector<uint8_t>& inputBytes, std::string& inOutMimeType) {
    // If image is already compact (<= 120 KB), keep original bytes
    if (inputBytes.size() <= 120 * 1024) {
        return inputBytes;
    }

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, inputBytes.size());
    if (!hGlobal) return inputBytes;

    void* pData = GlobalLock(hGlobal);
    if (!pData) {
        GlobalFree(hGlobal);
        return inputBytes;
    }
    memcpy(pData, inputBytes.data(), inputBytes.size());
    GlobalUnlock(hGlobal);

    IStream* pStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hGlobal, TRUE, &pStream)) || !pStream) {
        GlobalFree(hGlobal);
        return inputBytes;
    }

    std::vector<uint8_t> resultBytes = inputBytes;

    do {
        std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromStream(pStream));
        if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok) break;

        UINT origWidth = bmp->GetWidth();
        UINT origHeight = bmp->GetHeight();
        if (origWidth == 0 || origHeight == 0) break;

        // Target max dimension: 512px (sufficient for high-density phone screens, lightweight)
        const UINT maxDim = 512;
        UINT newWidth = origWidth;
        UINT newHeight = origHeight;

        if (origWidth > maxDim || origHeight > maxDim) {
            if (origWidth >= origHeight) {
                newWidth = maxDim;
                newHeight = static_cast<UINT>(static_cast<double>(origHeight) * maxDim / origWidth);
            } else {
                newHeight = maxDim;
                newWidth = static_cast<UINT>(static_cast<double>(origWidth) * maxDim / origHeight);
            }
        }

        if (newWidth == 0) newWidth = 1;
        if (newHeight == 0) newHeight = 1;

        std::unique_ptr<Gdiplus::Bitmap> scaledBmp(new Gdiplus::Bitmap(newWidth, newHeight, PixelFormat24bppRGB));
        if (!scaledBmp || scaledBmp->GetLastStatus() != Gdiplus::Ok) break;

        {
            Gdiplus::Graphics g(scaledBmp.get());
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            g.DrawImage(bmp.get(), 0, 0, newWidth, newHeight);
        }

        CLSID jpegClsid;
        if (GetEncoderClsid(L"image/jpeg", &jpegClsid) < 0) break;

        Gdiplus::EncoderParameters encoderParameters;
        encoderParameters.Count = 1;
        encoderParameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
        encoderParameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        encoderParameters.Parameter[0].NumberOfValues = 1;
        ULONG quality = 80;
        encoderParameters.Parameter[0].Value = &quality;

        IStream* pOutStream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &pOutStream)) || !pOutStream) break;

        if (scaledBmp->Save(pOutStream, &jpegClsid, &encoderParameters) == Gdiplus::Ok) {
            STATSTG stat;
            if (SUCCEEDED(pOutStream->Stat(&stat, STATFLAG_NONAME))) {
                ULONG outSize = static_cast<ULONG>(stat.cbSize.QuadPart);
                if (outSize > 0 && outSize < inputBytes.size()) {
                    std::vector<uint8_t> compressed(outSize);
                    LARGE_INTEGER zero = {};
                    pOutStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                    ULONG bytesRead = 0;
                    if (SUCCEEDED(pOutStream->Read(compressed.data(), outSize, &bytesRead)) && bytesRead == outSize) {
                        resultBytes = std::move(compressed);
                        inOutMimeType = "image/jpeg";
                        musicpad::utils::Logger::debug("Artwork optimized: " + std::to_string(inputBytes.size()) +
                                                       " -> " + std::to_string(resultBytes.size()) + " bytes");
                    }
                }
            }
        }
        pOutStream->Release();
    } while (false);

    pStream->Release();
    return resultBytes;
}

} // anonymous namespace

namespace musicpad::backend {

class VolumeNotificationCallback : public IAudioEndpointVolumeCallback {
public:
    explicit VolumeNotificationCallback(WindowsMediaBackend* backend)
        : backend_(backend), refCount_(1) {}
    virtual ~VolumeNotificationCallback() = default;

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG ulRef = InterlockedDecrement(&refCount_);
        if (ulRef == 0) {
            delete this;
        }
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) override {
        if (riid == __uuidof(IUnknown)) {
            AddRef();
            *ppvInterface = static_cast<IUnknown*>(this);
            return S_OK;
        }
        if (riid == __uuidof(IAudioEndpointVolumeCallback)) {
            AddRef();
            *ppvInterface = static_cast<IAudioEndpointVolumeCallback*>(this);
            return S_OK;
        }
        *ppvInterface = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override {
        if (pNotify && backend_) {
            backend_->onVolumeNotification(pNotify->fMasterVolume, pNotify->bMuted != FALSE);
        }
        return S_OK;
    }

private:
    WindowsMediaBackend* backend_;
    LONG refCount_;
};

WindowsMediaBackend::WindowsMediaBackend() {
    currentInfo_.hasMedia = false;
}

WindowsMediaBackend::~WindowsMediaBackend() {
    shutdown();
}

bool WindowsMediaBackend::init() {
    utils::Logger::info("Initializing WindowsMediaBackend...");
    try {
        init_apartment(apartment_type::multi_threaded);
    } catch (...) {
        // Apartment might already be initialized on this thread
    }

    initAudioEndpoint();

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr);

    try {
        sessionManager_ = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        if (sessionManager_) {
            sessionChangedToken_ = sessionManager_.CurrentSessionChanged(
                [this](auto&&, auto&&) {
                    utils::Logger::info("GSMTC: Current session changed");
                    refreshSession();
                    scheduleDebouncedNotify(60);
                }
            );
            refreshSession();
        } else {
            utils::Logger::warn("GSMTC: SessionManager request returned null");
        }
    } catch (const hresult_error& ex) {
        utils::Logger::error(std::string("GSMTC SessionManager error: ") + to_string(ex.message()));
    } catch (const std::exception& ex) {
        utils::Logger::error(std::string("GSMTC exception: ") + ex.what());
    }

    running_ = true;
    debounceThread_ = std::thread(&WindowsMediaBackend::debounceLoop, this);
    tickerThread_ = std::thread(&WindowsMediaBackend::tickerLoop, this);

    utils::Logger::info("WindowsMediaBackend initialized successfully.");
    return true;
}

void WindowsMediaBackend::shutdown() {
    if (running_.exchange(false)) {
        debounceCv_.notify_all();
        if (debounceThread_.joinable()) {
            debounceThread_.join();
        }
        if (tickerThread_.joinable()) {
            tickerThread_.join();
        }
    }

    try {
        if (currentSession_) {
            if (mediaPropertiesToken_.value) currentSession_.MediaPropertiesChanged(mediaPropertiesToken_);
            if (playbackInfoToken_.value) currentSession_.PlaybackInfoChanged(playbackInfoToken_);
            if (timelineToken_.value) currentSession_.TimelinePropertiesChanged(timelineToken_);
            currentSession_ = nullptr;
        }

        if (sessionManager_ && sessionChangedToken_.value) {
            sessionManager_.CurrentSessionChanged(sessionChangedToken_);
            sessionManager_ = nullptr;
        }
    } catch (...) {
        // Suppress WinRT exceptions during shutdown
    }

    cleanupAudioEndpoint();

    if (gdiplusToken_) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
    }
}

void WindowsMediaBackend::initAudioEndpoint() {
    HRESULT hr = CoInitialize(nullptr);
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator_);
    if (SUCCEEDED(hr) && deviceEnumerator_) {
        hr = deviceEnumerator_->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice_);
        if (SUCCEEDED(hr) && defaultDevice_) {
            hr = defaultDevice_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&endpointVolume_);
            if (SUCCEEDED(hr) && endpointVolume_) {
                volumeCallback_ = new VolumeNotificationCallback(this);
                endpointVolume_->RegisterControlChangeNotify(volumeCallback_);

                float vol = 0.5f;
                BOOL muted = FALSE;
                endpointVolume_->GetMasterVolumeLevelScalar(&vol);
                endpointVolume_->GetMute(&muted);

                std::lock_guard<std::mutex> lock(mutex_);
                currentInfo_.volume = static_cast<int>(vol * 100.0f + 0.5f);
                currentInfo_.isMuted = (muted != FALSE);
                utils::Logger::info("CoreAudio initialized. Volume: " + std::to_string(currentInfo_.volume) + "%");
            }
        }
    }
}

void WindowsMediaBackend::cleanupAudioEndpoint() {
    if (endpointVolume_) {
        if (volumeCallback_) {
            endpointVolume_->UnregisterControlChangeNotify(volumeCallback_);
            volumeCallback_->Release();
            volumeCallback_ = nullptr;
        }
        endpointVolume_->Release();
        endpointVolume_ = nullptr;
    }
    if (defaultDevice_) {
        defaultDevice_->Release();
        defaultDevice_ = nullptr;
    }
    if (deviceEnumerator_) {
        deviceEnumerator_->Release();
        deviceEnumerator_ = nullptr;
    }
}

void WindowsMediaBackend::onVolumeNotification(float newVolume, bool muted) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.volume = static_cast<int>(newVolume * 100.0f + 0.5f);
        currentInfo_.isMuted = muted;
    }
    scheduleDebouncedNotify(30);
}

void WindowsMediaBackend::refreshSession() {
    try {
        if (!sessionManager_) return;

        if (currentSession_) {
            if (mediaPropertiesToken_.value) currentSession_.MediaPropertiesChanged(mediaPropertiesToken_);
            if (playbackInfoToken_.value) currentSession_.PlaybackInfoChanged(playbackInfoToken_);
            if (timelineToken_.value) currentSession_.TimelinePropertiesChanged(timelineToken_);
            currentSession_ = nullptr;
        }

        currentSession_ = sessionManager_.GetCurrentSession();

        if (currentSession_) {
            utils::Logger::info("GSMTC: Active session attached: " + to_string(currentSession_.SourceAppUserModelId()));
            {
                std::lock_guard<std::mutex> lock(mutex_);
                currentInfo_.hasMedia = true;
                currentInfo_.source = to_string(currentSession_.SourceAppUserModelId());
            }

            mediaPropertiesToken_ = currentSession_.MediaPropertiesChanged(
                [this](auto&&, auto&&) {
                    refreshMediaProperties();
                    scheduleDebouncedNotify(60);
                }
            );

            playbackInfoToken_ = currentSession_.PlaybackInfoChanged(
                [this](auto&&, auto&&) {
                    refreshPlaybackInfo();
                    scheduleDebouncedNotify(60);
                }
            );

            timelineToken_ = currentSession_.TimelinePropertiesChanged(
                [this](auto&&, auto&&) {
                    refreshTimeline();
                    scheduleDebouncedNotify(60);
                }
            );

            refreshMediaProperties();
            refreshPlaybackInfo();
            refreshTimeline();
        } else {
            utils::Logger::info("GSMTC: No active session.");
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
            lastArtworkTitle_.clear();
            lastArtworkArtist_.clear();
        }
    } catch (const hresult_error& ex) {
        utils::Logger::error(std::string("refreshSession error: ") + to_string(ex.message()));
    } catch (...) {
        utils::Logger::error("refreshSession unknown error");
    }
}

void WindowsMediaBackend::refreshMediaProperties() {
    if (!currentSession_) return;

    try {
        auto props = currentSession_.TryGetMediaPropertiesAsync().get();
        if (!props) return;

        std::string title = to_string(props.Title());
        std::string artist = to_string(props.Artist());
        std::string album = to_string(props.AlbumTitle());

        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentInfo_.hasMedia = true;
            currentInfo_.title = title;
            currentInfo_.artist = artist;
            currentInfo_.album = album;

            if (title != lastArtworkTitle_ || artist != lastArtworkArtist_) {
                lastArtworkTitle_ = title;
                lastArtworkArtist_ = artist;
                // Track changed: clear previous artwork so old cover is NEVER retained
                currentInfo_.artwork.clear();
                currentInfo_.artworkHash.clear();
            }
        }

        // Try reading thumbnail
        bool artworkLoaded = tryExtractThumbnail(props);

        if (!artworkLoaded) {
            // Media players (Spotify, Chrome, Yandex Music, etc.) often provide thumbnail
            // asynchronously up to 1-3 seconds after title/artist change.
            // Try multiple intervals: 200ms, 400ms, 800ms, 1500ms, 3000ms.
            std::thread([this, title, artist]() {
                const int delaysMs[] = { 200, 400, 800, 1500, 3000 };
                for (int delay : delaysMs) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                    if (!running_ || !currentSession_) return;

                    // Check if track changed or already loaded
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (currentInfo_.title != title || currentInfo_.artist != artist) {
                            return; // Track changed, stop retrying for old track
                        }
                        if (!currentInfo_.artwork.empty()) {
                            return; // Already loaded!
                        }
                    }

                    try {
                        auto p = currentSession_.TryGetMediaPropertiesAsync().get();
                        if (p && to_string(p.Title()) == title && to_string(p.Artist()) == artist) {
                            if (tryExtractThumbnail(p)) {
                                utils::Logger::info("GSMTC: Thumbnail loaded on retry (" + std::to_string(delay) + "ms)");
                                scheduleDebouncedNotify(60);
                                return;
                            }
                        }
                    } catch (...) {}
                }
            }).detach();
        }

        utils::Logger::info("Track info: \"" + title + "\" by " + artist + " [" + album + "]");
    } catch (const hresult_error& ex) {
        utils::Logger::warn(std::string("refreshMediaProperties WinRT error: ") + to_string(ex.message()));
    } catch (...) {
        utils::Logger::warn("refreshMediaProperties unknown error");
    }
}

void WindowsMediaBackend::refreshPlaybackInfo() {
    if (!currentSession_) return;

    try {
        auto playback = currentSession_.GetPlaybackInfo();
        if (!playback) return;

        bool isPlaying = (playback.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
        auto controls = playback.Controls();

        std::lock_guard<std::mutex> lock(mutex_);
        if (currentInfo_.isPlaying && !isPlaying) {
            // Pausing: compute exact elapsed position and lock it
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
            // Resuming
            lastPositionTime_ = std::chrono::steady_clock::now();
        }

        currentInfo_.isPlaying = isPlaying;
        if (controls) {
            currentInfo_.capabilities.canPlayPause = controls.IsPlayPauseToggleEnabled() || controls.IsPlayEnabled() || controls.IsPauseEnabled();
            currentInfo_.capabilities.canNext = controls.IsNextEnabled();
            currentInfo_.capabilities.canPrev = controls.IsPreviousEnabled();
            currentInfo_.capabilities.canSeek = controls.IsPlaybackPositionEnabled();
        }
    } catch (const hresult_error& ex) {
        utils::Logger::warn(std::string("refreshPlaybackInfo WinRT error: ") + to_string(ex.message()));
    } catch (...) {
        utils::Logger::warn("refreshPlaybackInfo unknown error");
    }
}

void WindowsMediaBackend::refreshTimeline() {
    if (!currentSession_) return;

    try {
        auto timeline = currentSession_.GetTimelineProperties();
        if (!timeline) return;

        // TimeSpan is in 100-nanosecond intervals (1 ms = 10,000 intervals)
        int64_t posMs = timeline.Position().count() / 10000;
        int64_t durMs = timeline.EndTime().count() / 10000;

        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.positionMs = posMs;
        currentInfo_.durationMs = durMs;
        lastAuthoritativePosMs_ = posMs;
        lastPositionTime_ = std::chrono::steady_clock::now();
    } catch (const hresult_error& ex) {
        utils::Logger::warn(std::string("refreshTimeline WinRT error: ") + to_string(ex.message()));
    } catch (...) {
        utils::Logger::warn("refreshTimeline unknown error");
    }
}

bool WindowsMediaBackend::tryExtractThumbnail(const winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties& props) {
    if (!props) return false;
    try {
        auto thumbRef = props.Thumbnail();
        if (!thumbRef) return false;

        auto stream = thumbRef.OpenReadAsync().get();
        if (!stream) return false;

        uint32_t size = static_cast<uint32_t>(stream.Size());
        if (size == 0 || size > 10 * 1024 * 1024) return false;

        DataReader reader(stream);
        reader.LoadAsync(size).get();
        std::vector<uint8_t> buffer(size);
        reader.ReadBytes(buffer);

        std::string contentType = to_string(stream.ContentType());
        if (contentType.empty()) contentType = "image/jpeg";
        std::vector<uint8_t> optBuffer = optimizeArtworkImage(buffer, contentType);
        std::string hash = utils::computeHash(optBuffer);

        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.artwork = "data:" + contentType + ";base64," + utils::base64Encode(optBuffer);
        currentInfo_.artworkHash = hash;
        return true;
    } catch (...) {
        return false;
    }
}

MediaInfo WindowsMediaBackend::getNowPlaying() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_;
}

MediaInfo WindowsMediaBackend::getArtwork() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!currentInfo_.artwork.empty() || !currentInfo_.hasMedia || !currentSession_) {
            return currentInfo_;
        }
    }

    try {
        if (currentSession_) {
            auto props = currentSession_.TryGetMediaPropertiesAsync().get();
            if (props) {
                if (tryExtractThumbnail(props)) {
                    utils::Logger::info("GSMTC: Thumbnail loaded on-demand for client request");
                }
            }
        }
    } catch (...) {}

    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_;
}

void WindowsMediaBackend::play() {
    if (!currentSession_) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.isPlaying = true;
        lastPositionTime_ = std::chrono::steady_clock::now();
    }
    notifyChange();
    try {
        currentSession_.TryPlayAsync();
    } catch (...) {
        utils::Logger::error("TryPlayAsync failed");
    }
}

void WindowsMediaBackend::pause() {
    if (!currentSession_) return;
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
        currentSession_.TryPauseAsync();
    } catch (...) {
        utils::Logger::error("TryPauseAsync failed");
    }
}

void WindowsMediaBackend::togglePlayPause() {
    if (!currentSession_) return;
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
        currentSession_.TryTogglePlayPauseAsync();
    } catch (...) {
        utils::Logger::error("TryTogglePlayPauseAsync failed");
    }
}

void WindowsMediaBackend::next() {
    if (!currentSession_) return;
    try {
        currentSession_.TrySkipNextAsync();
    } catch (...) {
        utils::Logger::error("TrySkipNextAsync failed");
    }
}

void WindowsMediaBackend::previous() {
    if (!currentSession_) return;
    try {
        currentSession_.TrySkipPreviousAsync();
    } catch (...) {
        utils::Logger::error("TrySkipPreviousAsync failed");
    }
}

void WindowsMediaBackend::seek(int64_t positionMs) {
    if (!currentSession_) return;
    try {
        // Convert ms to 100ns units
        currentSession_.TryChangePlaybackPositionAsync(positionMs * 10000);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentInfo_.positionMs = positionMs;
            lastAuthoritativePosMs_ = positionMs;
            lastPositionTime_ = std::chrono::steady_clock::now();
        }
        notifyChange();
    } catch (...) {
        utils::Logger::error("TryChangePlaybackPositionAsync failed");
    }
}

void WindowsMediaBackend::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    HRESULT hr = E_FAIL;
    if (endpointVolume_) {
        float scalar = static_cast<float>(volume) / 100.0f;
        hr = endpointVolume_->SetMasterVolumeLevelScalar(scalar, nullptr);
    }

    if (FAILED(hr)) {
        // Default audio endpoint might have changed; re-acquire
        cleanupAudioEndpoint();
        initAudioEndpoint();
        if (endpointVolume_) {
            float scalar = static_cast<float>(volume) / 100.0f;
            endpointVolume_->SetMasterVolumeLevelScalar(scalar, nullptr);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.volume = volume;
    }
    notifyChange();
}

int WindowsMediaBackend::getVolume() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_.volume;
}

void WindowsMediaBackend::setMute(bool mute) {
    HRESULT hr = E_FAIL;
    if (endpointVolume_) {
        hr = endpointVolume_->SetMute(mute ? TRUE : FALSE, nullptr);
    }

    if (FAILED(hr)) {
        cleanupAudioEndpoint();
        initAudioEndpoint();
        if (endpointVolume_) {
            endpointVolume_->SetMute(mute ? TRUE : FALSE, nullptr);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentInfo_.isMuted = mute;
    }
    notifyChange();
}

bool WindowsMediaBackend::isMuted() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentInfo_.isMuted;
}

void WindowsMediaBackend::setOnChangeCallback(std::function<void(const MediaInfo&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    changeCallback_ = std::move(callback);
}

void WindowsMediaBackend::notifyChange() {
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

void WindowsMediaBackend::tickerLoop() {
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
                // Only notify if position in seconds changed
                if (calculated / 1000 != currentInfo_.positionMs / 1000) {
                    needsNotify = true;
                }
                currentInfo_.positionMs = calculated;
            }
        }
        if (needsNotify) {
            notifyChange();
        }
    }
}

void WindowsMediaBackend::debounceLoop() {
    std::unique_lock<std::mutex> lock(debounceMutex_);
    while (running_) {
        if (!debouncePending_) {
            debounceCv_.wait(lock, [this] { return !running_ || debouncePending_; });
        }
        if (!running_) break;

        auto now = std::chrono::steady_clock::now();
        if (now < debounceTargetTime_) {
            debounceCv_.wait_until(lock, debounceTargetTime_, [this] { return !running_; });
            if (!running_) break;
            if (std::chrono::steady_clock::now() < debounceTargetTime_) {
                continue;
            }
        }

        debouncePending_ = false;
        lock.unlock();

        notifyChange();

        lock.lock();
    }
}

void WindowsMediaBackend::scheduleDebouncedNotify(int delayMs) {
    std::lock_guard<std::mutex> lock(debounceMutex_);
    debounceTargetTime_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
    debouncePending_ = true;
    debounceCv_.notify_all();
}

} // namespace musicpad::backend

#endif // _WIN32
