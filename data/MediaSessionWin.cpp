#include "data/MediaSessionPlatform.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <roapi.h>
#include <wincodec.h>
#include <winstring.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>

// MinGW ships the ShCore import library but not every SDK version exposes this
// WinRT/COM bridge declaration in its headers.
extern "C" HRESULT WINAPI CreateStreamOverRandomAccessStream(
    IUnknown* randomAccessStream, REFIID riid, void** stream);

namespace data {
namespace {
struct InspectableVtable {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(void*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE* AddRef)(void*);
    ULONG (STDMETHODCALLTYPE* Release)(void*);
    HRESULT (STDMETHODCALLTYPE* GetIids)(void*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(void*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(void*, TrustLevel*);
};

struct Inspectable { InspectableVtable* lpVtbl; };
struct AsyncVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* PutCompleted)(void*, void*);
    HRESULT (STDMETHODCALLTYPE* GetCompleted)(void*, void**);
    HRESULT (STDMETHODCALLTYPE* GetResults)(void*, void**);
};
struct AsyncOperation { AsyncVtable* lpVtbl; };
struct ManagerStaticsVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* RequestAsync)(void*, AsyncOperation**);
};
struct ManagerStatics { ManagerStaticsVtable* lpVtbl; };
struct ManagerVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* GetCurrentSession)(void*, void**);
    HRESULT (STDMETHODCALLTYPE* GetSessions)(void*, void**);
    HRESULT (STDMETHODCALLTYPE* CurrentSessionChanged)(void*, void*, std::int64_t*);
    HRESULT (STDMETHODCALLTYPE* RemoveCurrentSessionChanged)(void*, std::int64_t);
    HRESULT (STDMETHODCALLTYPE* SessionsChanged)(void*, void*, std::int64_t*);
    HRESULT (STDMETHODCALLTYPE* RemoveSessionsChanged)(void*, std::int64_t);
};
struct Manager { ManagerVtable* lpVtbl; };
struct SessionVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* SourceAppUserModelId)(void*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE* TryGetMediaPropertiesAsync)(void*, AsyncOperation**);
    HRESULT (STDMETHODCALLTYPE* GetTimelineProperties)(void*, void**);
    HRESULT (STDMETHODCALLTYPE* GetPlaybackInfo)(void*, void**);
};
struct Session { SessionVtable* lpVtbl; };
struct MediaPropertiesVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* Title)(void*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE* Subtitle)(void*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE* AlbumArtist)(void*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE* Artist)(void*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE* AlbumTitle)(void*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE* TrackNumber)(void*, std::int32_t*);
    HRESULT (STDMETHODCALLTYPE* Genres)(void*, void**);
    HRESULT (STDMETHODCALLTYPE* AlbumTrackCount)(void*, std::int32_t*);
    HRESULT (STDMETHODCALLTYPE* PlaybackType)(void*, void**);
    HRESULT (STDMETHODCALLTYPE* Thumbnail)(void*, void**);
};
struct MediaProperties { MediaPropertiesVtable* lpVtbl; };
struct RandomAccessStreamReferenceVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* OpenReadAsync)(void*, AsyncOperation**);
};
struct RandomAccessStreamReference { RandomAccessStreamReferenceVtable* lpVtbl; };
struct TimelineVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* StartTime)(void*, std::int64_t*);
    HRESULT (STDMETHODCALLTYPE* EndTime)(void*, std::int64_t*);
    HRESULT (STDMETHODCALLTYPE* MinSeekTime)(void*, std::int64_t*);
    HRESULT (STDMETHODCALLTYPE* MaxSeekTime)(void*, std::int64_t*);
    HRESULT (STDMETHODCALLTYPE* Position)(void*, std::int64_t*);
    HRESULT (STDMETHODCALLTYPE* LastUpdatedTime)(void*, std::int64_t*);
};
struct Timeline { TimelineVtable* lpVtbl; };
struct PlaybackInfoVtable {
    InspectableVtable base;
    HRESULT (STDMETHODCALLTYPE* Controls)(void*, void**);
    HRESULT (STDMETHODCALLTYPE* PlaybackStatus)(void*, std::int32_t*);
};
struct PlaybackInfo { PlaybackInfoVtable* lpVtbl; };

