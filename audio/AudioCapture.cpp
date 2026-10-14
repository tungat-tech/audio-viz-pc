#include "audio/AudioCapture.hpp"
#include "audio/AudioInputFeed.hpp"
#include <miniaudio.h>
#include <algorithm>
#include <cstring>

namespace audio {

namespace {
ma_backend desktopBackend() {
#ifdef _WIN32
    return ma_backend_wasapi;
#elif defined(__APPLE__)
    return ma_backend_coreaudio;
#else
    return ma_backend_pulseaudio;
#endif
}

const char* backendName() {
#ifdef _WIN32
    return "WASAPI";
#elif defined(__APPLE__)
    return "CoreAudio";
#else
    return "PulseAudio/PipeWire";
#endif
}

bool monitorSource(const ma_device_info& info) {
#if !defined(_WIN32) && !defined(__APPLE__)
    const std::string id(info.id.pulse);
    return id.size() >= 8 && id.compare(id.size() - 8, 8, ".monitor") == 0;
#else
    (void)info;
    return false;
#endif
}
} // namespace

struct AudioCapture::Impl {
    explicit Impl(AudioProcessor& target) : feed(target) {}
    AudioInputFeed feed;
    ma_context context{};
    ma_device device{};
    ma_device_id selectedId{};
    bool contextReady = false;
    bool deviceReady = false;

    static void callback(ma_device* device, void*, const void* input, ma_uint32 frames) {
        auto* self = static_cast<Impl*>(device->pUserData);
        if (self && input) self->feed.process(static_cast<const float*>(input), frames);
    }
};

AudioCapture::AudioCapture(AudioProcessor& processor)
    : impl_(std::make_unique<Impl>(processor)) {}

AudioCapture::~AudioCapture() { stop(); }

bool AudioCapture::listDevices(std::vector<std::string>& devices, std::string& error) {
    devices.clear();
    error.clear();
#ifdef _WIN32
    return true;
#else
    ma_context context{};
    const auto backend = desktopBackend();
    ma_context_config config = ma_context_config_init();
    ma_result result = ma_context_init(&backend, 1, &config, &context);
    if (result != MA_SUCCESS) {
        error = std::string(backendName()) + " unavailable: " + ma_result_description(result);
        return false;
    }
    ma_device_info* playback = nullptr;
    ma_device_info* capture = nullptr;
    ma_uint32 playbackCount = 0;
    ma_uint32 captureCount = 0;
    result = ma_context_get_devices(&context, &playback, &playbackCount, &capture, &captureCount);
    if (result == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < captureCount; ++i) {
#ifndef __APPLE__
            if (!monitorSource(capture[i])) continue;
#endif
            devices.emplace_back(capture[i].name);
        }
    } else {
        error = std::string("Cannot list ") + backendName() + " inputs: " + ma_result_description(result);
    }
    ma_context_uninit(&context);
    return result == MA_SUCCESS;
#endif
}

bool AudioCapture::start(std::string& error, const std::string& deviceName) {
    stop();
    error.clear();
    const auto backend = desktopBackend();
    ma_context_config contextConfig = ma_context_config_init();
    ma_result result = ma_context_init(&backend, 1, &contextConfig, &impl_->context);
    if (result != MA_SUCCESS) {
        error = std::string(backendName()) + " initialization failed: " + ma_result_description(result);
        return false;
    }
    impl_->contextReady = true;

#ifdef _WIN32
    if (!deviceName.empty()) {
        error = "Windows WASAPI loopback always uses the default output; omit --audio-device";
        stop();
        return false;
    }
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
#else
    ma_device_info* playback = nullptr;
    ma_device_info* capture = nullptr;
    ma_uint32 playbackCount = 0;
    ma_uint32 captureCount = 0;
    result = ma_context_get_devices(&impl_->context, &playback, &playbackCount,
                                    &capture, &captureCount);
    if (result != MA_SUCCESS) {
        error = std::string("Cannot enumerate ") + backendName() + " devices: " +
                ma_result_description(result);
        stop();
        return false;
    }
    const ma_device_info* selected = nullptr;
#ifdef __APPLE__
    if (deviceName.empty()) {
        error = "macOS needs an explicit loopback input (for example BlackHole). "
                "Route system output to it and choose --audio-device NAME.";
        stop();
        return false;
    }
#else
    std::string defaultMonitor;
    if (deviceName.empty()) {
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            if (playback[i].isDefault) {
                defaultMonitor = std::string(playback[i].id.pulse) + ".monitor";
                break;
            }
        }
    }
#endif
    for (ma_uint32 i = 0; i < captureCount; ++i) {
#ifndef __APPLE__
        if (!monitorSource(capture[i])) continue;
        const bool matches = deviceName.empty()
            ? std::string(capture[i].id.pulse) == defaultMonitor
            : deviceName == capture[i].name || deviceName == capture[i].id.pulse;
#else
        const bool matches = deviceName == capture[i].name ||
                             deviceName == capture[i].id.coreaudio;
#endif
        if (matches) { selected = &capture[i]; break; }
    }
    if (!selected) {
        error = deviceName.empty()
            ? "Default output monitor not found. Enable the PulseAudio/PipeWire Pulse server."
            : "Selected loopback input not found: " + deviceName;
        stop();
        return false;
    }
    impl_->selectedId = selected->id;
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = &impl_->selectedId;
#endif
    config.capture.format = ma_format_f32;
    config.capture.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = &Impl::callback;
    config.pUserData = impl_.get();
    result = ma_device_init(&impl_->context, &config, &impl_->device);
    if (result != MA_SUCCESS) {
        error = std::string("Audio input failed: ") + ma_result_description(result);
        stop();
        return false;
    }
    impl_->deviceReady = true;
    impl_->feed.start();
    result = ma_device_start(&impl_->device);
    if (result != MA_SUCCESS) {
        error = std::string("Audio capture could not start: ") + ma_result_description(result);
        stop();
        return false;
    }
    return true;
}

void AudioCapture::stop() {
    impl_->feed.stop();
    if (impl_->deviceReady) {
        ma_device_uninit(&impl_->device);
        impl_->deviceReady = false;
    }
    if (impl_->contextReady) {
        ma_context_uninit(&impl_->context);
        impl_->contextReady = false;
    }
}

void AudioCapture::update() {
    if (impl_->deviceReady) impl_->feed.pumpSilence();
}

bool AudioCapture::running() const {
    return impl_->deviceReady && ma_device_is_started(&impl_->device) == MA_TRUE;
}

} // namespace audio
