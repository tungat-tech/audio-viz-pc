#pragma once

#include "audio/AudioProcessor.hpp"
#include <memory>
#include <string>
#include <vector>

namespace audio {

// Desktop capture adapter. Windows uses WASAPI loopback; Linux selects the
// default output monitor exposed by PulseAudio/PipeWire; macOS uses an explicitly
// selected CoreAudio loopback input (for example BlackHole).
class AudioCapture {
public:
    explicit AudioCapture(AudioProcessor& processor);
    ~AudioCapture();
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool start(std::string& error, const std::string& deviceName = {});
    static bool listDevices(std::vector<std::string>& devices, std::string& error);
    void stop();
    // Call on the UI thread to age the DSP while loopback sends no packets.
    void update();
    bool running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audio