constexpr GUID kManagerStaticsIid = {
    0x2050c4ee, 0x11a0, 0x57de, {0xae, 0xd7, 0xc9, 0x7c, 0x70, 0x33, 0x82, 0x45}};
constexpr wchar_t kManagerClass[] =
    L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
constexpr HRESULT kAsyncNotReady = static_cast<HRESULT>(0x8000000eL);
constexpr double kTicksPerSecond = 10000000.0;

void release(void* object) {
    if (object) reinterpret_cast<Inspectable*>(object)->lpVtbl->Release(object);
}

bool await(AsyncOperation* operation, void** result, std::string& error) {
    if (!operation) {
        error = "Media async operation was null";
        return false;
    }
    *result = nullptr;
    HRESULT hr = kAsyncNotReady;
    for (int attempt = 0; attempt < 300; ++attempt) {
        hr = operation->lpVtbl->GetResults(operation, result);
        if (SUCCEEDED(hr) && *result) return true;
        if (hr != kAsyncNotReady) break;
        Sleep(10);
    }
    error = "Media async operation failed (HRESULT " + std::to_string(
        static_cast<unsigned long>(hr)) + ")";
    return false;
}

std::string takeHString(HSTRING value) {
    if (!value) return {};
    UINT32 length = 0;
    const wchar_t* text = WindowsGetStringRawBuffer(value, &length);
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length),
                                          nullptr, 0, nullptr, nullptr);
    // windows.h defines max as a function-like macro unless NOMINMAX is set.
    // Parenthesizing the qualified name keeps this translation unit usable by
    // both MSVC and the MinGW cross-build without relying on include order.
    std::string result(static_cast<std::size_t>((std::max)(0, bytes)), '\0');
    if (bytes > 0)
        WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), result.data(),
                            bytes, nullptr, nullptr);
    WindowsDeleteString(value);
    return result;
}

bool getText(MediaProperties* properties,
             HRESULT (STDMETHODCALLTYPE* getter)(void*, HSTRING*),
             std::string& result) {
    HSTRING value = nullptr;
    if (FAILED(getter(properties, &value))) return false;
    result = takeHString(value);
    return true;
}

template <typename T>
void releaseCom(T*& object) {
    if (!object) return;
    object->Release();
    object = nullptr;
}

