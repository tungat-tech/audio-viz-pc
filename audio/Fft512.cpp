#include "audio/Fft512.hpp"
#include <algorithm>
#include <cmath>

namespace audio {
namespace {
constexpr float kPi = 3.14159265358979323846f;
}

Fft512::Fft512() {
    for (std::size_t i = 0; i < kFftSize; ++i) {
        window_[i] = 0.5f - 0.5f * std::cos(2.0f * kPi * static_cast<float>(i) /
                                               static_cast<float>(kFftSize - 1));
    }
    for (std::size_t i = 0; i < roots_.size(); ++i) {
        const float angle = -2.0f * kPi * static_cast<float>(i) /
                            static_cast<float>(kFftSize);
        roots_[i] = {std::cos(angle), std::sin(angle)};
    }
}

void Fft512::magnitudes(const std::array<float, kFftSize>& samples,
                        std::array<float, kFftSize / 2>& output) const {
    std::array<std::complex<float>, kFftSize> data{};
    for (std::size_t i = 0; i < kFftSize; ++i)
        data[i] = {samples[i] * window_[i], 0.0f};

    for (std::size_t i = 1, j = 0; i < kFftSize; ++i) {
        std::size_t bit = kFftSize >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    for (std::size_t len = 2; len <= kFftSize; len <<= 1) {
        const std::size_t rootStep = kFftSize / len;
        for (std::size_t base = 0; base < kFftSize; base += len) {
            for (std::size_t j = 0; j < len / 2; ++j) {
                const auto a = data[base + j];
                const auto b = data[base + j + len / 2] * roots_[j * rootStep];
                data[base + j] = a + b;
                data[base + j + len / 2] = a - b;
            }
        }
    }

    // Hann coherent gain is approximately 0.5, so 4/N maps a full-scale
    // bin-centred sine close to unity.
    for (std::size_t i = 0; i < output.size(); ++i)
        output[i] = std::abs(data[i]) * (4.0f / static_cast<float>(kFftSize));
}

} // namespace audio
