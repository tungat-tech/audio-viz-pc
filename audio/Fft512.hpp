#pragma once

#include "audio/AudioFrame.hpp"
#include <array>
#include <complex>

namespace audio {

// In-place radix-2 FFT. No heap allocation and no platform-specific DSP calls.
class Fft512 {
public:
    Fft512();
    void magnitudes(const std::array<float, kFftSize>& samples,
                    std::array<float, kFftSize / 2>& output) const;

private:
    std::array<float, kFftSize> window_{};
    std::array<std::complex<float>, kFftSize / 2> roots_{};
};

} // namespace audio