bool readAlbumArt(MediaProperties* properties,
                  std::shared_ptr<const AlbumArt>& output) {
    RandomAccessStreamReference* reference = nullptr;
    AsyncOperation* openOperation = nullptr;
    Inspectable* randomAccessStream = nullptr;
    IStream* stream = nullptr;
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapClipper* clipper = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool success = false;

    if (FAILED(properties->lpVtbl->Thumbnail(
            properties, reinterpret_cast<void**>(&reference))) || !reference)
        goto cleanup;
    if (FAILED(reference->lpVtbl->OpenReadAsync(reference, &openOperation)) ||
        !openOperation) goto cleanup;
    {
        std::string ignored;
        if (!await(openOperation, reinterpret_cast<void**>(&randomAccessStream), ignored))
            goto cleanup;
    }
    if (FAILED(CreateStreamOverRandomAccessStream(
            reinterpret_cast<IUnknown*>(randomAccessStream), IID_IStream,
            reinterpret_cast<void**>(&stream))) || !stream) goto cleanup;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        goto cleanup;
    if (FAILED(factory->CreateDecoderFromStream(stream, nullptr,
            WICDecodeMetadataCacheOnLoad, &decoder))) goto cleanup;
    if (FAILED(decoder->GetFrame(0, &frame))) goto cleanup;

    {
        UINT sourceWidth = 0;
        UINT sourceHeight = 0;
        if (FAILED(frame->GetSize(&sourceWidth, &sourceHeight)) ||
            sourceWidth == 0 || sourceHeight == 0) goto cleanup;

        const double targetRatio = static_cast<double>(kAlbumArtWidth) /
                                   static_cast<double>(kAlbumArtHeight);
        WICRect crop{0, 0, static_cast<INT>(sourceWidth),
                    static_cast<INT>(sourceHeight)};
        if (static_cast<double>(sourceWidth) / sourceHeight > targetRatio) {
            crop.Width = std::max<INT>(1,
                static_cast<INT>(sourceHeight * targetRatio + 0.5));
            crop.X = (static_cast<INT>(sourceWidth) - crop.Width) / 2;
        } else {
            crop.Height = std::max<INT>(1,
                static_cast<INT>(sourceWidth / targetRatio + 0.5));
            crop.Y = (static_cast<INT>(sourceHeight) - crop.Height) / 2;
        }

        if (FAILED(factory->CreateBitmapClipper(&clipper)) ||
            FAILED(clipper->Initialize(frame, &crop))) goto cleanup;
        if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
            FAILED(scaler->Initialize(clipper,
                static_cast<UINT>(kAlbumArtWidth),
                static_cast<UINT>(kAlbumArtHeight),
                WICBitmapInterpolationModeFant))) goto cleanup;
        if (FAILED(factory->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0,
                WICBitmapPaletteTypeCustom))) goto cleanup;

        std::array<std::uint8_t, kAlbumArtWidth * kAlbumArtHeight * 4> bgra{};
        if (FAILED(converter->CopyPixels(nullptr,
                static_cast<UINT>(kAlbumArtWidth * 4),
                static_cast<UINT>(bgra.size()), bgra.data()))) goto cleanup;

        auto art = std::make_shared<AlbumArt>();
        for (std::size_t pixel = 0;
             pixel < kAlbumArtWidth * kAlbumArtHeight; ++pixel) {
            const std::size_t source = pixel * 4;
            const unsigned alpha = bgra[source + 3];
            // Composite transparent thumbnails over the app's deep-navy panel.
            const auto red = static_cast<std::uint8_t>(
                (bgra[source + 2] * alpha + 4 * (255 - alpha)) / 255);
            const auto green = static_cast<std::uint8_t>(
                (bgra[source + 1] * alpha + 13 * (255 - alpha)) / 255);
            const auto blue = static_cast<std::uint8_t>(
                (bgra[source] * alpha + 26 * (255 - alpha)) / 255);
            art->rgb565[pixel] = static_cast<std::uint16_t>(
                ((red & 0xF8u) << 8) | ((green & 0xFCu) << 3) | (blue >> 3));
        }
        output = std::move(art);
        success = true;
    }

cleanup:
    releaseCom(converter);
    releaseCom(scaler);
    releaseCom(clipper);
    releaseCom(frame);
    releaseCom(decoder);
    releaseCom(factory);
    releaseCom(stream);
    release(randomAccessStream);
    release(openOperation);
    release(reference);
    return success;
}
}

bool platformMediaSessionSupported() { return true; }

