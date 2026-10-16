#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace audio {

constexpr std::size_t kFftSize = 512;
constexpr std::size_t kBandCount = 24;
constexpr std::size_t kWavePoints = 160;

// Values are normalized for drawing except the dBFS readings.
struct AudioFrame {
    // Advances for every newly processed/received frame, including silence.
    // A renderer can distinguish fresh quiet audio from re-reading one frame.
    std::uint32_t generation = 0;
    std::array<float, kBandCount> bands{};
    std::array<float, kBandCount> peaks{};
    std::array<float, kWavePoints> leftWave{};
    std::array<float, kWavePoints> rightWave{};
    float leftDb = -60.0f;
    float rightDb = -60.0f;
    bool leftClip = false;
    bool rightClip = false;
    bool hasAudio = false;
    // 0..1 visual music gate. Synthetic UI frames default to active so they
    // retain their established appearance and regression coverage.
    float visualActivity = 1.0f;
    // Per-channel visual waveform gain; negative means derive from this frame
    // (used by synthetic UI inputs), zero intentionally blanks an idle mic.
    std::array<float, 2> scopeScale{{-1.0f, -1.0f}};
    // Optional rolling-history columns supplied by a PC visual-link packet.
    // The embedded UI can display the exact PC envelope instead of estimating
    // peaks from the heavily downsampled scope trace.
    static constexpr std::size_t kTransferredWaveColumns = 4;
    bool hasTransferredWaveform = false;
    std::uint16_t transferredWaveSequence = 0;
    std::array<std::uint8_t, kTransferredWaveColumns> transferredWavePeak{};
    std::array<std::uint8_t, kTransferredWaveColumns> transferredWaveRms{};
};

} // namespace audio
