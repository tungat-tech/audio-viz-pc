#pragma once

#include "audio/AudioProcessor.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace app {

class DemoSignal {
public:
    static std::array<float, 2> sampleAt(std::uint64_t index) {
        constexpr double pi = 3.14159265358979323846;
        // An integer sample clock avoids float time accumulation, which used
        // to change pitch and eventually stop advancing after about 8 minutes.
        const double time = static_cast<double>(index) / 48000.0;
        const double low = 0.44 * std::sin(2.0 * pi * 110.0 * time);
        const double sweep = 0.24 * std::sin(2.0 * pi *
            (320.0 * time + 160.0 * std::sin(0.6 * time)));
        const double mid = 0.10 * std::sin(2.0 * pi * 1900.0 * time);
        const double high = 0.06 * std::sin(2.0 * pi * 5400.0 * time);
        const double beat = 0.55 + 0.45 * std::sin(2.0 * pi * 1.6 * time);
        return {static_cast<float>(beat * (low + sweep + mid + high)),
                static_cast<float>(beat * (0.78 * low - sweep + 0.7 * mid - high))};
    }

    void generate(audio::AudioProcessor& processor, std::uint32_t milliseconds) {
        std::array<float, 960 * 2> samples{};
        // Chunk instead of dropping elapsed audio when a display frame takes
        // longer than the fixed temporary buffer's 20 ms capacity.
        for (std::uint64_t remaining = 48ULL * milliseconds; remaining > 0;) {
            const auto frames = static_cast<std::uint32_t>(std::min<std::uint64_t>(960, remaining));
            for (std::uint32_t i = 0; i < frames; ++i) {
                const auto value = sampleAt(++sampleIndex_);
                samples[2 * i] = value[0];
                samples[2 * i + 1] = value[1];
            }
            processor.processInterleaved(samples.data(), frames);
            remaining -= frames;
        }
    }

    std::uint64_t framesGenerated() const { return sampleIndex_; }

private:
    std::uint64_t sampleIndex_ = 0;
};

} // namespace app
