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
