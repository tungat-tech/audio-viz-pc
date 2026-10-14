#include "audio/AudioProcessor.hpp"
#include <algorithm>
#include <cmath>

namespace audio {
namespace {
float safeSample(float value) { return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f; }
float dbfs(float rms) { return std::clamp(20.0f * std::log10(std::max(rms, 0.000001f)), -60.0f, 3.0f); }
}

AudioProcessor::AudioProcessor(float sampleRate)
    : sampleRate_(std::isfinite(sampleRate) && sampleRate >= 8000.0f ? sampleRate : 48000.0f) {}

void AudioProcessor::processInterleaved(const float* stereo, std::uint32_t frameCount) {
    if (!stereo) return;
    for (std::uint32_t i = 0; i < frameCount; ++i) {
        left_[fill_] = safeSample(stereo[2 * i]);
        right_[fill_] = safeSample(stereo[2 * i + 1]);
        if (++fill_ == kFftSize) {
            processBlock();
            fill_ = 0;
        }
    }
}

void AudioProcessor::processBlock() {
    std::array<float, kFftSize / 2> leftSpectrum{}, rightSpectrum{};
    fft_.magnitudes(left_, leftSpectrum);
    fft_.magnitudes(right_, rightSpectrum);

    float power[2] = {0.0f, 0.0f};
    float samplePeak[2] = {0.0f, 0.0f};
    for (std::size_t i = 0; i < kFftSize; ++i) {
        power[0] += left_[i] * left_[i];
        power[1] += right_[i] * right_[i];
        samplePeak[0] = std::max(samplePeak[0], std::abs(left_[i]));
        samplePeak[1] = std::max(samplePeak[1], std::abs(right_[i]));
    }
    const float rms[2] = {std::sqrt(power[0] / kFftSize), std::sqrt(power[1] / kFftSize)};
    const bool active = std::max(rms[0], rms[1]) > 0.0005f;

    std::array<float, kBandCount> energy{};
    float strongest = 0.0f;
    const float nyquist = sampleRate_ * 0.5f;
    for (std::size_t band = 0; band < kBandCount; ++band) {
        const float low = 45.0f * std::pow(nyquist / 45.0f,
            static_cast<float>(band) / static_cast<float>(kBandCount));
        const float high = 45.0f * std::pow(nyquist / 45.0f,
            static_cast<float>(band + 1) / static_cast<float>(kBandCount));
        const std::size_t first = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::floor(low * kFftSize / sampleRate_)), 1, 255);
        const std::size_t last = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::ceil(high * kFftSize / sampleRate_)), first, 255);
        for (std::size_t bin = first; bin <= last; ++bin) {
            const float stereoPower = 0.5f * (leftSpectrum[bin] * leftSpectrum[bin] +
                                              rightSpectrum[bin] * rightSpectrum[bin]);
            energy[band] = std::max(energy[band], std::sqrt(stereoPower));
        }
        strongest = std::max(strongest, energy[band]);
    }
    // A single broad visual reference suits PC loopback. There is deliberately
    // no microphone-floor learning or board-tuned activity gate here.
    const float wanted = std::max(strongest, 0.008f);
    displayReference_ += (wanted - displayReference_) *
        (wanted > displayReference_ ? 0.18f : 0.025f);

    AudioFrame frame;
    frame.generation = ++generation_;
    frame.hasAudio = active;
    frame.visualActivity = active ? 1.0f : 0.0f;
    for (std::size_t band = 0; band < kBandCount; ++band) {
        const float target = active ? std::clamp(energy[band] /
            std::max(displayReference_, 0.008f), 0.0f, 1.0f) : 0.0f;
        bars_[band] += (target - bars_[band]) * (target > bars_[band] ? 0.55f : 0.20f);
        peaks_[band] = std::max(bars_[band], peaks_[band] - 0.02f);
        frame.bands[band] = bars_[band];
        frame.peaks[band] = peaks_[band];
    }
    for (std::size_t i = 0; i < kWavePoints; ++i) {
        const std::size_t at = i * kFftSize / kWavePoints;
        frame.leftWave[i] = left_[at];
        frame.rightWave[i] = right_[at];
    }
    // One stereo reference avoids boosting the louder channel with the quiet
    // channel's gain. Attack catches level jumps; slower release preserves the
    // relative shape of softer passages instead of filling every column.
    const float stereoPeak = std::max(samplePeak[0], samplePeak[1]);
    waveformReference_ += (std::max(stereoPeak, 0.008f) - waveformReference_) *
        (stereoPeak > waveformReference_ ? 0.35f : 0.008f);
    const float waveformScale = active ? std::min(600.0f,
        27.0f / std::max(waveformReference_, 0.008f)) : 0.0f;
    for (std::size_t channel = 0; channel < 2; ++channel) {
        meterRms_[channel] += (rms[channel] - meterRms_[channel]) *
            (rms[channel] > meterRms_[channel] ? 0.35f : 0.14f);
        clipHold_[channel] = samplePeak[channel] >= 0.98f ? 12 : std::max(0, clipHold_[channel] - 1);
        frame.scopeScale[channel] = waveformScale;
    }
    frame.leftDb = dbfs(meterRms_[0]);
    frame.rightDb = dbfs(meterRms_[1]);
    frame.leftClip = clipHold_[0] > 0;
    frame.rightClip = clipHold_[1] > 0;
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = frame;
}

AudioFrame AudioProcessor::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

} // namespace audio
