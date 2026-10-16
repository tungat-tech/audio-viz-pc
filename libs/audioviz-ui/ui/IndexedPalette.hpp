#pragma once

#include <lvgl.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace ui {
namespace indexed_palette {

struct Rgb {
    std::uint8_t red, green, blue;
};

constexpr Rgb mix(Rgb a, Rgb b, int step, int steps) {
    return {static_cast<std::uint8_t>((a.red * (steps - step) + b.red * step) / steps),
            static_cast<std::uint8_t>((a.green * (steps - step) + b.green * step) / steps),
            static_cast<std::uint8_t>((a.blue * (steps - step) + b.blue * step) / steps)};
}

constexpr std::array<Rgb, 256> makeColors() {
    std::array<Rgb, 256> colors{};
    // Exact semantic swatches from the approved VU and the Windows blue/cyan/violet UI.
    constexpr std::array<Rgb, 24> anchors{{
        {9, 20, 31}, {16, 30, 43}, {6, 27, 40}, {16, 42, 76},
        {52, 78, 96}, {61, 89, 107}, {30, 54, 70}, {98, 181, 204},
        {126, 199, 218}, {105, 184, 205}, {106, 225, 230}, {147, 215, 230},
        {194, 235, 241}, {68, 55, 182}, {91, 69, 214}, {62, 80, 219},
        {233, 214, 174}, {187, 163, 123}, {72, 53, 35}, {177, 58, 43},
        {194, 151, 87}, {241, 238, 224}, {174, 185, 184}, {0, 0, 0}
    }};
    for (std::size_t i = 0; i < anchors.size(); ++i) colors[i] = anchors[i];
    auto ramp = [&colors](int first, int last, Rgb from, Rgb to) {
        for (int i = first; i <= last; ++i)
            colors[static_cast<std::size_t>(i)] = mix(from, to, i - first, last - first);
    };
    ramp(24, 63, {0, 7, 13}, {38, 76, 104});
    ramp(64, 103, {20, 70, 95}, {87, 185, 204});
    ramp(104, 143, {77, 169, 199}, {215, 245, 249});
    ramp(144, 175, {47, 41, 123}, {174, 154, 246});
    ramp(176, 207, {171, 136, 94}, {250, 236, 206});
    ramp(208, 239, {64, 46, 32}, {230, 85, 57});
    ramp(240, 255, {7, 11, 16}, {252, 252, 252});
    return colors;
}

inline const std::array<Rgb, 256>& colors() {
    static constexpr auto value = makeColors();
    return value;
}

inline lv_color_t color(std::uint8_t index) {
    const auto rgb = colors()[index];
    return lv_color_make(rgb.red, rgb.green, rgb.blue);
}

inline const std::array<std::uint8_t, 4096>& lookup() {
    // 4 KiB RAM, one-time startup work, then O(1) indexing while drawing at 60 Hz.
    static std::array<std::uint8_t, 4096> table{};
    static bool ready = false;
    if (!ready) {
        const auto& palette = colors();
        for (int key = 0; key < 4096; ++key) {
            const int red = ((key >> 8) & 15) * 17;
            const int green = ((key >> 4) & 15) * 17;
            const int blue = (key & 15) * 17;
            int best = std::numeric_limits<int>::max();
            std::uint8_t selected = 0;
            for (int i = 0; i < 256; ++i) {
                const auto candidate = palette[static_cast<std::size_t>(i)];
                const int dr = red - candidate.red;
                const int dg = green - candidate.green;
                const int db = blue - candidate.blue;
                const int distance = 2 * dr * dr + 3 * dg * dg + 2 * db * db;
                if (distance < best) { best = distance; selected = static_cast<std::uint8_t>(i); }
            }
            table[static_cast<std::size_t>(key)] = selected;
        }
        ready = true;
    }
    return table;
}

inline std::uint8_t index(lv_color_t source) {
    lv_color32_t expanded;
    expanded.full = lv_color_to32(source);
    const auto r = static_cast<unsigned>((expanded.ch.red + 8) >> 4);
    const auto g = static_cast<unsigned>((expanded.ch.green + 8) >> 4);
    const auto b = static_cast<unsigned>((expanded.ch.blue + 8) >> 4);
    const unsigned key = (std::min(r, 15u) << 8) |
                         (std::min(g, 15u) << 4) | std::min(b, 15u);
    return lookup()[key];
}

} // namespace indexed_palette
} // namespace ui
