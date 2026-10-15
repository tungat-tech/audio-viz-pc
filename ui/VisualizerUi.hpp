#pragma once

#include "audio/AudioFrame.hpp"
#include "audio/WaveformHistory.hpp"
#include "data/UtilityFrame.hpp"
#include <lvgl.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace ui {

class VisualizerUi {
public:
    enum class SpectrumStyle : std::uint8_t {
        Bars = 0, Trace, Dots, NeonPulse, Mirror, Waveform, Halo,
        Waterfall, Ridgeline, TopoBloom, AuroraWeave, CymaticMesh, Count
    };
    enum class TouchAction : std::uint8_t { NextMode, NextSpectrumStyle };
    using TouchActionHandler = void (*)(void*, TouchAction);
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 240;
    static constexpr std::size_t kCanvasPixels = kWidth * kHeight;

    // The PC canvas is heap allocated, never placed on the task stack.
    VisualizerUi();
    // Destroy this instance before its containing LVGL screen or lv_deinit().
    ~VisualizerUi();
    VisualizerUi(const VisualizerUi&) = delete;
    VisualizerUi& operator=(const VisualizerUi&) = delete;
    VisualizerUi(VisualizerUi&&) = delete;
    VisualizerUi& operator=(VisualizerUi&&) = delete;
    // False for allocation failure, a null buffer, or fewer than kCanvasPixels.
    // An unready instance has no LVGL objects; all mutating methods are safe no-ops.
    bool ready() const { return canvas_ != nullptr; }
    void nextMode();
    void setMode(unsigned oneBasedMode);
    // Optional application-level handler; built-in behavior remains available.
    void setTouchActionHandler(TouchActionHandler handler, void* context);
    void setSpectrumStyle(SpectrumStyle style);
    SpectrumStyle spectrumStyle() const { return spectrumStyle_; }
    void setStatus(const std::string& status);
    void render(const audio::AudioFrame& frame,
                const audio::WaveformHistory* waveformHistory = nullptr,
                const data::UtilityFrame* utilityFrame = nullptr);

private:
    enum class Mode {
        Spectrum = 0, Vu, Scope, Market, Clock, NowPlaying, DeskBoard, Count
    };
    struct MeterState {
        int tipX = 0;
        int tipY = 0;
        bool clip = false;
    };

    static void buttonEvent(lv_event_t* event);
    static void styleButtonEvent(lv_event_t* event);
    void dispatchTouchAction(TouchAction action);
    void initialize();
    void updateLabels();
    void invalidate(const lv_area_t& area);
    void pixel(int x, int y, lv_color_t color);
    void line(int x0, int y0, int x1, int y1, lv_color_t color);
    void rect(int x, int y, int w, int h, lv_color_t color);
    void frameRect(int x, int y, int w, int h, lv_color_t color);
    void disc(int cx, int cy, int radius, lv_color_t color);
    void clear(lv_color_t color);
    void drawShell();
    void drawVu(const audio::AudioFrame& frame, bool full);
    void drawMeter(int x, float db, bool clip);
    void drawSpectrumBase();
    void drawSpectrumBar(std::size_t band, int height, int peak);
    void drawSpectrumDots(std::size_t band, int height, int peak);
    void drawSpectrumTrace();
    void drawSpectrum(const audio::AudioFrame& frame, bool full);
    void drawConceptSpectrum(const audio::AudioFrame& frame, bool full);
    void drawPulseRibbon();
    void drawWaveform(const audio::AudioFrame& frame, bool full);
    void drawHalo(const audio::AudioFrame& frame, bool full);
    void drawWaterfall(const audio::AudioFrame& frame, bool full);
    void drawRidgeline(const audio::AudioFrame& frame, bool full);
    void drawTopoBloom(const audio::AudioFrame& frame, bool full);
    void drawAuroraWeave(const audio::AudioFrame& frame, bool full);
    void drawCymaticMesh(const audio::AudioFrame& frame, bool full);
    void drawWaveformColumn(int x);
    void resetWaveform();
    void resetSpectrumHistory();
    void drawScopeBase(int channel);
    void drawScopeTrace(int channel);
    void drawScope(const audio::AudioFrame& frame, bool full);
    void drawMarket(const data::UtilityFrame& utility, bool full);
    void drawClock(const audio::AudioFrame& frame, const data::UtilityFrame& utility,
                   bool full);
    void drawNowPlaying(const audio::AudioFrame& frame,
                        const data::UtilityFrame& utility, bool full);
    void drawDeskBoard(const audio::AudioFrame& frame,
                       const data::UtilityFrame& utility, bool full);
    void drawUtilityPanel();
    void drawMiniWave(const audio::AudioFrame& frame, int left, int right,
                      int center, int amplitude, lv_color_t primary,
                      lv_color_t secondary);