bool readPlatformMediaSession(MediaDataSnapshot& snapshot, std::string& error) {
    snapshot = {};
    error.clear();
    const HRESULT initialized = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialized)) {
        error = "RoInitialize failed";
        return false;
    }

    HSTRING className = nullptr;
    ManagerStatics* statics = nullptr;
    AsyncOperation* managerOperation = nullptr;
    Manager* manager = nullptr;
    Session* session = nullptr;
    AsyncOperation* propertiesOperation = nullptr;
    MediaProperties* properties = nullptr;
    Timeline* timeline = nullptr;
    PlaybackInfo* playback = nullptr;
    bool success = false;

    if (FAILED(WindowsCreateString(kManagerClass,
            static_cast<UINT32>(sizeof(kManagerClass) / sizeof(wchar_t) - 1), &className))) {
        error = "WindowsCreateString failed";
        goto cleanup;
    }
    if (FAILED(RoGetActivationFactory(className, kManagerStaticsIid,
                                      reinterpret_cast<void**>(&statics))) || !statics) {
        error = "Windows media-session API is unavailable";
        goto cleanup;
    }
    if (FAILED(statics->lpVtbl->RequestAsync(statics, &managerOperation)) ||
        !await(managerOperation, reinterpret_cast<void**>(&manager), error)) goto cleanup;
    if (FAILED(manager->lpVtbl->GetCurrentSession(manager,
                                                  reinterpret_cast<void**>(&session)))) {
        error = "Could not query the current media session";
        goto cleanup;
    }
    if (!session) {
        success = true;
        goto cleanup;
    }
    snapshot.active = true;
    if (FAILED(session->lpVtbl->TryGetMediaPropertiesAsync(session, &propertiesOperation)) ||
        !await(propertiesOperation, reinterpret_cast<void**>(&properties), error)) goto cleanup;
    if (!getText(properties, properties->lpVtbl->Title, snapshot.title) ||
        !getText(properties, properties->lpVtbl->Artist, snapshot.artist)) {
        error = "Could not read media title/artist";
        goto cleanup;
    }
    if (snapshot.title.empty()) snapshot.title = "UNTITLED MEDIA";
    if (snapshot.artist.empty()) snapshot.artist = "UNKNOWN ARTIST";

    // Album art usually changes only when the track changes. Cache the decoded
    // 80x84 RGB image and retry occasionally when a player publishes metadata
    // before its thumbnail becomes ready.
    {
        static std::string cachedKey;
        static std::shared_ptr<const AlbumArt> cachedArt;
        static unsigned missingArtPolls = 0;
        const std::string key = snapshot.title + '\n' + snapshot.artist;
        const bool changed = key != cachedKey;
        if (changed) {
            cachedKey = key;
            cachedArt.reset();
            missingArtPolls = 0;
        }
        if (changed || (!cachedArt && ++missingArtPolls >= 10)) {
            std::shared_ptr<const AlbumArt> next;
            if (readAlbumArt(properties, next)) cachedArt = std::move(next);
            missingArtPolls = 0;
        }
        snapshot.albumArt = cachedArt;
    }

    if (SUCCEEDED(session->lpVtbl->GetTimelineProperties(
            session, reinterpret_cast<void**>(&timeline))) && timeline) {
        std::int64_t start = 0, end = 0, position = 0;
        if (SUCCEEDED(timeline->lpVtbl->StartTime(timeline, &start)) &&
            SUCCEEDED(timeline->lpVtbl->EndTime(timeline, &end)) &&
            SUCCEEDED(timeline->lpVtbl->Position(timeline, &position))) {
            snapshot.durationSeconds = std::max<std::int64_t>(0, end - start) / kTicksPerSecond;
            snapshot.positionSeconds = std::clamp<std::int64_t>(position - start, 0,
                std::max<std::int64_t>(0, end - start)) / kTicksPerSecond;
        }
    }
    if (SUCCEEDED(session->lpVtbl->GetPlaybackInfo(
            session, reinterpret_cast<void**>(&playback))) && playback) {
        std::int32_t status = 0;
        if (SUCCEEDED(playback->lpVtbl->PlaybackStatus(playback, &status)))
            snapshot.playing = status == 4;
    }
    success = true;

cleanup:
    release(playback);
    release(timeline);
    release(properties);
    release(propertiesOperation);
    release(session);
    release(manager);
    release(managerOperation);
    release(statics);
    if (className) WindowsDeleteString(className);
    RoUninitialize();
    return success;
}

} // namespace data
