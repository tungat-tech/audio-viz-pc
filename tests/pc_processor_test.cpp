#include "app/DemoSignal.hpp"
#include "app/Options.hpp"
#include "audio/AudioProcessor.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}

int main() {
    app::Options options;
    std::string error;
    char name[] = "AudioViz_PC";
    char serial[] = "--serial";
    char port[] = "COM6";
    char* invalid[] = {name, serial, port};
    require(!app::parseOptions(3, invalid, options, error),
            "Public app unexpectedly accepts a device-link option");

    audio::AudioProcessor processor;
    app::DemoSignal demo;
    for (int i = 0; i < 100; ++i) demo.generate(processor, 20);
    const auto live = processor.snapshot();
    require(live.hasAudio && live.generation > 0, "Demo produced no audio frame");
    require(*std::max_element(live.bands.begin(), live.bands.end()) > 0.1f,
            "Spectrum remained blank");
    require(std::isfinite(live.leftDb) && std::isfinite(live.rightDb),
            "VU meter is non-finite");

    // A loud steady signal should have headroom, and the next quieter block
    // should remain visibly quieter instead of being normalized to full height.
    audio::AudioProcessor waveformProcessor;
    std::array<float, audio::kFftSize * 2> tone{};
    const auto feedTone = [&](float amplitude) {
        for (std::size_t i = 0; i < audio::kFftSize; ++i) {
            tone[2 * i] = amplitude * std::sin(static_cast<float>(i) * 0.17f);
            tone[2 * i + 1] = 0.0f;
        }
        waveformProcessor.processInterleaved(tone.data(), audio::kFftSize);
        return waveformProcessor.snapshot();
    };
    for (int i = 0; i < 30; ++i) feedTone(0.7f);
    const auto loud = waveformProcessor.snapshot();
    const float loudPixels = 0.7f * loud.scopeScale[0];
    require(loudPixels > 20.0f && loudPixels < 34.0f,
            "Steady waveform should have visible headroom");
    require(loud.scopeScale[0] == loud.scopeScale[1],
            "Waveform should use one stereo gain");
    const auto quiet = feedTone(0.14f);
    require(0.14f * quiet.scopeScale[0] < loudPixels * 0.4f,
            "Quiet passage was normalized to a full-height waveform");

    std::array<float, audio::kFftSize * 2> silence{};
    for (int i = 0; i < 180; ++i)
        processor.processInterleaved(silence.data(), audio::kFftSize);
    const auto idle = processor.snapshot();
    require(!idle.hasAudio && idle.leftDb <= -59.0f && idle.rightDb <= -59.0f,
            "Silent input retained level");
    require(*std::max_element(idle.bands.begin(), idle.bands.end()) < 0.02f,
            "Silent input retained spectrum");
    std::cout << "PC-only parser, demo spectrum and silence decay passed\n";
}
