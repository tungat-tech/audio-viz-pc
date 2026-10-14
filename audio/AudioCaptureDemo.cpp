// Portable GUI validation adapter; never claims to capture system audio.
#include "audio/AudioCapture.hpp"

namespace audio {
struct AudioCapture::Impl {};
AudioCapture::AudioCapture(AudioProcessor&) : impl_(std::make_unique<Impl>()) {}
AudioCapture::~AudioCapture() = default;
bool AudioCapture::start(std::string& error, const std::string&) {
    error = "This demo-only build has no live capture. Run with --demo.";
    return false;
}
bool AudioCapture::listDevices(std::vector<std::string>& devices, std::string& error) {
    devices.clear();
    error = "This demo-only build has no live capture devices.";
    return false;
}
void AudioCapture::stop() {}
void AudioCapture::update() {}
bool AudioCapture::running() const { return false; }
} // namespace audio
