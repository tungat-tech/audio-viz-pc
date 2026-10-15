#include "ui/VisualizerUi.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

LV_FONT_DECLARE(lv_font_montserrat_vietnamese_12);
LV_FONT_DECLARE(lv_font_montserrat_vietnamese_14);

namespace ui {
namespace {
constexpr float kPi = 3.14159265358979323846f;

lv_color_t color(int r, int g, int b) {
    return lv_color_make(static_cast<std::uint8_t>(r),
                         static_cast<std::uint8_t>(g),
                         static_cast<std::uint8_t>(b));
}

const lv_color_t kPanel = color(7, 29, 44);
const lv_color_t kDeep = color(3, 11, 20);
const lv_color_t kGrid = color(39, 76, 91);
const lv_color_t kCyan = color(106, 225, 230);
const lv_color_t kViolet = color(91, 69, 214);
const lv_color_t kIce = color(193, 247, 248);
const lv_color_t kMuted = color(142, 169, 182);

void updateLabel(lv_obj_t* item, const std::string& value) {
    if (std::strcmp(lv_label_get_text(item), value.c_str()) != 0)
        lv_label_set_text(item, value.c_str());
}
}

void VisualizerUi::drawUtilityPanel() {
    rect(10, 50, 304, 150, color(2, 8, 16));
    rect(8, 48, 304, 150, kPanel);
    frameRect(8, 48, 304, 150, kGrid);
}

void VisualizerUi::drawMiniWave(const audio::AudioFrame& frame, int left, int right,
                                int center, int amplitude, lv_color_t primary,
                                lv_color_t secondary) {
    line(left, center, right, center, color(31, 70, 91));
    const int width = std::max(1, right - left);
    int previousX = left;
    int previousLeft = center;
    int previousRight = center;
    for (std::size_t i = 0; i < audio::kWavePoints; ++i) {
        const int x = left + static_cast<int>(i) * width /
                               static_cast<int>(audio::kWavePoints - 1);
        const float gain = frame.scopeScale[0] >= 0.0f
            ? frame.scopeScale[0] / 34.0f
            : (frame.hasAudio ? 7.0f : 0.0f);
        const int leftY = center - static_cast<int>(std::clamp(
            frame.leftWave[i] * gain * amplitude, -static_cast<float>(amplitude),
            static_cast<float>(amplitude)));
        const int rightY = center - static_cast<int>(std::clamp(
            frame.rightWave[i] * gain * amplitude, -static_cast<float>(amplitude),
            static_cast<float>(amplitude)));
        if (i) {
            line(previousX, previousRight, x, rightY, secondary);
            line(previousX, previousLeft, x, leftY, primary);
        }
        previousX = x;
        previousLeft = leftY;
        previousRight = rightY;
    }
}

void VisualizerUi::drawMarket(const data::UtilityFrame& utility, bool full) {
    (void)full;
    drawUtilityPanel();
    const auto& btc = utility.market[0];
    const auto& eth = utility.market[1];
    const auto& sol = utility.market[2];
    updateLabel(pageLabel_, utility.marketLive ? "24H LIVE" :
                utility.marketStatus == "DEMO" ? "24H DEMO" :
                utility.marketStatus == "LOADING" ? "LOADING" : "24H STALE");
    updateLabel(leftValue_, btc.price);
    char text[32];
    std::snprintf(text, sizeof(text), "%+.2f%%", btc.changePercent);
    updateLabel(annotations_[1], text);
    updateLabel(annotations_[4], eth.price);
    std::snprintf(text, sizeof(text), "%+.2f%%", eth.changePercent);
    updateLabel(annotations_[5], text);
    updateLabel(annotations_[7], sol.price);
    std::snprintf(text, sizeof(text), "%+.2f%%", sol.changePercent);
    updateLabel(annotations_[8], text);

    // Main 24-hour sparkline. Numeric labels remain visible even when color is
    // not, so direction is never communicated by hue alone.
    constexpr int chartLeft = 155;
    constexpr int chartTop = 62;
    constexpr int chartWidth = 146;
    constexpr int chartHeight = 72;
    rect(chartLeft, chartTop, chartWidth, chartHeight, kDeep);
    frameRect(chartLeft, chartTop, chartWidth, chartHeight, kGrid);
    for (int x = chartLeft + 29; x < chartLeft + chartWidth; x += 29)
        line(x, chartTop + 1, x, chartTop + chartHeight - 2, color(24, 57, 76));
    for (int y = chartTop + 18; y < chartTop + chartHeight; y += 18)
        line(chartLeft + 1, y, chartLeft + chartWidth - 2, y, color(24, 57, 76));
    int previousX = chartLeft + 2;
    int previousY = chartTop + chartHeight - 3 -
                    static_cast<int>(btc.history[0] * (chartHeight - 7));
    for (std::size_t i = 1; i < btc.history.size(); ++i) {
        const int x = chartLeft + 2 + static_cast<int>(i) * (chartWidth - 5) /
                                      static_cast<int>(btc.history.size() - 1);
        const int y = chartTop + chartHeight - 3 -
                      static_cast<int>(btc.history[i] * (chartHeight - 7));
        line(previousX, previousY + 1, x, y + 1, color(37, 91, 120));
        line(previousX, previousY, x, y, kCyan);
        previousX = x;
        previousY = y;
    }
    disc(previousX, previousY, 2, kIce);

    line(16, 144, 303, 144, kGrid);
    line(159, 150, 159, 191, kGrid);

    const auto sparkline = [this](const data::MarketQuote& quote, int left, int top,
                                  int width, int height) {
        int previousX = left;
        int previousY = top + height -
                        static_cast<int>(quote.history.front() * height);
        for (std::size_t i = 1; i < quote.history.size(); i += 3) {
            const int x = left + static_cast<int>(i) * width /
                                  static_cast<int>(quote.history.size() - 1);
            const int y = top + height - static_cast<int>(quote.history[i] * height);
            line(previousX, previousY, x, y,
                 i > quote.history.size() * 2 / 3 ? kViolet : kCyan);
            previousX = x;
            previousY = y;
        }
        disc(previousX, previousY, 1, kIce);
    };
    sparkline(eth, 92, 151, 55, 17);
    sparkline(sol, 242, 151, 56, 17);
    invalidate({8, 48, 311, 197});
}

void VisualizerUi::drawClock(const audio::AudioFrame& frame,
                             const data::UtilityFrame& utility, bool full) {
    (void)full;
    drawUtilityPanel();
    updateLabel(leftValue_, utility.time);
    updateLabel(annotations_[0], utility.date);
    updateLabel(annotations_[1], utility.seconds);

    const int seconds = std::clamp(std::atoi(utility.seconds.c_str()), 0, 59);
    constexpr int cx = 256;
    constexpr int cy = 91;
    for (int tick = 0; tick < 60; ++tick) {
        const float angle = (static_cast<float>(tick) / 60.0f * 2.0f * kPi) - kPi * 0.5f;
        const int inner = tick % 5 == 0 ? 29 : 31;
        const int outer = 35;
        const auto ink = tick <= seconds
            ? (tick > 48 ? color(126, 112, 232) : kCyan)
            : color(27, 65, 87);
        line(cx + static_cast<int>(std::cos(angle) * inner),
             cy + static_cast<int>(std::sin(angle) * inner),
             cx + static_cast<int>(std::cos(angle) * outer),
             cy + static_cast<int>(std::sin(angle) * outer), ink);
    }
    constexpr int baseline = 163;
    line(17, baseline, 303, baseline, color(31, 70, 91));
    const float activity = frame.hasAudio
        ? std::clamp(frame.visualActivity, 0.0f, 1.0f) : 0.0f;
    for (std::size_t band = 0; band < audio::kBandCount; ++band) {
        const int x = 18 + static_cast<int>(band) * 12;
        const float t = static_cast<float>(band) /
                        static_cast<float>(audio::kBandCount - 1);
        const float packet = std::exp(-std::pow((t - 0.18f) / 0.13f, 2.0f)) +
                             0.72f * std::exp(-std::pow((t - 0.52f) / 0.11f, 2.0f)) +
                             0.90f * std::exp(-std::pow((t - 0.83f) / 0.10f, 2.0f));
        const float shaped = std::clamp(frame.bands[band], 0.0f, 1.0f);
        const float display = std::clamp(shaped * 0.68f + packet * activity * 0.32f,
                                         0.0f, 1.0f);
        const int height = 2 + static_cast<int>(display * 31.0f);
        const auto ink = band < 8 || band > 18 ? kCyan : color(118, 102, 235);
        rect(x, baseline - height, 6, height, ink);
        rect(x, baseline + 2, 6, std::max(1, height / 3),
             band < 8 || band > 18 ? color(31, 83, 112) : color(45, 48, 119));
    }
    invalidate({8, 48, 311, 197});
}

void VisualizerUi::drawNowPlaying(const audio::AudioFrame& frame,
                                  const data::UtilityFrame& utility, bool full) {
    (void)full;
    drawUtilityPanel();
    updateLabel(leftValue_, utility.trackTitle);
    updateLabel(annotations_[0], utility.trackArtist);
    updateLabel(annotations_[1], utility.trackPosition);

    constexpr int artLeft = 17;
    constexpr int artTop = 58;
    if (utility.albumArt) {
        const auto& rgb565 = utility.albumArt->rgb565;
        for (std::size_t y = 0; y < data::kAlbumArtHeight; ++y) {
            for (std::size_t x = 0; x < data::kAlbumArtWidth; ++x) {
                const auto packed = rgb565[y * data::kAlbumArtWidth + x];
                pixel(artLeft + static_cast<int>(x), artTop + static_cast<int>(y),
                      color(((packed >> 11) & 0x1F) * 255 / 31,
                            ((packed >> 5) & 0x3F) * 255 / 63,
                            (packed & 0x1F) * 255 / 31));
            }
        }
    } else {
        // A-signal app logo: deterministic fallback when the active player
        // does not expose a thumbnail through the Windows media session.
        rect(artLeft, artTop, data::kAlbumArtWidth, data::kAlbumArtHeight,
             color(8, 26, 42));
        frameRect(artLeft + 5, artTop + 5, data::kAlbumArtWidth - 10,
                  data::kAlbumArtHeight - 10, color(40, 71, 87));
        line(artLeft + 13, artTop + 42, artLeft + 67, artTop + 42,
             color(53, 91, 107));
        constexpr std::array<int, 5> heights{22, 42, 58, 36, 18};
        for (std::size_t bar = 0; bar < heights.size(); ++bar) {
            const int height = heights[bar];
            const int x = artLeft + 16 + static_cast<int>(bar) * 11;
            rect(x, artTop + 42 - height / 2, 7, height,
                 bar == 3 ? color(119, 104, 237) : kCyan);
        }
    }
    frameRect(artLeft, artTop, data::kAlbumArtWidth, data::kAlbumArtHeight,
              color(87, 70, 174));

    rect(110, 129, 190, 5, color(22, 57, 78));
    rect(110, 129, std::max(2, static_cast<int>(190.0f * utility.trackProgress)),
         5, kCyan);
    constexpr int baseline = 181;
    const float activity = frame.hasAudio
        ? std::clamp(frame.visualActivity, 0.0f, 1.0f) : 0.0f;
    for (std::size_t band = 0; band < audio::kBandCount; ++band) {
        const int x = 18 + static_cast<int>(band) * 12;
        const float t = static_cast<float>(band) /
                        static_cast<float>(audio::kBandCount - 1);
        const float packet = 0.65f * std::exp(-std::pow((t - 0.20f) / 0.16f, 2.0f)) +
                             std::exp(-std::pow((t - 0.50f) / 0.13f, 2.0f)) +
                             0.72f * std::exp(-std::pow((t - 0.78f) / 0.14f, 2.0f));
        const float display = std::clamp(frame.bands[band] * 0.72f +
                                         packet * activity * 0.28f, 0.0f, 1.0f);
        const int height = 2 + static_cast<int>(display * 31.0f);
        const auto ink = band < 9 ? kCyan : color(111, 99, 235);
        rect(x, baseline - height, 7, height, ink);
        rect(x, baseline + 2, 7, std::max(1, height / 3), color(25, 55, 91));
    }
    invalidate({8, 48, 311, 197});
}

void VisualizerUi::drawDeskBoard(const audio::AudioFrame& frame,
                                 const data::UtilityFrame& utility, bool full) {
    (void)full;
    drawUtilityPanel();
    updateLabel(leftValue_, utility.time);
    updateLabel(annotations_[0], utility.date);
    updateLabel(annotations_[1], utility.temperature);
    updateLabel(annotations_[8], utility.weatherLocation);
    std::string compactPrice;
    for (char character : utility.market[0].price)
        if (character != '$' && character != ',') compactPrice += character;
    const double btcValue = std::strtod(compactPrice.c_str(), nullptr);
    char text[32];
    std::snprintf(text, sizeof(text), "$%.1fK", btcValue / 1000.0);
    updateLabel(annotations_[3], text);
    std::snprintf(text, sizeof(text), "%+.1f%%", utility.market[0].changePercent);
    updateLabel(annotations_[4], text);
    std::snprintf(text, sizeof(text), "%d MIN", utility.focusMinutes);
    updateLabel(annotations_[6], text);

    line(160, 56, 160, 151, kGrid);
    line(172, 87, 301, 87, kGrid);
    line(172, 120, 301, 120, kGrid);

    // Compact vector cloud following the reference card's outlined icon.
    line(174, 77, 207, 77, kCyan);
    line(174, 77, 171, 73, kCyan);
    line(171, 73, 172, 68, kCyan);
    line(172, 68, 177, 65, kCyan);
    line(177, 65, 183, 66, kCyan);
    line(183, 66, 187, 60, kCyan);
    line(187, 60, 194, 58, kCyan);
    line(194, 58, 201, 61, kCyan);
    line(201, 61, 204, 67, kCyan);
    line(204, 67, 209, 69, kCyan);
    line(209, 69, 210, 73, kCyan);
    line(210, 73, 207, 77, kCyan);

    // Focus ring communicates remaining time with geometry as well as text.
    constexpr int cx = 194;
    constexpr int cy = 138;
    for (int segment = 0; segment < 24; ++segment) {
        const float angle = segment * 2.0f * kPi / 24.0f - kPi * 0.5f;
        const auto ink = segment < utility.focusMinutes * 24 / 42
            ? kCyan : color(30, 67, 91);
        pixel(cx + static_cast<int>(std::cos(angle) * 13),
              cy + static_cast<int>(std::sin(angle) * 13), ink);
        pixel(cx + static_cast<int>(std::cos(angle) * 14),
              cy + static_cast<int>(std::sin(angle) * 14), ink);
    }
    const float bass = (frame.bands[0] + frame.bands[1] + frame.bands[2]) / 3.0f;
    const float mid = (frame.bands[8] + frame.bands[10] + frame.bands[12]) / 3.0f;
    const float phase = static_cast<float>(frame.generation % 10000) * 0.035f;
    const float cyanAmplitude = 6.0f + bass * 8.0f;
    const float violetAmplitude = 4.0f + mid * 6.0f;
    int previousX = 17;
    int previousCyan = 168;
    int previousViolet = 168;
    for (int x = 18; x <= 303; ++x) {
        const float t = static_cast<float>(x - 17) / 286.0f;
        const int cyanY = 168 + static_cast<int>(std::sin(t * kPi * 7.0f + phase) *
                                                     cyanAmplitude * std::sin(t * kPi));
        const int violetY = 168 + static_cast<int>(std::sin(t * kPi * 4.2f - phase * 0.55f) *
                                                       violetAmplitude * std::sin(t * kPi));
        line(previousX, previousViolet, x, violetY, kViolet);
        line(previousX, previousCyan, x, cyanY, kCyan);
        previousX = x;
        previousCyan = cyanY;
        previousViolet = violetY;
    }
    line(17, 190, 94, 190, color(45, 105, 129));
    line(225, 190, 303, 190, color(45, 105, 129));
    invalidate({8, 48, 311, 197});
}

} // namespace ui
