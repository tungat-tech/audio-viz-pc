#pragma once

#include "audio/AudioFrame.hpp"
#include "audio/Fft512.hpp"
#include <array>
#include <cstdint>
#include <mutex>

namespace audio {

// Desktop-only visual analysis. This implementation has no device calibration
// or board-specific noise model; its output is consumed only by the PC UI.
class AudioProcessor {
public:
    explicit AudioProcessor(float sampleRate = 48000.0f);
    void processInterleaved(const float* stereo, std::uint32_t frameCount);
    AudioFrame snapshot() const;

private:
    void processBlock();

    Fft512 fft_;
    float sampleRate_ = 48000.0f;
    std::array<float, kFftSize> left_{};
    std::array<float, kFftSize> right_{};
    std::array<float, kBandCount> bars_{};
    std::array<float, kBandCount> peaks_{};
    std::array<float, 2> meterRms_{};
    std::array<int, 2> clipHold_{};
    std::size_t fill_ = 0;
    float displayReference_ = 0.05f;
    float waveformReference_ = 0.08f;
    std::uint32_t generation_ = 0;
    mutable std::mutex mutex_;
    AudioFrame latest_{};
};

} // namespace audio
