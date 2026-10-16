#pragma once

#include "audio/AudioFrame.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace audio {

constexpr std::size_t kWaveformHistoryColumns = 287;

struct WaveformHistory {
    std::array<std::uint8_t, kWaveformHistoryColumns> peak{};
    std::array<std::uint8_t, kWaveformHistoryColumns> rms{};
    std::uint64_t sequence = 0;
};

// Written on the audio thread, copied under AudioProcessor's snapshot mutex.
// Time is measured in audio samples, not SDL paint calls: a native window drag
// may pause painting, but it cannot erase the beats captured during that pause.
class WaveformHistoryBuffer {
public:
    void append(const AudioFrame& frame, float sampleRate) {
        float peak = 0.0f, power = 0.0f;
        for (std::size_t i = 0; i < kWavePoints; ++i) {
            const float sample = std::max(std::abs(frame.leftWave[i]),
                                          std::abs(frame.rightWave[i]));
            peak = std::max(peak, sample);
            power += sample * sample;
        }
        const float rms = std::sqrt(power / static_cast<float>(kWavePoints));
        const float scale = frame.scopeScale[0] >= 0.0f && frame.scopeScale[1] >= 0.0f ?
            std::min(frame.scopeScale[0], frame.scopeScale[1]) :
            frame.scopeScale[0] >= 0.0f ? frame.scopeScale[0] :
            frame.scopeScale[1] >= 0.0f ? frame.scopeScale[1] : 48.0f;
        const float activity = !frame.hasAudio ? 0.0f :
            frame.scopeScale[0] >= 0.0f || frame.scopeScale[1] >= 0.0f ?
            1.0f : std::clamp(frame.visualActivity, 0.0f, 1.0f);
        const int peakPixels = std::clamp(static_cast<int>(std::lround(peak * scale * activity)), 0, 48);
        const int rmsPixels = std::clamp(static_cast<int>(std::lround(rms * scale * activity)), 0, peakPixels);
        pendingPeak_ = std::max(pendingPeak_, static_cast<std::uint8_t>(peakPixels));
        pendingRms_ = std::max(pendingRms_, static_cast<std::uint8_t>(rmsPixels));
        accumulatedSamples_ += kFftSize;
        const double samplesPerColumn = std::max(8000.0, static_cast<double>(sampleRate)) / 60.0;
        while (accumulatedSamples_ >= samplesPerColumn) {
            accumulatedSamples_ -= samplesPerColumn;
            peak_[head_] = pendingPeak_;
            rms_[head_] = pendingRms_;
            head_ = (head_ + 1) % kWaveformHistoryColumns;
            pendingPeak_ = pendingRms_ = 0;
            ++sequence_;
        }
    }

    WaveformHistory snapshot() const {
        WaveformHistory result;
        for (std::size_t i = 0; i < kWaveformHistoryColumns; ++i) {
            const auto source = (head_ + i) % kWaveformHistoryColumns;
            result.peak[i] = peak_[source];
            result.rms[i] = rms_[source];
        }
        result.sequence = sequence_;
        return result;
    }

private:
    std::array<std::uint8_t, kWaveformHistoryColumns> peak_{};
    std::array<std::uint8_t, kWaveformHistoryColumns> rms_{};
    std::size_t head_ = 0;
    std::uint64_t sequence_ = 0;
    double accumulatedSamples_ = 0.0;
    std::uint8_t pendingPeak_ = 0;
    std::uint8_t pendingRms_ = 0;
};

} // namespace audio