    std::unique_ptr<lv_color_t[]> ownedBuffer_;
    lv_color_t* buffer_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* modeLabel_ = nullptr;
    lv_obj_t* pageLabel_ = nullptr;
    lv_obj_t* status_ = nullptr;
    lv_obj_t* button_ = nullptr;
    lv_obj_t* styleButton_ = nullptr;
    TouchActionHandler touchActionHandler_ = nullptr;
    void* touchActionContext_ = nullptr;
    std::array<lv_obj_t*, 12> annotations_{};
    lv_obj_t* leftValue_ = nullptr;
    lv_obj_t* rightValue_ = nullptr;
    std::string statusText_ = "STARTING AUDIO";
    Mode mode_ = Mode::Spectrum;
    SpectrumStyle spectrumStyle_ = SpectrumStyle::Bars;
    bool shellDrawn_ = false;
    bool modeChanged_ = true;
    lv_area_t clipArea_{0, 0, kWidth - 1, kHeight - 1};
    std::array<MeterState, 2> meters_{};
    std::array<int, audio::kBandCount> barHeights_{};
    std::array<int, audio::kBandCount> peakHeights_{};
    std::array<std::array<std::int16_t, audio::kWavePoints>, 2> waveY_{};
    static constexpr int kWaveformLeft = 17;
    static constexpr int kWaveformRight = 303;
    static constexpr std::size_t kWaveformColumns = kWaveformRight - kWaveformLeft + 1;
    std::array<std::uint8_t, kWaveformColumns> waveformPeak_{};
    std::array<std::uint8_t, kWaveformColumns> waveformRms_{};
    std::array<float, audio::kWavePoints> waveformLastLeft_{};
    std::array<float, audio::kWavePoints> waveformLastRight_{};
    std::array<float, 2> waveformLastScale_{};
    float waveformLastActivity_ = 0.0f;
    std::uint32_t waveformLastGeneration_ = 0;
    bool waveformLastHasAudio_ = false;
    bool waveformSourceValid_ = false;
    const audio::WaveformHistory* waveformExternalHistory_ = nullptr;
    std::uint64_t waveformLastHistorySequence_ = 0;
    float haloPhase_ = 0.0f;
    float haloPulse_ = 0.0f;
    float haloLastBass_ = 0.0f;
    std::uint32_t haloLastGeneration_ = 0;
    static constexpr std::size_t kWaterfallRows = 12;
    static constexpr std::size_t kRidgelineRows = 5;
    std::array<std::array<std::uint8_t, audio::kBandCount>, kWaterfallRows> waterfall_{};
    std::array<std::array<std::uint8_t, audio::kBandCount>, kRidgelineRows> ridgeline_{};
    std::uint32_t waterfallLastGeneration_ = 0;
    std::uint32_t ridgelineLastGeneration_ = 0;
    std::uint32_t topoLastGeneration_ = 0;
    std::uint32_t auroraLastGeneration_ = 0;
    std::uint32_t cymaticLastGeneration_ = 0;
    float topoPhase_ = 0.0f;
    float auroraPhase_ = 0.0f;
    float cymaticPhase_ = 0.0f;
    std::uint64_t utilityLastGeneration_ = 0;
    std::uint64_t marketLastGeneration_ = 0;
};

} // namespace ui
