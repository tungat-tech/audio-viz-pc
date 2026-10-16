#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ui {

// Increment only when a consumer-visible ID or contract meaning changes.
// Layout/color-only work does not require a version bump.
constexpr std::uint16_t kUiContractVersion = 1;

// These values are persistent IDs, not display positions. Never renumber or
// reuse an existing value; append a new ID and choose its display position in
// kVisibleModeOrder instead.
enum class ModeId : std::uint8_t {
    NowPlaying = 1,
    Spectrum = 2,
    Clock = 3,
    Market = 4,
    DeskBoard = 5,
    Vu = 16,
    LegacyScope = 17,
};

// Spectrum style values are also persistent IDs and are stored in device NVS.
// New styles may be appended before Count, but existing values must stay fixed.
enum class SpectrumStyleId : std::uint8_t {
    Bars = 0,
    Trace = 1,
    Dots = 2,
    NeonPulse = 3,
    Mirror = 4,
    Waveform = 5,
    Oscilloscope = 6,
    Halo = 7,
    Waterfall = 8,
    Ridgeline = 9,
    TopoBloom = 10,
    AuroraWeave = 11,
    CymaticMesh = 12,
    Count = 13,
};

constexpr std::array<ModeId, 5> kVisibleModeOrder{
    ModeId::NowPlaying,
    ModeId::Spectrum,
    ModeId::Clock,
    ModeId::Market,
    ModeId::DeskBoard,
};

constexpr std::uint32_t modeBit(ModeId mode) {
    switch (mode) {
    case ModeId::NowPlaying: return 1u << 0;
    case ModeId::Spectrum: return 1u << 1;
    case ModeId::Clock: return 1u << 2;
    case ModeId::Market: return 1u << 3;
    case ModeId::DeskBoard: return 1u << 4;
    case ModeId::Vu: return 1u << 5;
    case ModeId::LegacyScope: return 1u << 6;
    }
    return 0;
}

constexpr std::uint32_t styleBit(SpectrumStyleId style) {
    const auto id = static_cast<unsigned>(style);
    return id < static_cast<unsigned>(SpectrumStyleId::Count) ? 1u << id : 0;
}

constexpr std::uint32_t kAllVisibleModesMask =
    modeBit(ModeId::NowPlaying) | modeBit(ModeId::Spectrum) |
    modeBit(ModeId::Clock) | modeBit(ModeId::Market) |
    modeBit(ModeId::DeskBoard);
constexpr std::uint32_t kAllSpectrumStylesMask =
    (1u << static_cast<unsigned>(SpectrumStyleId::Count)) - 1u;

struct UiCapabilities {
    std::uint32_t modeMask = kAllVisibleModesMask;
    std::uint32_t spectrumStyleMask = kAllSpectrumStylesMask;

    constexpr bool supports(ModeId mode) const {
        return (modeMask & modeBit(mode)) != 0;
    }
    constexpr bool supports(SpectrumStyleId style) const {
        return (spectrumStyleMask & styleBit(style)) != 0;
    }
};

constexpr std::size_t visibleModeIndex(ModeId id) {
    for (std::size_t index = 0; index < kVisibleModeOrder.size(); ++index)
        if (kVisibleModeOrder[index] == id) return index;
    return kVisibleModeOrder.size();
}

constexpr ModeId firstSupportedMode(const UiCapabilities& capabilities) {
    for (const auto mode : kVisibleModeOrder)
        if (capabilities.supports(mode)) return mode;
    return ModeId::Spectrum;
}

constexpr ModeId nextSupportedMode(ModeId current,
                                   const UiCapabilities& capabilities) {
    const auto currentIndex = visibleModeIndex(current);
    const auto start = currentIndex < kVisibleModeOrder.size() ? currentIndex : 0;
    for (std::size_t offset = 1; offset <= kVisibleModeOrder.size(); ++offset) {
        const auto candidate = kVisibleModeOrder[(start + offset) % kVisibleModeOrder.size()];
        if (capabilities.supports(candidate)) return candidate;
    }
    return firstSupportedMode(capabilities);
}

constexpr SpectrumStyleId firstSupportedSpectrumStyle(
    const UiCapabilities& capabilities) {
    for (unsigned id = 0; id < static_cast<unsigned>(SpectrumStyleId::Count); ++id) {
        const auto style = static_cast<SpectrumStyleId>(id);
        if (capabilities.supports(style)) return style;
    }
    return SpectrumStyleId::Bars;
}

constexpr SpectrumStyleId nextSupportedSpectrumStyle(
    SpectrumStyleId current, const UiCapabilities& capabilities) {
    constexpr auto count = static_cast<unsigned>(SpectrumStyleId::Count);
    const auto start = static_cast<unsigned>(current) < count
        ? static_cast<unsigned>(current) : 0;
    for (unsigned offset = 1; offset <= count; ++offset) {
        const auto style = static_cast<SpectrumStyleId>((start + offset) % count);
        if (capabilities.supports(style)) return style;
    }
    return firstSupportedSpectrumStyle(capabilities);
}

} // namespace ui
