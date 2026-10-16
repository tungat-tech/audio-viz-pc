#include "ui/VisualizerUi.hpp"
#include "ui/IndexedPalette.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
LV_FONT_DECLARE(lv_font_montserrat_vietnamese_12);
LV_FONT_DECLARE(lv_font_montserrat_vietnamese_14);
}
#include <new>

namespace ui {
namespace {
constexpr float kPi = 3.14159265358979323846f;
struct UnitPoint {
    float x;
    float y;
};

template <std::size_t Count>
const std::array<UnitPoint, Count>& unitCircle() {
    // Halo used to evaluate hundreds of sin/cos calls on every frame. Cache
    // the fixed unit-circle geometry once and rotate it with one sin/cos pair.
    static const auto points = [] {
        std::array<UnitPoint, Count> result{};
        for (std::size_t i = 0; i < Count; ++i) {
            const float angle = 2.0f * kPi * static_cast<float>(i) /
                                static_cast<float>(Count);
            result[i] = {std::cos(angle), std::sin(angle)};
        }
        return result;
    }();
    return points;
}

float fastSin(float angle) {
    constexpr float toIndex = 256.0f / (2.0f * kPi);
    const auto index = static_cast<unsigned>(static_cast<int>(angle * toIndex));
    return unitCircle<256>()[index & 255U].y;
}

float fastCos(float angle) {
    constexpr float toIndex = 256.0f / (2.0f * kPi);
    const auto index = static_cast<unsigned>(static_cast<int>(angle * toIndex));
    return unitCircle<256>()[index & 255U].x;
}

lv_color_t rgb(int r, int g, int b) {
    return lv_color_make(static_cast<std::uint8_t>(r),
                         static_cast<std::uint8_t>(g),
                         static_cast<std::uint8_t>(b));
}
// Keep the approved warm VU separate from the Windows-inspired cool plots.
const lv_color_t kBackground = rgb(9, 20, 31);
const lv_color_t kFace = rgb(233, 214, 174);
const lv_color_t kGrid = rgb(187, 163, 123);
const lv_color_t kInk = rgb(72, 53, 35);
const lv_color_t kRust = rgb(177, 58, 43);
const lv_color_t kAmber = rgb(194, 151, 87);
const lv_color_t kText = rgb(241, 238, 224);
const lv_color_t kMutedText = rgb(174, 185, 184);
const lv_color_t kButtonInk = rgb(20, 25, 25);
const lv_color_t kColdPanel = rgb(7, 29, 44);
const lv_color_t kColdGrid = rgb(39, 76, 91);
const lv_color_t kCyan = rgb(106, 225, 230);
const lv_color_t kViolet = rgb(91, 69, 214);
lv_color_t haloColor(float frequency) {
    const float t = std::clamp(frequency, 0.0f, 1.0f);
    const float u = t < 0.5f ? t * 2.0f : (t - 0.5f) * 2.0f;
    const int r0 = t < 0.5f ? 106 : 75;
    const int g0 = t < 0.5f ? 225 : 155;
    const int b0 = t < 0.5f ? 230 : 224;
    const int r1 = t < 0.5f ? 75 : 143;
    const int g1 = t < 0.5f ? 155 : 111;
    const int b1 = t < 0.5f ? 224 : 229;
    return rgb(static_cast<int>(r0 + (r1 - r0) * u),
               static_cast<int>(g0 + (g1 - g0) * u),
               static_cast<int>(b0 + (b1 - b0) * u));
}
lv_color_t waterfallColor(std::uint8_t level) {
    if (level < 10) return rgb(10, 31, 45);
    if (level < 52) return rgb(67, 54, 137);
    if (level < 103) return rgb(82, 76, 174);
    if (level < 154) return rgb(43, 126, 190);
    if (level < 205) return rgb(72, 187, 213);
    if (level < 238) return rgb(111, 226, 233);
    return rgb(201, 248, 250);
}
float spectrumAt(const audio::AudioFrame& frame, float position) {
    const float scaled = std::clamp(position, 0.0f, 1.0f) *
                         static_cast<float>(audio::kBandCount - 1);
    const auto left = static_cast<std::size_t>(scaled);
    const auto right = std::min(left + 1, audio::kBandCount - 1);
    const float fraction = scaled - static_cast<float>(left);
    return frame.bands[left] * (1.0f - fraction) + frame.bands[right] * fraction;
}
lv_obj_t* label(lv_obj_t* parent, int x, int y, const char* value,
                const lv_font_t* font, lv_color_t color) {
    lv_obj_t* item = lv_label_create(parent);
    lv_obj_set_pos(item, x, y);
    lv_obj_set_style_text_font(item, font, 0);
    lv_obj_set_style_text_color(item, color, 0);
    lv_label_set_text(item, value);
    return item;
}
void setLabelIfChanged(lv_obj_t* item, const char* text) {
    if (std::strcmp(lv_label_get_text(item), text) != 0) lv_label_set_text(item, text);
}
float meterPosition(float db) {
    return std::clamp((db + 20.0f) / 23.0f, 0.0f, 1.0f);
}
struct MeterTick {
    std::int8_t x0, y0, x1, y1;
};
const std::array<MeterTick, 47>& meterTicks() {
    // All tick geometry is static. Keep only 188 bytes of coordinates rather
    // than repeat 188 trigonometric calls whenever either needle moves.
    static const auto ticks = [] {
        std::array<MeterTick, 47> result{};
        for (std::size_t i = 0; i < result.size(); ++i) {
            const float angle = (210.0f + 120.0f * i / 46.0f) * kPi / 180.0f;
            const int inner = i % 10 == 0 || i == 46 ? 66 : 71;
            result[i] = {static_cast<std::int8_t>(std::cos(angle) * inner),
                         static_cast<std::int8_t>(std::sin(angle) * inner),
                         static_cast<std::int8_t>(std::cos(angle) * 76),
                         static_cast<std::int8_t>(std::sin(angle) * 76)};
        }
        return result;
    }();
    return ticks;
}
bool conceptStyle(VisualizerUi::SpectrumStyle style) {
    return style == VisualizerUi::SpectrumStyle::NeonPulse ||
           style == VisualizerUi::SpectrumStyle::Mirror;
}
const char* spectrumStyleName(VisualizerUi::SpectrumStyle style) {
    switch (style) {
    case VisualizerUi::SpectrumStyle::Bars: return "BARS";
    case VisualizerUi::SpectrumStyle::Trace: return "TRACE";
    case VisualizerUi::SpectrumStyle::Dots: return "DOTS";
    case VisualizerUi::SpectrumStyle::NeonPulse: return "PULSE";
    case VisualizerUi::SpectrumStyle::Mirror: return "MIRROR";
    case VisualizerUi::SpectrumStyle::Waveform: return "WAVEFORM";
    case VisualizerUi::SpectrumStyle::Oscilloscope: return "SCOPE";
    case VisualizerUi::SpectrumStyle::Halo: return "HALO";
    case VisualizerUi::SpectrumStyle::Waterfall: return "WATERFALL";
    case VisualizerUi::SpectrumStyle::Ridgeline: return "RIDGELINE";
    case VisualizerUi::SpectrumStyle::TopoBloom: return "TOPO BLOOM";
    case VisualizerUi::SpectrumStyle::AuroraWeave: return "AURORA";
    case VisualizerUi::SpectrumStyle::CymaticMesh: return "CYMATIC";
    case VisualizerUi::SpectrumStyle::Count: break;
    }
    return "BARS";
}
struct ConceptColors {
    lv_color_t panel, grid, accent, secondary, peak;
};
ConceptColors conceptColors(VisualizerUi::SpectrumStyle style) {
    using Style = VisualizerUi::SpectrumStyle;
    switch (style) {
    case Style::NeonPulse: return {kColdPanel, kColdGrid, kCyan,
                                  rgb(68, 86, 220), rgb(193, 247, 248)};
    case Style::Mirror: return {kColdPanel, kColdGrid, kCyan,
                               kViolet, rgb(193, 247, 248)};
    case Style::Waveform: return {kColdPanel, kColdGrid, kCyan,
                                 rgb(142, 177, 238), rgb(193, 247, 248)};
    case Style::Halo: return {kColdPanel, kColdGrid, kCyan,
                             rgb(143, 111, 229), rgb(193, 247, 248)};
    case Style::Waterfall: return {kColdPanel, kColdGrid, kCyan,
                                  rgb(82, 76, 174), rgb(201, 248, 250)};
    case Style::Ridgeline: return {kColdPanel, kColdGrid, rgb(128, 224, 236),
                                  rgb(96, 119, 184), rgb(201, 248, 250)};
    case Style::TopoBloom: return {kColdPanel, kColdGrid, rgb(100, 222, 230),
                                  rgb(90, 78, 202), rgb(210, 249, 250)};
    case Style::AuroraWeave: return {kColdPanel, kColdGrid, rgb(98, 224, 231),
                                    rgb(91, 91, 218), rgb(207, 249, 250)};
    case Style::CymaticMesh: return {kColdPanel, kColdGrid, rgb(88, 199, 221),
                                    rgb(98, 88, 209), rgb(215, 250, 251)};
    default: return {kColdPanel, kColdGrid, kCyan,
                     kViolet, rgb(193, 247, 248)};
    }
}
std::uint8_t paletteIndex(lv_color_t color) {
    return indexed_palette::index(color);
}
} // namespace

VisualizerUi::VisualizerUi()
    : ownedBuffer_(new (std::nothrow) lv_color_t[kCanvasPixels]),
      buffer_(ownedBuffer_.get()) {
    if (buffer_) initialize();
}

VisualizerUi::VisualizerUi(lv_color_t* canvasBuffer, std::size_t pixelCount)
    : buffer_(pixelCount >= kCanvasPixels ? canvasBuffer : nullptr) {
    if (buffer_) initialize();
}

VisualizerUi::VisualizerUi(std::uint8_t* indexedBuffer, std::size_t byteCount,
                           CanvasStorage storage)
    : indexedBuffer_(byteCount >= LV_CANVAS_BUF_SIZE_INDEXED_8BIT(kWidth, kHeight)
                         ? indexedBuffer : nullptr) {
    (void)storage;
    if (indexedBuffer_) initialize();
}

VisualizerUi::~VisualizerUi() {
    // LVGL must outlive this object. Delete the canvas before its owned pixels.
    if (!ready()) return;
    for (auto* item : annotations_) lv_obj_del(item);
    for (auto* item : {leftValue_, rightValue_, title_, modeLabel_, pageLabel_,
                       status_, styleButton_, button_, canvas_}) lv_obj_del(item);
}

void VisualizerUi::initialize() {
    // A refresh before the first render must never expose uninitialized pixels.
    if (indexedBuffer_) {
        std::fill_n(indexedBuffer_,
                    LV_CANVAS_BUF_SIZE_INDEXED_8BIT(kWidth, kHeight), 0);
        indexPixels_ = indexedBuffer_ + 4 * 256;
    } else {
        std::fill_n(buffer_, kCanvasPixels, lv_color_t{});
    }
    lv_obj_t* screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, kBackground, 0);
    canvas_ = lv_canvas_create(screen);
    if (indexedBuffer_) {
        lv_canvas_set_buffer(canvas_, indexedBuffer_, kWidth, kHeight,
                             LV_IMG_CF_INDEXED_8BIT);
        for (unsigned index = 0; index < 256; ++index)
            lv_canvas_set_palette(canvas_, static_cast<std::uint8_t>(index),
                                  indexed_palette::color(
                                      static_cast<std::uint8_t>(index)));
    } else {
        lv_canvas_set_buffer(canvas_, buffer_, kWidth, kHeight,
                             LV_IMG_CF_TRUE_COLOR);
    }
    lv_obj_set_pos(canvas_, 0, 0);
    title_ = label(screen, 20, 7, "STUDIO MONITOR", &lv_font_montserrat_14,
                   kText);
    modeLabel_ = label(screen, 20, 27, "ANALOG VU", &lv_font_montserrat_10,
                       kMutedText);
    pageLabel_ = label(screen, 268, 13, "01 / 03", &lv_font_montserrat_10,
                       rgb(215, 171, 105));
    status_ = label(screen, 24, 213, "STARTING AUDIO", &lv_font_montserrat_10,
                    kMutedText);
    lv_obj_set_width(status_, 194);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_DOT);
    button_ = lv_btn_create(screen);
    lv_obj_set_size(button_, 58, 26);
    lv_obj_set_pos(button_, 254, 205);
    lv_obj_set_ext_click_area(button_, 9);
    lv_obj_set_style_radius(button_, 5, 0);
    lv_obj_set_style_bg_color(button_, kAmber, 0);
    lv_obj_set_style_bg_color(button_, rgb(173, 229, 238), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(button_, 0, 0);
    lv_obj_set_style_border_width(button_, 0, 0);
    lv_obj_add_event_cb(button_, &VisualizerUi::buttonEvent, LV_EVENT_CLICKED, this);
    lv_obj_t* buttonText = label(button_, 0, 0, "MODE  >",
                                  &lv_font_montserrat_10, kButtonInk);
    lv_obj_center(buttonText);
    styleButton_ = lv_btn_create(screen);
    lv_obj_set_size(styleButton_, 58, 26);
    lv_obj_set_pos(styleButton_, 254, 9);
    lv_obj_set_ext_click_area(styleButton_, 9);
    lv_obj_set_style_radius(styleButton_, 5, 0);
    lv_obj_set_style_bg_color(styleButton_, kAmber, 0);
    lv_obj_set_style_bg_color(styleButton_, rgb(173, 229, 238), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(styleButton_, 0, 0);
    lv_obj_set_style_border_width(styleButton_, 0, 0);
    lv_obj_add_event_cb(styleButton_, &VisualizerUi::styleButtonEvent, LV_EVENT_CLICKED, this);
    lv_obj_t* styleButtonText = label(styleButton_, 0, 0, "STYLE  >",
                                     &lv_font_montserrat_10, kButtonInk);
    lv_obj_center(styleButtonText);
    for (auto& item : annotations_)
        item = label(screen, 0, 0, "", &lv_font_montserrat_10, kMutedText);
    leftValue_ = label(screen, 0, 0, "", &lv_font_montserrat_12, kText);
    rightValue_ = label(screen, 0, 0, "", &lv_font_montserrat_12, kText);
    updateLabels();
}

void VisualizerUi::buttonEvent(lv_event_t* event) {
    auto* self = static_cast<VisualizerUi*>(lv_event_get_user_data(event));
    self->dispatchTouchAction(TouchAction::NextMode);
}
void VisualizerUi::styleButtonEvent(lv_event_t* event) {
    auto* self = static_cast<VisualizerUi*>(lv_event_get_user_data(event));
    self->dispatchTouchAction(TouchAction::NextSpectrumStyle);
}
void VisualizerUi::dispatchTouchAction(TouchAction action) {
    if (touchActionHandler_) {
        touchActionHandler_(touchActionContext_, action);
    } else if (action == TouchAction::NextMode) {
        nextMode();
    } else {
        nextSpectrumStyle();
    }
}
void VisualizerUi::setTouchActionHandler(TouchActionHandler handler, void* context) {
    touchActionHandler_ = handler;
    touchActionContext_ = context;
}
void VisualizerUi::nextMode() {
    if (!ready()) return;
    setModeId(nextSupportedMode(mode_, capabilities_));
}
void VisualizerUi::setModeId(ModeId mode) {
    if (!ready() || !capabilities_.supports(mode) || mode == mode_) return;
    mode_ = mode;
    modeChanged_ = true;
    if (mode_ == ModeId::Spectrum && spectrumStyle_ == SpectrumStyle::Waveform)
        resetWaveform();
    updateLabels();
}
void VisualizerUi::setMode(unsigned oneBasedMode) {
    if (oneBasedMode == 0 || oneBasedMode > kVisibleModeOrder.size()) return;
    setModeId(kVisibleModeOrder[oneBasedMode - 1]);
}
void VisualizerUi::setCapabilities(UiCapabilities capabilities) {
    capabilities_ = capabilities;
    if (!capabilities_.supports(mode_)) mode_ = firstSupportedMode(capabilities_);
    if (!capabilities_.supports(spectrumStyle_))
        spectrumStyle_ = firstSupportedSpectrumStyle(capabilities_);
    modeChanged_ = true;
    if (ready()) updateLabels();
}
void VisualizerUi::setSpectrumStyle(SpectrumStyle style) {
    if (!ready() || style == spectrumStyle_ || !capabilities_.supports(style)) return;
    spectrumStyle_ = style;
    modeChanged_ = true;
    if (style == SpectrumStyle::Waveform) resetWaveform();
    if (style == SpectrumStyle::Waterfall || style == SpectrumStyle::Ridgeline)
        resetSpectrumHistory();
    if (mode_ == ModeId::Spectrum) updateLabels();
}
void VisualizerUi::nextSpectrumStyle() {
    setSpectrumStyle(ui::nextSupportedSpectrumStyle(spectrumStyle_, capabilities_));
}
void VisualizerUi::setStatus(const std::string& status) {
    // LV_LABEL_LONG_DOT edits its own text while truncating. Cache the original
    // status so repeated long strings do not trigger needless label redraws.
    if (!ready() || status == statusText_) return;
    statusText_ = status;
    lv_label_set_text(status_, status.c_str());
}
void VisualizerUi::updateLabels() {
    lv_obj_set_pos(pageLabel_, 268, 13);
    lv_obj_set_pos(title_, 20, 7);
    lv_obj_set_style_text_letter_space(title_, 0, 0);
    lv_obj_clear_flag(modeLabel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(styleButton_, LV_OBJ_FLAG_HIDDEN);
    for (auto* item : annotations_) {
        lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(item, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_letter_space(item, 0, 0);
        lv_obj_set_width(item, LV_SIZE_CONTENT);
        lv_label_set_long_mode(item, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_LEFT, 0);
    }
    lv_obj_add_flag(leftValue_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rightValue_, LV_OBJ_FLAG_HIDDEN);
    auto annotation = [this](std::size_t index, int x, int y, const char* value,
                             lv_color_t color) {
        lv_obj_t* item = annotations_[index];
        lv_label_set_text(item, value);
        lv_obj_set_pos(item, x, y);
        lv_obj_set_style_text_color(item, color, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_HIDDEN);
    };
    lv_label_set_text(title_, "STUDIO MONITOR");
    lv_obj_set_style_text_color(title_, kText, 0);
    lv_obj_set_style_text_color(modeLabel_, kMutedText, 0);
    lv_obj_set_style_text_color(pageLabel_, rgb(215, 171, 105), 0);
    lv_obj_set_style_text_color(status_, kMutedText, 0);
    lv_obj_set_style_bg_color(button_, kAmber, 0);
    lv_obj_set_style_bg_color(styleButton_, kAmber, 0);
    lv_obj_set_style_bg_color(button_, rgb(173, 229, 238), LV_STATE_PRESSED);
    lv_obj_set_style_text_font(leftValue_, &lv_font_montserrat_12, 0);
    lv_obj_set_width(leftValue_, 205);
    lv_label_set_long_mode(leftValue_, LV_LABEL_LONG_DOT);
    if (mode_ == ModeId::Vu) {
        lv_obj_set_style_text_color(title_, rgb(241, 238, 224), 0);
        lv_obj_set_style_text_color(modeLabel_, rgb(174, 185, 184), 0);
        lv_obj_set_style_text_color(pageLabel_, rgb(215, 171, 105), 0);
        lv_obj_set_style_text_color(status_, rgb(171, 185, 181), 0);
        lv_obj_set_style_bg_color(button_, rgb(194, 151, 87), 0);
        lv_obj_set_style_bg_color(button_, rgb(173, 229, 238), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(leftValue_, rgb(72, 53, 35), 0);
        lv_obj_set_style_text_color(rightValue_, rgb(72, 53, 35), 0);
        lv_label_set_text(modeLabel_, "ANALOG VU  /  STEREO");
        lv_label_set_text(pageLabel_, "02 / 07");
        for (int side = 0; side < 2; ++side) {
            const int x = side == 0 ? 8 : 165;
            const std::size_t offset = static_cast<std::size_t>(side) * 4;
            const auto ink = rgb(102, 76, 49);
            annotation(offset, x + 9, 143, "-20", ink);
            annotation(offset + 1, x + 55, 117, "-10", ink);
            annotation(offset + 2, x + 106, 123, "0", ink);
            annotation(offset + 3, x + 122, 143, "+3", rgb(169, 63, 47));
            annotation(8 + side, x + 14, 56, side == 0 ? "LEFT" : "RIGHT",
                       rgb(72, 57, 38));
            annotation(10 + side, x + 108, 57, "PK", rgb(126, 72, 51));
        }
        lv_obj_set_pos(leftValue_, 31, 178);
        lv_obj_set_pos(rightValue_, 188, 178);
        lv_obj_clear_flag(leftValue_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(rightValue_, LV_OBJ_FLAG_HIDDEN);
    } else if (mode_ == ModeId::Spectrum) {
        lv_obj_set_style_text_color(pageLabel_, rgb(126, 199, 218), 0);
        lv_obj_set_style_text_color(modeLabel_, rgb(167, 190, 204), 0);
        lv_obj_set_style_bg_color(button_, kCyan, 0);
        lv_obj_set_style_bg_color(styleButton_, kCyan, 0);
        lv_obj_set_pos(pageLabel_, 185, 13);
        lv_obj_clear_flag(styleButton_, LV_OBJ_FLAG_HIDDEN);
        const char* style = spectrumStyleName(spectrumStyle_);
        char title[32];
        std::snprintf(title, sizeof(title), "SPECTRUM  /  %s", style);
        lv_label_set_text(modeLabel_, title);
        char page[16];
        std::snprintf(page, sizeof(page), "%02u / %02u",
                      static_cast<unsigned>(spectrumStyle_) + 1,
                      static_cast<unsigned>(SpectrumStyle::Count));
        lv_label_set_text(pageLabel_, page);
        if (spectrumStyle_ == SpectrumStyle::Waveform) {
            const auto palette = conceptColors(spectrumStyle_);
            annotation(0, 17, 57, "LIVE AUDIO  /  PEAK ENVELOPE", palette.peak);
            annotation(1, 17, 181, "ROLLING HISTORY", rgb(142, 169, 182));
            annotation(2, 272, 181, "TIME >", rgb(142, 169, 182));
        } else if (spectrumStyle_ == SpectrumStyle::Oscilloscope) {
            // The traces use the complete panel. Avoid LVGL labels over this
            // direct-DMA region so a trace update never overwrites text.
        } else if (spectrumStyle_ == SpectrumStyle::Halo) {
            annotation(0, 17, 57, "RADIAL / LIVE", rgb(193, 247, 248));
            annotation(1, 17, 181, "LOW  /  HIGH  /  LOW", rgb(142, 169, 182));
        } else if (spectrumStyle_ == SpectrumStyle::Waterfall) {
            annotation(0, 17, 57, "SPECTRAL HISTORY", rgb(193, 247, 248));
            annotation(1, 262, 57, "NOW v", rgb(142, 169, 182));
            annotation(2, 17, 181, "LOW", rgb(142, 169, 182));
            annotation(3, 145, 181, "MID", rgb(142, 169, 182));
            annotation(4, 280, 181, "HIGH", rgb(142, 169, 182));
        } else if (spectrumStyle_ == SpectrumStyle::Ridgeline) {
            annotation(0, 17, 57, "SPECTRAL TERRAIN", rgb(193, 247, 248));
            annotation(1, 276, 57, "LIVE", rgb(142, 169, 182));
            annotation(2, 17, 181, "LOW", rgb(142, 169, 182));
            annotation(3, 145, 181, "MID", rgb(142, 169, 182));
            annotation(4, 280, 181, "HIGH", rgb(142, 169, 182));
        } else if (spectrumStyle_ == SpectrumStyle::TopoBloom) {
            annotation(0, 17, 57, "TOPOGRAPHIC ENERGY", rgb(193, 247, 248));
            annotation(1, 274, 57, "DRIFT", rgb(142, 169, 182));
            annotation(2, 17, 181, "LOW", rgb(142, 169, 182));
            annotation(3, 145, 181, "MID", rgb(142, 169, 182));
            annotation(4, 280, 181, "HIGH", rgb(142, 169, 182));
        } else if (spectrumStyle_ == SpectrumStyle::AuroraWeave) {
            annotation(0, 17, 57, "HARMONIC WEAVE", rgb(193, 247, 248));
            annotation(1, 278, 57, "FLOW", rgb(142, 169, 182));
            annotation(2, 17, 181, "LOW", rgb(142, 169, 182));
            annotation(3, 145, 181, "MID", rgb(142, 169, 182));
            annotation(4, 280, 181, "HIGH", rgb(142, 169, 182));
        } else if (spectrumStyle_ == SpectrumStyle::CymaticMesh) {
            annotation(0, 17, 57, "INTERFERENCE FIELD", rgb(193, 247, 248));
            annotation(1, 269, 57, "NODES", rgb(142, 169, 182));
            annotation(2, 17, 181, "LOW", rgb(142, 169, 182));
            annotation(3, 145, 181, "MID", rgb(142, 169, 182));
            annotation(4, 280, 181, "HIGH", rgb(142, 169, 182));
        } else {
            const auto ink = rgb(142, 169, 182);
            annotation(0, 16, 181, "SUB", ink);
            annotation(1, 128, 181, "1K", ink);
            annotation(2, 207, 181, "5K", ink);
            annotation(3, 249, 181, "10K", ink);
            annotation(4, 281, 181, "20K", ink);
        }
    } else if (mode_ == ModeId::LegacyScope) {
        lv_label_set_text(title_, "STUDIO MONITOR");
        lv_obj_set_style_text_color(title_, kText, 0);
        lv_obj_set_style_text_color(pageLabel_, rgb(126, 199, 218), 0);
        lv_obj_set_style_bg_color(button_, kCyan, 0);
        lv_label_set_text(modeLabel_, "OSCILLOSCOPE  /  STEREO");
        lv_label_set_text(pageLabel_, "03 / 07");
        annotation(0, 18, 55, "LEFT  /  CH 1", kCyan);
        annotation(1, 18, 131, "RIGHT  /  CH 2", rgb(142, 177, 238));
    } else {
        lv_obj_add_flag(modeLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(title_, 16, 14);
        lv_obj_set_style_text_letter_space(title_, 2, 0);
        lv_obj_set_style_text_color(title_, kText, 0);
        lv_obj_set_style_text_color(pageLabel_, rgb(126, 199, 218), 0);
        lv_obj_set_style_text_color(modeLabel_, rgb(167, 190, 204), 0);
        lv_obj_set_style_text_color(status_, rgb(142, 169, 182), 0);
        lv_obj_set_style_text_color(leftValue_, rgb(193, 247, 248), 0);
        lv_obj_set_style_bg_color(button_, kCyan, 0);
        if (mode_ == ModeId::Market) {
            lv_label_set_text(title_, "MARKET PULSE");
            lv_label_set_text(pageLabel_, "24H");
            lv_obj_set_pos(pageLabel_, 244, 16);
            lv_obj_set_style_text_font(leftValue_, &lv_font_montserrat_28, 0);
            lv_obj_set_pos(leftValue_, 17, 73);
            lv_obj_set_width(leftValue_, 140);
            lv_obj_clear_flag(leftValue_, LV_OBJ_FLAG_HIDDEN);
            annotation(0, 18, 57, "BTC", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[0], &lv_font_montserrat_14, 0);
            annotation(1, 18, 113, "+0.00%", kCyan);
            lv_obj_set_style_text_font(annotations_[1], &lv_font_montserrat_20, 0);
            annotation(3, 18, 151, "ETH", rgb(193, 247, 248));
            annotation(4, 18, 166, "$0", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[4], &lv_font_montserrat_14, 0);
            annotation(5, 105, 174, "+0.0%", kCyan);
            annotation(6, 168, 151, "SOL", rgb(193, 247, 248));
            annotation(7, 168, 166, "$0", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[7], &lv_font_montserrat_14, 0);
            annotation(8, 255, 174, "+0.0%", kCyan);
        } else if (mode_ == ModeId::Clock) {
            lv_label_set_text(title_, "STUDIO CLOCK");
            lv_label_set_text(pageLabel_, "AUDIO / LIVE");
            lv_obj_set_pos(pageLabel_, 233, 16);
            lv_obj_set_style_text_font(leftValue_, &lv_font_montserrat_40, 0);
            lv_obj_set_pos(leftValue_, 17, 61);
            lv_obj_set_width(leftValue_, 150);
            lv_obj_clear_flag(leftValue_, LV_OBJ_FLAG_HIDDEN);
            annotation(0, 20, 108, "--- -- ---", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[0], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_letter_space(annotations_[0], 1, 0);
            annotation(1, 241, 80, "--", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[1], &lv_font_montserrat_20, 0);
            lv_obj_set_width(annotations_[1], 30);
            lv_obj_set_style_text_align(annotations_[1], LV_TEXT_ALIGN_CENTER, 0);
        } else if (mode_ == ModeId::NowPlaying) {
            lv_label_set_text(title_, "NOW PLAYING");
            lv_label_set_text(pageLabel_, "SOUND / TIME");
            lv_obj_set_pos(pageLabel_, 226, 16);
            lv_obj_set_style_text_font(leftValue_,
                                       &lv_font_montserrat_vietnamese_14, 0);
            lv_obj_set_pos(leftValue_, 109, 63);
            lv_obj_set_width(leftValue_, 187);
            lv_label_set_long_mode(leftValue_, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_style_anim_speed(leftValue_, 24, 0);
            lv_obj_clear_flag(leftValue_, LV_OBJ_FLAG_HIDDEN);
            annotation(0, 110, 87, "AUDIOVIZ", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[0],
                                       &lv_font_montserrat_vietnamese_12, 0);
            lv_obj_set_style_text_letter_space(annotations_[0], 1, 0);
            lv_obj_set_width(annotations_[0], 186);
            lv_label_set_long_mode(annotations_[0], LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_style_anim_speed(annotations_[0], 20, 0);
            annotation(1, 110, 109, "00:00 / 04:08", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[1], &lv_font_montserrat_12, 0);
        } else if (mode_ == ModeId::DeskBoard) {
            lv_label_set_text(title_, "DESK BOARD");
            lv_label_set_text(pageLabel_, "FOCUS / TODAY");
            lv_obj_set_pos(pageLabel_, 219, 16);
            lv_obj_set_style_text_font(leftValue_, &lv_font_montserrat_40, 0);
            lv_obj_set_pos(leftValue_, 17, 61);
            lv_obj_set_width(leftValue_, 142);
            lv_obj_clear_flag(leftValue_, LV_OBJ_FLAG_HIDDEN);
            annotation(0, 20, 108, "--- -- ---", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[0], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_letter_space(annotations_[0], 1, 0);
            annotation(1, 247, 58, "27 C", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[1], &lv_font_montserrat_14, 0);
            annotation(2, 181, 91, "BTC", rgb(142, 169, 182));
            annotation(3, 181, 105, "$0", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[3], &lv_font_montserrat_12, 0);
            annotation(4, 259, 106, "+0.0%", kCyan);
            annotation(5, 226, 126, "FOCUS", rgb(142, 169, 182));
            annotation(6, 226, 141, "42 MIN", rgb(193, 247, 248));
            lv_obj_set_style_text_font(annotations_[6], &lv_font_montserrat_14, 0);
            annotation(7, 105, 184, "A CALMER DESKTOP", rgb(126, 199, 218));
            annotation(8, 218, 76, "HANOI", rgb(142, 169, 182));
        }
    }
}

void VisualizerUi::pixel(int x, int y, lv_color_t color) {
    if (x >= clipArea_.x1 && x <= clipArea_.x2 &&
        y >= clipArea_.y1 && y <= clipArea_.y2 &&
        x >= 0 && x < kWidth && y >= 0 && y < kHeight) {
        if (indexPixels_)
            indexPixels_[static_cast<std::size_t>(y) * kWidth + x] =
                paletteIndex(color);
        else
            buffer_[static_cast<std::size_t>(y) * kWidth + x] = color;
    }
}
void VisualizerUi::line(int x0, int y0, int x1, int y1, lv_color_t color) {
    // Most grid/baseline segments are axis-aligned. Route these through the
    // clipped span writer: a narrow dirty column must not walk all 304 pixels
    // of every horizontal grid line for every changed Spectrum band.
    if (y0 == y1) {
        rect(std::min(x0, x1), y0, std::abs(x1 - x0) + 1, 1, color);
        return;
    }
    if (x0 == x1) {
        rect(x0, std::min(y0, y1), 1, std::abs(y1 - y0) + 1, color);
        return;
    }
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * error;
        if (e2 >= dy) { error += dy; x0 += sx; }
        if (e2 <= dx) { error += dx; y0 += sy; }
    }
}
void VisualizerUi::rect(int x, int y, int w, int h, lv_color_t color) {
    const int left = std::max({x, 0, static_cast<int>(clipArea_.x1)});
    const int right = std::min({x + w, kWidth, static_cast<int>(clipArea_.x2) + 1});
    const int top = std::max({y, 0, static_cast<int>(clipArea_.y1)});
    const int bottom = std::min({y + h, kHeight, static_cast<int>(clipArea_.y2) + 1});
    if (left >= right) return;
    if (indexPixels_) {
        const auto index = paletteIndex(color);
        for (int row = top; row < bottom; ++row)
            std::fill(indexPixels_ + static_cast<std::size_t>(row) * kWidth + left,
                      indexPixels_ + static_cast<std::size_t>(row) * kWidth + right,
                      index);
    } else {
        for (int row = top; row < bottom; ++row)
            std::fill(buffer_ + static_cast<std::size_t>(row) * kWidth + left,
                      buffer_ + static_cast<std::size_t>(row) * kWidth + right,
                      color);
    }
}
void VisualizerUi::frameRect(int x, int y, int w, int h, lv_color_t color) {
    line(x, y, x + w - 1, y, color);
    line(x, y + h - 1, x + w - 1, y + h - 1, color);
    line(x, y, x, y + h - 1, color);
    line(x + w - 1, y, x + w - 1, y + h - 1, color);
}
void VisualizerUi::disc(int cx, int cy, int radius, lv_color_t color) {
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x)
            if (x * x + y * y <= radius * radius) pixel(cx + x, cy + y, color);
}
void VisualizerUi::clear(lv_color_t color) {
    if (indexPixels_)
        std::fill_n(indexPixels_, kCanvasPixels, paletteIndex(color));
    else
        std::fill_n(buffer_, kCanvasPixels, color);
}
void VisualizerUi::invalidate(const lv_area_t& area) {
    const bool directStyle = spectrumStyle_ == SpectrumStyle::Bars ||
        spectrumStyle_ == SpectrumStyle::Dots ||
        spectrumStyle_ == SpectrumStyle::NeonPulse ||
        spectrumStyle_ == SpectrumStyle::Mirror ||
        spectrumStyle_ == SpectrumStyle::Waveform ||
        spectrumStyle_ == SpectrumStyle::Oscilloscope ||
        spectrumStyle_ == SpectrumStyle::Halo ||
        spectrumStyle_ == SpectrumStyle::Waterfall ||
        spectrumStyle_ == SpectrumStyle::Ridgeline ||
        spectrumStyle_ == SpectrumStyle::TopoBloom ||
        spectrumStyle_ == SpectrumStyle::AuroraWeave ||
        spectrumStyle_ == SpectrumStyle::CymaticMesh;
    const bool directRegion = (mode_ == ModeId::Spectrum && directStyle) ||
        (mode_ == ModeId::Clock && area.y1 >= 129) ||
        (mode_ == ModeId::DeskBoard && area.y1 >= 152 && area.y2 <= 182);
    if (directCanvasUpdates_ && !modeChanged_ && directRegion &&
        directDirtyCount_ < directDirtyAreas_.size()) {
        directDirtyAreas_[directDirtyCount_++] = area;
        return;
    }
    // This canvas is always untransformed and covers the screen. LVGL's object
    // invalidation expands every area by 5 px for transformed objects; that
    // joins adjacent 8-px Spectrum columns (4-px gaps) into one broad strip.
    // We edit these canvas pixels directly, so report their exact bounds.
    lv_area_t canvasCoordinates;
    lv_obj_get_coords(canvas_, &canvasCoordinates);
    lv_area_t absolute = area;
    absolute.x1 += canvasCoordinates.x1;
    absolute.x2 += canvasCoordinates.x1;
    absolute.y1 += canvasCoordinates.y1;
    absolute.y2 += canvasCoordinates.y1;
    if (lv_obj_get_screen(canvas_) == lv_scr_act())
        _lv_inv_area(lv_obj_get_disp(canvas_), &absolute);
}

void VisualizerUi::invalidateTiled(const lv_area_t& area) {
    // The ESP32 profile has a 320x8-pixel DMA buffer. Horizontal slices keep
    // every direct indexed-canvas transfer within that fixed capacity.
    constexpr int tileLines = 8;
    for (int y = area.y1; y <= area.y2; y += tileLines) {
        invalidate({area.x1, static_cast<lv_coord_t>(y), area.x2,
                    static_cast<lv_coord_t>(std::min(y + tileLines - 1,
                                                    static_cast<int>(area.y2)))});
    }
}
void VisualizerUi::drawShell() {
    clear(rgb(9, 20, 31));
    rect(0, 0, 320, 44, rgb(16, 30, 43));
    rect(0, 201, 320, 39, rgb(16, 30, 43));
    line(0, 43, 319, 43, rgb(52, 78, 96));
    line(0, 201, 319, 201, rgb(52, 78, 96));
    rect(11, 10, 3, 24, rgb(105, 184, 205));
    disc(14, 220, 4, rgb(74, 150, 176));
    disc(14, 220, 2, rgb(187, 229, 237));
}
void VisualizerUi::drawMeter(int x, float db, bool clip) {
    rect(x + 2, 52, 147, 146, rgb(3, 8, 10));
    rect(x, 50, 147, 146, rgb(113, 84, 55));
    rect(x + 3, 53, 141, 140, rgb(233, 214, 174));
    for (int y = 54; y < 192; ++y) {
        const int shift = (y - 54) / 8;
        rect(x + 4, y, 139, 1, rgb(241 - shift, 225 - 2 * shift, 192 - 3 * shift));
    }
    line(x + 4, 75, x + 142, 75, rgb(187, 163, 123));
    line(x + 4, 171, x + 142, 171, rgb(189, 166, 128));
    const int cx = x + 73, cy = 172;
    const auto& ticks = meterTicks();
    for (std::size_t i = 0; i < ticks.size(); ++i) {
        const auto mark = i >= 39 ? rgb(172, 64, 48) : rgb(103, 78, 51);
        const auto& tick = ticks[i];
        line(cx + tick.x0, cy + tick.y0, cx + tick.x1, cy + tick.y1, mark);
    }
    const float angle = (210.0f + 120.0f * meterPosition(db)) * kPi / 180.0f;
    const int tipX = cx + static_cast<int>(std::cos(angle) * 68.0f);
    const int tipY = cy + static_cast<int>(std::sin(angle) * 68.0f);
    line(cx + 1, cy, tipX + 1, tipY, rgb(124, 84, 54));
    line(cx, cy, tipX, tipY, rgb(177, 58, 43));
    disc(cx, cy, 5, rgb(88, 68, 47));
    disc(cx, cy, 2, rgb(221, 195, 145));
    disc(x + 127, 63, 6, rgb(102, 68, 56));
    disc(x + 127, 63, 4, clip ? rgb(242, 65, 47) : rgb(126, 77, 61));
    frameRect(x + 3, 53, 141, 140, rgb(170, 135, 91));
}
void VisualizerUi::drawVu(const audio::AudioFrame& frame, bool full) {
    const float levels[2] = {frame.leftDb, frame.rightDb};
    const bool clips[2] = {frame.leftClip, frame.rightClip};
    for (int channel = 0; channel < 2; ++channel) {
        const int x = channel == 0 ? 8 : 165;
        const int cx = x + 73, cy = 172;
        const float angle = (210.0f + 120.0f * meterPosition(levels[channel])) * kPi / 180.0f;
        const MeterState next{cx + static_cast<int>(std::cos(angle) * 68.0f),
                              cy + static_cast<int>(std::sin(angle) * 68.0f), clips[channel]};
        const auto& previous = meters_[channel];
        if (full) drawMeter(x, levels[channel], clips[channel]);
        else {
            if (next.tipX != previous.tipX || next.tipY != previous.tipY) {
                // Restore the old needle and the complete new needle/pivot from
                // the original meter artwork, then repaint only this rectangle.
                clipArea_ = {static_cast<lv_coord_t>(std::min({previous.tipX, next.tipX, cx - 5})),
                             static_cast<lv_coord_t>(std::min({previous.tipY, next.tipY, cy - 5})),
                             static_cast<lv_coord_t>(std::max({previous.tipX + 1, next.tipX + 1, cx + 5})),
                             static_cast<lv_coord_t>(cy + 5)};
                drawMeter(x, levels[channel], clips[channel]);
                invalidate(clipArea_);
            }
            if (next.clip != previous.clip) {
                clipArea_ = {static_cast<lv_coord_t>(x + 123), 59,
                             static_cast<lv_coord_t>(x + 131), 67};
                disc(x + 127, 63, 4, clips[channel] ? rgb(242, 65, 47) : rgb(126, 77, 61));
                invalidate(clipArea_);
            }
        }
        meters_[channel] = next;
    }
    char value[24];
    std::snprintf(value, sizeof(value), "%+.1f dB", frame.leftDb);
    setLabelIfChanged(leftValue_, value);
    std::snprintf(value, sizeof(value), "%+.1f dB", frame.rightDb);
    setLabelIfChanged(rightValue_, value);
}
void VisualizerUi::drawSpectrumBase() {
    rect(10, 53, 304, 147, rgb(6, 16, 25));
    rect(8, 51, 304, 147, kColdPanel);
    frameRect(8, 51, 304, 147, kColdGrid);
    line(16, 74, 303, 74, rgb(44, 70, 84));
    for (int y : {100, 126, 152, 178})
        line(16, y, 303, y, rgb(36, 61, 75));
    for (int x : {17, 137, 221, 257, 303})
        line(x, 77, x, 178, rgb(30, 54, 70));
    line(16, 179, 303, 179, rgb(85, 127, 145));
}
void VisualizerUi::drawSpectrumBar(std::size_t band, int height, int peak) {
    const int x = 17 + static_cast<int>(band) * 12;
    constexpr int baseY = 178;
    if (height > 0) {
        rect(x, baseY - height, 8, height, rgb(75, 160, 185));
        rect(x, baseY - height, 8, std::min(height, 9),
             height > 76 ? rgb(179, 219, 236) : rgb(126, 205, 221));
        rect(x, baseY - height, 1, height, rgb(162, 218, 230));
    }
    rect(x, baseY - peak, 8, 2, rgb(220, 241, 247));
    // A zero peak occupies rows 178..179; the original baseline wins at 179.
    line(x, 179, x + 7, 179, rgb(85, 127, 145));
}
void VisualizerUi::drawSpectrumDots(std::size_t band, int height, int peak) {
    const int x = 17 + static_cast<int>(band) * 12;
    constexpr int baseY = 178;
    for (int segment = 0; segment < 16; ++segment) {
        const int bottom = segment * 6;
        if (height <= bottom) break;
        const auto color = segment >= 12 ? rgb(165, 204, 235) :
                           segment >= 8 ? rgb(107, 183, 219) : rgb(67, 133, 176);
        rect(x + 1, baseY - bottom - 5, 6, 4, color);
    }
    rect(x, baseY - peak, 8, 2, rgb(220, 241, 247));
    line(x, 179, x + 7, 179, rgb(85, 127, 145));
}
void VisualizerUi::drawSpectrumTrace() {
    constexpr int baseY = 178;
    for (std::size_t i = 0; i < audio::kBandCount; ++i) {
        const int x = 21 + static_cast<int>(i) * 12;
        const int y = baseY - barHeights_[i];
        if (i) {
            const int previousX = x - 12;
            const int previousY = baseY - barHeights_[i - 1];
            line(previousX, previousY + 1, x, y + 1, rgb(46, 98, 123));
            line(previousX, previousY, x, y, rgb(147, 215, 230));
        }
        if (barHeights_[i] > 0) disc(x, y, 2, rgb(194, 235, 241));
        disc(x, baseY - peakHeights_[i], 1, rgb(228, 241, 249));
    }
}
void VisualizerUi::drawSpectrum(const audio::AudioFrame& frame, bool full) {
    if (spectrumStyle_ == SpectrumStyle::Waveform) {
        drawWaveform(frame, full);
        return;
    }
    if (spectrumStyle_ == SpectrumStyle::Oscilloscope) {
        drawScope(frame, full);
        return;
    }
    if (spectrumStyle_ == SpectrumStyle::Halo) {
        drawHalo(frame, full);
        return;
    }
    if (spectrumStyle_ == SpectrumStyle::Waterfall) {
        drawWaterfall(frame, full);
        return;
    }
    if (spectrumStyle_ == SpectrumStyle::Ridgeline) {
        drawRidgeline(frame, full);
        return;
    }
    if (spectrumStyle_ == SpectrumStyle::TopoBloom) {
        drawTopoBloom(frame, full);
        return;
    }
    if (spectrumStyle_ == SpectrumStyle::AuroraWeave) {
        drawAuroraWeave(frame, full);
        return;
    }
    if (spectrumStyle_ == SpectrumStyle::CymaticMesh) {
        drawCymaticMesh(frame, full);
        return;
    }
    if (conceptStyle(spectrumStyle_)) {
        drawConceptSpectrum(frame, full);
        return;
    }
    if (full) drawSpectrumBase();
    if (spectrumStyle_ == SpectrumStyle::Trace) {
        int firstChanged = static_cast<int>(audio::kBandCount);
        int lastChanged = -1;
        int highest = 0;
        for (std::size_t i = 0; i < audio::kBandCount; ++i) {
            const int height = std::clamp(static_cast<int>(frame.bands[i] * 99.0f), 0, 99);
            const int peak = std::clamp(static_cast<int>(frame.peaks[i] * 99.0f), 0, 99);
            if (height != barHeights_[i] || peak != peakHeights_[i]) {
                firstChanged = std::min(firstChanged, static_cast<int>(i));
                lastChanged = static_cast<int>(i);
            }
            highest = std::max({highest, height, peak, barHeights_[i], peakHeights_[i]});
            barHeights_[i] = height;
            peakHeights_[i] = peak;
        }
        if (!full && lastChanged < 0) return;
        if (!full) {
            clipArea_ = {static_cast<lv_coord_t>(21 + std::max(0, firstChanged - 1) * 12 - 2),
                         static_cast<lv_coord_t>(std::max(76, 178 - highest - 3)),
                         static_cast<lv_coord_t>(21 + std::min(23, lastChanged + 1) * 12 + 2),
                         179};
            drawSpectrumBase();
        }
        drawSpectrumTrace();
        if (!full) invalidate(clipArea_);
        return;
    }
    for (std::size_t i = 0; i < audio::kBandCount; ++i) {
        const int height = std::clamp(static_cast<int>(frame.bands[i] * 99.0f), 0, 99);
        const int peak = std::clamp(static_cast<int>(frame.peaks[i] * 99.0f), 0, 99);
        if (!full && height == barHeights_[i] && peak == peakHeights_[i]) continue;
        if (!full) {
            const int x = 17 + static_cast<int>(i) * 12;
            int top = 178 - std::max({height, peak, barHeights_[i], peakHeights_[i]});
            if (spectrumStyle_ == SpectrumStyle::Dots) {
                const auto dotTop = [](int value) {
                    return value > 0 ? 178 - (((value - 1) / 6) * 6 + 5) : 178;
                };
                top = std::min({top, dotTop(height), dotTop(barHeights_[i])});
            }
            clipArea_ = {static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(top),
                         static_cast<lv_coord_t>(x + 7), 179};
            drawSpectrumBase();
        }
        if (spectrumStyle_ == SpectrumStyle::Dots) drawSpectrumDots(i, height, peak);
        else drawSpectrumBar(i, height, peak);
        if (!full) invalidate(clipArea_);
        barHeights_[i] = height;
        peakHeights_[i] = peak;
    }
}
void VisualizerUi::drawPulseRibbon() {
    // A single connected energy ribbon, deliberately unlike the discrete
    // columns of Bars/Mirror or the thin contour of Trace.
    constexpr int center = 127;
    const auto palette = conceptColors(SpectrumStyle::NeonPulse);
    line(17, center, 303, center, rgb(69, 118, 144));
    for (int x = 17; x <= 303; ++x) {
        const int position = std::clamp(x - 21, 0, 23 * 12);
        const int leftBand = std::min(position / 12, 22);
        const int fraction = position - leftBand * 12;
        const int level = (barHeights_[leftBand] * (12 - fraction) +
                           barHeights_[leftBand + 1] * fraction + 6) / 12;
        const int amplitude = level * 38 / 100;
        if (!amplitude) continue;
        rect(x, center - amplitude - 3, 1, amplitude * 2 + 7, rgb(28, 48, 89));
        rect(x, center - amplitude, 1, amplitude * 2 + 1, rgb(42, 119, 154));
        const int core = std::max(1, amplitude / 4);
        rect(x, center - core, 1, core * 2 + 1, palette.accent);
        pixel(x, center - amplitude, palette.peak);
        pixel(x, center + amplitude, palette.peak);
    }
    for (std::size_t i = 0; i < audio::kBandCount; ++i) {
        const int x = 21 + static_cast<int>(i) * 12;
        const int y = center - peakHeights_[i] * 40 / 100 - 5;
        rect(x - 1, y, 3, 2, palette.peak);
    }
}

void VisualizerUi::drawConceptSpectrum(const audio::AudioFrame& frame, bool full) {
    const auto style = spectrumStyle_;
    const auto palette = conceptColors(style);
    const auto previousHeights = barHeights_;
    const auto previousPeaks = peakHeights_;
    std::array<bool, audio::kBandCount> dirtyBands{};
    bool changed = full;
    for (std::size_t i = 0; i < audio::kBandCount; ++i) {
        const int height = std::clamp(static_cast<int>(frame.bands[i] * 100.0f), 0, 100);
        const int peak = std::clamp(static_cast<int>(frame.peaks[i] * 100.0f), 0, 100);
        dirtyBands[i] = height != barHeights_[i] || peak != peakHeights_[i];
        changed |= dirtyBands[i];
        barHeights_[i] = height;
        peakHeights_[i] = peak;
    }
    if (!changed) return;
    clipArea_ = {8, 51, 311, 197};
    drawSpectrumBase();
    if (style == SpectrumStyle::NeonPulse) drawPulseRibbon();
    else {
        constexpr int base = 124;
        line(16, base, 303, base, palette.secondary);
        for (std::size_t i = 0; i < audio::kBandCount; ++i) {
            const int x = 17 + static_cast<int>(i) * 12;
            const int height = barHeights_[i] * 55 / 100;
            const int peak = peakHeights_[i] * 55 / 100;
            if (height > 0) {
                rect(x, base - height, 8, height, palette.accent);
                rect(x, base, 8, height * 2 / 3, palette.secondary);
            }
            rect(x, base - peak - 4, 8, 2, palette.peak);
        }
    }
    if (!full) {
        if (style == SpectrumStyle::NeonPulse) {
            // A changed band affects both neighboring ribbon segments. Send
            // non-overlapping <=24x95 tiles; each fits the 2560-pixel DMA buffer.
            for (int left = 17; left <= 303; left += 24) {
                const int right = std::min(left + 23, 303);
                bool dirty = false;
                for (std::size_t i = 0; i < audio::kBandCount; ++i) {
                    const int center = 21 + static_cast<int>(i) * 12;
                    dirty |= dirtyBands[i] && center >= left - 12 && center <= right + 12;
                }
                if (dirty) invalidate({static_cast<lv_coord_t>(left), 79,
                                       static_cast<lv_coord_t>(right), 173});
            }
            return;
        }
        for (std::size_t i = 0; i < audio::kBandCount; ++i) {
            if (!dirtyBands[i]) continue;
            const int x = 17 + static_cast<int>(i) * 12;
            constexpr int base = 124;
            const int oldHeight = previousHeights[i] * 55 / 100;
            const int newHeight = barHeights_[i] * 55 / 100;
            const int oldPeak = previousPeaks[i] * 55 / 100;
            const int newPeak = peakHeights_[i] * 55 / 100;
            const int top = std::min({base - oldHeight, base - newHeight,
                                      base - oldPeak - 4, base - newPeak - 4});
            const int bottom = base + std::max(oldHeight, newHeight) * 2 / 3;
            invalidate({static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(top),
                        static_cast<lv_coord_t>(x + 7), static_cast<lv_coord_t>(bottom)});
        }
    }
}
void VisualizerUi::drawHalo(const audio::AudioFrame& frame, bool full) {
    // Draw a native 320x240 radial visualization from our own 24-band frame.
    // The PC and embedded renderers can share the audio frame without using
    // WebGL, a copied shader, or additional per-frame heap allocations.
    constexpr int cx = 160, cy = 124;
    constexpr int spokes = 64;
    constexpr float baseRadius = 42.0f;
    float bass = 0.0f;
    for (std::size_t i = 0; i < 5; ++i) bass += frame.bands[i];
    bass = std::clamp(bass / 5.0f, 0.0f, 1.0f);
    if (frame.generation != haloLastGeneration_) {
        haloPulse_ = std::max(haloPulse_ * 0.83f,
                              std::clamp((bass - haloLastBass_) * 3.0f, 0.0f, 1.0f));
        if (frame.hasAudio) haloPhase_ = std::fmod(haloPhase_ + 0.003f + bass * 0.012f,
                                                   2.0f * kPi);
        haloLastBass_ = bass;
        haloLastGeneration_ = frame.generation;
    } else {
        haloPulse_ *= 0.92f;
    }

    // Only this circle can contain moving Halo pixels. Keeping the rest of the
    // panel static cuts each normal refresh from 44,688 to about 16,300 pixels.
    // The bounding square remains the clip, while direct SPI invalidation below
    // follows the circle in eight-line chords.
    constexpr lv_area_t dynamicArea{88, 52, 232, 196};
    clipArea_ = full ? lv_area_t{8, 51, 311, 197} : dynamicArea;
    if (full) {
        rect(10, 53, 304, 147, rgb(3, 10, 20));
        rect(8, 51, 304, 147, kColdPanel);
        rect(11, 54, 298, 141, rgb(4, 17, 30));
        frameRect(8, 51, 304, 147, kColdGrid);
    } else {
        constexpr int clearRadius = 72;
        for (int dy = -clearRadius; dy <= clearRadius; ++dy) {
            const int halfWidth = static_cast<int>(std::sqrt(
                static_cast<float>(clearRadius * clearRadius - dy * dy)));
            const int y = cy + dy;
            const auto background = y >= 54 && y <= 194 ? rgb(4, 17, 30) : kColdPanel;
            rect(cx - halfWidth, y, halfWidth * 2 + 1, 1, background);
        }
    }

    const auto ring = [this, cx, cy](float radius, lv_color_t color) {
        constexpr int segments = 96;
        const auto& points = unitCircle<segments>();
        int lastX = cx + static_cast<int>(std::lround(points[0].x * radius));
        int lastY = cy + static_cast<int>(std::lround(points[0].y * radius));
        for (int i = 1; i <= segments; ++i) {
            const auto& point = points[static_cast<std::size_t>(i) % points.size()];
            const int x = cx + static_cast<int>(std::lround(point.x * radius));
            const int y = cy + static_cast<int>(std::lround(point.y * radius));
            line(lastX, lastY, x, y, color);
            lastX = x;
            lastY = y;
        }
    };
    ring(42.0f, rgb(31, 72, 91));
    ring(70.0f + haloPulse_ * 2.0f, rgb(23, 54, 78));
    ring(22.0f, rgb(39, 85, 112));
    disc(cx, cy, 17 + static_cast<int>(haloPulse_ * 3.0f), rgb(12, 39, 66));
    disc(cx, cy, 12, rgb(7, 27, 49));

    const float phaseCos = std::cos(haloPhase_);
    const float phaseSin = std::sin(haloPhase_);
    const auto& spokePoints = unitCircle<spokes>();
    for (int i = 0; i < spokes; ++i) {
        const auto& point = spokePoints[static_cast<std::size_t>(i)];
        // Rotate the cached unit point by phase, then by -pi/2.
        const float rotatedX = point.x * phaseCos - point.y * phaseSin;
        const float rotatedY = point.x * phaseSin + point.y * phaseCos;
        const float ux = rotatedY, uy = -rotatedX;
        const float tx = -uy, ty = ux;
        const float frequency = static_cast<float>(std::min(i, spokes - i)) /
                                (spokes / 2);
        const float position = frequency * static_cast<float>(audio::kBandCount - 1);
        const std::size_t lower = static_cast<std::size_t>(position);
        const std::size_t upper = std::min(lower + 1, audio::kBandCount - 1);
        const float fraction = position - static_cast<float>(lower);
        const float value = std::clamp(frame.bands[lower] * (1.0f - fraction) +
                                      frame.bands[upper] * fraction, 0.0f, 1.0f);
        const float peak = std::clamp(frame.peaks[lower] * (1.0f - fraction) +
                                     frame.peaks[upper] * fraction, 0.0f, 1.0f);
        const float barEnd = baseRadius + 4.0f + std::sqrt(value) * 23.0f;
        const float peakRadius = baseRadius + 5.0f + peak * 23.0f;
        const auto color = haloColor(frequency);
        const auto xAt = [cx, ux, tx](float radius, float offset) {
            return cx + static_cast<int>(std::lround(ux * radius + tx * offset));
        };
        const auto yAt = [cy, uy, ty](float radius, float offset) {
            return cy + static_cast<int>(std::lround(uy * radius + ty * offset));
        };
        line(xAt(baseRadius - 4.0f, 0), yAt(baseRadius - 4.0f, 0),
             xAt(baseRadius - 1.0f, 0), yAt(baseRadius - 1.0f, 0),
             rgb(35, 77, 103));
        if (value > 0.01f) {
            line(xAt(baseRadius, -1), yAt(baseRadius, -1),
                 xAt(barEnd + 2.0f, -1), yAt(barEnd + 2.0f, -1), rgb(28, 73, 112));
            line(xAt(baseRadius, 1), yAt(baseRadius, 1),
                 xAt(barEnd + 2.0f, 1), yAt(barEnd + 2.0f, 1), rgb(28, 73, 112));
            line(xAt(baseRadius, 0), yAt(baseRadius, 0),
                 xAt(barEnd, 0), yAt(barEnd, 0), color);
            pixel(xAt(barEnd, 0), yAt(barEnd, 0), rgb(193, 247, 248));
        }
        if (peak > 0.02f)
            line(xAt(peakRadius, -1.5f), yAt(peakRadius, -1.5f),
                 xAt(peakRadius, 1.5f), yAt(peakRadius, 1.5f),
                 rgb(205, 241, 250));
    }

    // The inside ring shows the actual stereo waveform, rather than a second
    // copy of the spectrum. Scale is inherited from the PC audio processor.
    float maximum = 0.0f;
    for (std::size_t i = 0; i < audio::kWavePoints; ++i)
        maximum = std::max(maximum, std::abs((frame.leftWave[i] + frame.rightWave[i]) * 0.5f));
    const float scale = frame.scopeScale[0] >= 0.0f ? frame.scopeScale[0] * 0.24f :
        frame.hasAudio ? 6.0f / std::max(maximum, 0.01f) : 0.0f;
    int lastX = 0, lastY = 0;
    const auto& wavePoints = unitCircle<audio::kWavePoints>();
    for (std::size_t i = 0; i <= audio::kWavePoints; ++i) {
        const std::size_t sampleIndex = i % audio::kWavePoints;
        const float sample = (frame.leftWave[sampleIndex] + frame.rightWave[sampleIndex]) * 0.5f;
        const float displacement = std::clamp(sample * scale, -7.0f, 7.0f);
        const auto& point = wavePoints[sampleIndex];
        const float rotatedX = point.x * phaseCos - point.y * phaseSin;
        const float rotatedY = point.x * phaseSin + point.y * phaseCos;
        const float ux = rotatedY, uy = -rotatedX;
        const float radius = 31.0f + displacement;
        const int x = cx + static_cast<int>(std::lround(ux * radius));
        const int y = cy + static_cast<int>(std::lround(uy * radius));
        if (i) {
            line(lastX, lastY - 1, x, y - 1, rgb(35, 103, 151));
            line(lastX, lastY, x, y, rgb(146, 225, 246));
        }
        lastX = x;
        lastY = y;
    }
    // A light contour makes the waveform legible without a costly blur pass.
    const float centerEnergy = std::max(haloPulse_, bass * 0.5f);
    ring(20.0f + centerEnergy * 2.0f, rgb(55, 122, 163));
    disc(cx, cy, 4 + static_cast<int>(centerEnergy * 4.0f), rgb(42, 128, 174));
    disc(cx, cy, 2, rgb(193, 247, 248));
    if (!full) {
        // Follow the circular footprint rather than transferring the empty
        // corners of its 145x145 bounding square. Each chord still fits the
        // fixed 320x8 DMA buffers used by the ESP32 backend.
        constexpr int radius = 72;
        constexpr int tileLines = 8;
        for (int y1 = cy - radius; y1 <= cy + radius; y1 += tileLines) {
            const int y2 = std::min(y1 + tileLines - 1, cy + radius);
            const int nearestY = std::clamp(cy, y1, y2);
            const int dy = nearestY - cy;
            const int halfWidth = static_cast<int>(std::sqrt(
                static_cast<float>(radius * radius - dy * dy)));
            invalidate({static_cast<lv_coord_t>(cx - halfWidth), static_cast<lv_coord_t>(y1),
                        static_cast<lv_coord_t>(cx + halfWidth), static_cast<lv_coord_t>(y2)});
        }
    }
}
void VisualizerUi::drawWaterfall(const audio::AudioFrame& frame, bool full) {
    const bool fresh = frame.generation != waterfallLastGeneration_;
    if (fresh) {
        std::move(waterfall_.begin() + 1, waterfall_.end(), waterfall_.begin());
        auto& newest = waterfall_.back();
        for (std::size_t band = 0; band < audio::kBandCount; ++band)
            newest[band] = static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(std::lround(frame.bands[band] * 255.0f)), 0, 255));
        waterfallLastGeneration_ = frame.generation;
    }
    if (!full && !fresh) return;

    constexpr lv_area_t dynamicArea{16, 72, 304, 180};
    clipArea_ = full ? lv_area_t{8, 51, 311, 197} : dynamicArea;
    rect(10, 53, 304, 147, rgb(3, 10, 20));
    rect(8, 51, 304, 147, kColdPanel);
    frameRect(8, 51, 304, 147, kColdGrid);
    rect(16, 72, 288, 108, rgb(3, 12, 21));
    for (int x : {16, 112, 208, 304}) line(x, 72, x, 179, rgb(25, 57, 76));
    for (std::size_t row = 0; row < kWaterfallRows; ++row) {
        const int y = 73 + static_cast<int>(row) * 9;
        for (std::size_t band = 0; band < audio::kBandCount; ++band) {
            const int x = 17 + static_cast<int>(band) * 12;
            rect(x, y, 10, 7, waterfallColor(waterfall_[row][band]));
        }
    }
    line(16, 179, 303, 179, rgb(85, 127, 145));
    if (!full) invalidateTiled(dynamicArea);
}

void VisualizerUi::drawRidgeline(const audio::AudioFrame& frame, bool full) {
    const bool fresh = frame.generation != ridgelineLastGeneration_;
    if (fresh) {
        std::move(ridgeline_.begin() + 1, ridgeline_.end(), ridgeline_.begin());
        auto& newest = ridgeline_.back();
        for (std::size_t band = 0; band < audio::kBandCount; ++band)
            newest[band] = static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(std::lround(frame.bands[band] * 100.0f)), 0, 100));
        ridgelineLastGeneration_ = frame.generation;
    }
    if (!full && !fresh) return;

    const std::array<lv_color_t, kRidgelineRows> fills{
        rgb(25, 45, 75), rgb(24, 57, 92), rgb(22, 71, 104),
        rgb(18, 82, 118), rgb(12, 93, 128)};
    const std::array<lv_color_t, kRidgelineRows> lines{
        rgb(76, 95, 145), rgb(96, 119, 184), rgb(109, 159, 219),
        rgb(105, 196, 221), rgb(128, 224, 236)};
    constexpr lv_area_t dynamicArea{16, 73, 303, 179};
    clipArea_ = full ? lv_area_t{8, 51, 311, 197} : dynamicArea;
    rect(10, 53, 304, 147, rgb(3, 10, 20));
    rect(8, 51, 304, 147, kColdPanel);
    rect(11, 54, 298, 141, rgb(7, 23, 39));
    frameRect(8, 51, 304, 147, kColdGrid);
    for (const int y : {78, 98, 118, 138, 158, 178})
        line(17, y, 303, y, rgb(24, 52, 69));
    for (int x = 17; x <= 303; x += 48)
        line(x, 178, 154 + (x - 160) / 9, 73, rgb(20, 48, 66));

    for (std::size_t row = 0; row < kRidgelineRows; ++row) {
        const int baseline = 106 + static_cast<int>(row) * 18;
        int previousX = 17;
        int previousY = baseline - static_cast<int>(ridgeline_[row][0]) * 25 / 100;
        for (int x = 17; x <= 293; ++x) {
            const int position = x - 17;
            const int lower = std::min(position / 12, 22);
            const int fraction = position - lower * 12;
            const int level = (static_cast<int>(ridgeline_[row][lower]) * (12 - fraction) +
                               static_cast<int>(ridgeline_[row][lower + 1]) * fraction + 6) / 12;
            const int y = baseline - level * 25 / 100;
            rect(x, y, 1, baseline - y + 1, fills[row]);
            line(previousX, previousY, x, y, lines[row]);
            previousX = x;
            previousY = y;
        }
    }
    line(16, 179, 303, 179, rgb(85, 127, 145));
    if (!full) invalidateTiled(dynamicArea);
}

void VisualizerUi::drawTopoBloom(const audio::AudioFrame& frame, bool full) {
    const bool fresh = frame.generation != topoLastGeneration_;
    if (fresh) {
        float bass = 0.0f;
        for (std::size_t i = 0; i < 6; ++i) bass += frame.bands[i];
        bass /= 6.0f;
        topoPhase_ = std::fmod(topoPhase_ + 0.012f + bass * 0.010f, 2.0f * kPi);
        topoLastGeneration_ = frame.generation;
    }
    if (!full && !fresh) return;

    constexpr lv_area_t dynamicArea{8, 62, 305, 192};
    clipArea_ = full ? lv_area_t{8, 51, 311, 197} : dynamicArea;
    rect(8, 51, 304, 147, kColdPanel);
    rect(11, 54, 298, 141, rgb(4, 18, 32));
    frameRect(8, 51, 304, 147, kColdGrid);

    // Sparse ordered grain creates a rich surface without alpha blending.
    const int grainPhase = static_cast<int>(topoPhase_ * 11.0f);
    for (int y = 73; y <= 177; y += 4) {
        for (int x = 18 + ((y / 4) & 1) * 3; x <= 302; x += 6) {
            const int hash = x * 13 + y * 29 + grainPhase;
            if ((hash % 11) == 0) pixel(x, y, rgb(30, 69, 91));
            else if ((hash % 17) == 0) pixel(x, y, rgb(50, 57, 112));
        }
    }

    constexpr int contours = 11;
    constexpr int segments = 84;
    const int cx = 145 + static_cast<int>(std::lround(fastSin(topoPhase_ * 0.43f) * 5.0f));
    const int cy = 127 + static_cast<int>(std::lround(fastCos(topoPhase_ * 0.37f) * 3.0f));
    const auto& contourPoints = unitCircle<segments>();
    for (int contour = contours - 1; contour >= 0; --contour) {
        const float base = 8.0f + contour * 6.4f;
        const float frequency = static_cast<float>(contour) / (contours - 1);
        const auto color = haloColor(frequency);
        int previousX = 0, previousY = 0;
        for (int segment = 0; segment <= segments; ++segment) {
            const float angle = 2.0f * kPi * segment / segments;
            const auto& point = contourPoints[static_cast<std::size_t>(segment) %
                                              contourPoints.size()];
            const float audioPosition = std::fmod(angle / (2.0f * kPi) + 0.13f, 1.0f);
            const float energy = spectrumAt(frame, audioPosition);
            const float warp = fastSin(angle * 3.0f + topoPhase_ + contour * 0.31f) * 2.4f +
                               fastSin(angle * 7.0f - topoPhase_ * 0.63f + contour) * 1.3f +
                               energy * (3.2f + contour * 0.16f);
            const float radius = base + warp;
            const int x = cx + static_cast<int>(std::lround(point.x * radius * 1.82f));
            const int y = cy + static_cast<int>(std::lround(point.y * radius * 0.72f));
            if (segment) {
                line(previousX, previousY + 1, x, y + 1, rgb(20, 45, 70));
                line(previousX, previousY, x, y, color);
            }
            if (((segment + contour * 3) % 13) == 0)
                pixel(x + 1, y, rgb(205, 248, 250));
            previousX = x;
            previousY = y;
        }
    }
    disc(cx, cy, 3, rgb(83, 190, 211));
    disc(cx, cy, 1, rgb(215, 250, 251));
    line(16, 179, 303, 179, rgb(85, 127, 145));
    if (!full) invalidateTiled(dynamicArea);
}

void VisualizerUi::drawAuroraWeave(const audio::AudioFrame& frame, bool full) {
    const bool fresh = frame.generation != auroraLastGeneration_;
    if (fresh) {
        float mid = 0.0f;
        for (std::size_t i = 6; i < 17; ++i) mid += frame.bands[i];
        mid /= 11.0f;
        auroraPhase_ = std::fmod(auroraPhase_ + 0.010f + mid * 0.009f, 2.0f * kPi);
        auroraLastGeneration_ = frame.generation;
    }
    if (!full && !fresh) return;

    constexpr lv_area_t dynamicArea{16, 68, 304, 180};
    clipArea_ = full ? lv_area_t{8, 51, 311, 197} : dynamicArea;
    rect(8, 51, 304, 147, kColdPanel);
    rect(11, 54, 298, 141, rgb(4, 18, 32));
    frameRect(8, 51, 304, 147, kColdGrid);
    for (int y : {82, 106, 130, 154, 178})
        line(17, y, 303, y, rgb(18, 46, 65));

    const std::array<lv_color_t, 5> fills{
        rgb(13, 43, 70), rgb(18, 50, 83), rgb(22, 55, 94),
        rgb(26, 57, 105), rgb(29, 53, 106)};
    const std::array<lv_color_t, 5> edges{
        rgb(70, 132, 183), rgb(73, 171, 205), rgb(99, 221, 229),
        rgb(88, 159, 220), rgb(112, 105, 224)};

    for (int ribbon = 4; ribbon >= 0; --ribbon) {
        const int baseline = 89 + ribbon * 17;
        int previousTop = baseline, previousBottom = baseline, previousCenter = baseline;
        for (int x = 17; x <= 303; x += 2) {
            const float t = static_cast<float>(x - 17) / 286.0f;
            const float energy = spectrumAt(frame, t);
            const float direction = ribbon & 1 ? -1.0f : 1.0f;
            const float wave = fastSin(t * 2.0f * kPi * (1.05f + ribbon * 0.12f) +
                                        direction * auroraPhase_ + ribbon * 0.78f) *
                                   (4.0f + energy * 7.0f) +
                               fastSin(t * 5.0f * kPi - auroraPhase_ * 0.48f + ribbon) * 2.2f;
            const int center = baseline + static_cast<int>(std::lround(wave));
            const int half = 2 + static_cast<int>(std::lround(energy * 3.0f));
            const int top = center - half;
            const int bottom = center + half;
            for (int y = top + 1; y < bottom; ++y) {
                if (((x + y + ribbon * 2) & 3) == 0) pixel(x, y, fills[ribbon]);
                if (((x + y + ribbon) % 7) == 0) pixel(x + 1, y, rgb(42, 91, 132));
            }
            if (x > 17) {
                line(x - 2, previousTop, x, top, edges[ribbon]);
                line(x - 2, previousBottom, x, bottom, rgb(49, 103, 157));
                for (int thread = -2; thread <= 2; ++thread) {
                    const int offset = thread * 2;
                    line(x - 2, previousCenter + offset, x, center + offset,
                         thread == 0 ? edges[ribbon] :
                         std::abs(thread) % 2 ? rgb(50, 114, 160) : rgb(43, 83, 143));
                }
            }
            if (((x / 2 + ribbon * 5) % 17) == 0)
                pixel(x, top - 1, rgb(205, 248, 250));
            previousTop = top;
            previousBottom = bottom;
            previousCenter = center;
        }
    }
    line(16, 179, 303, 179, rgb(85, 127, 145));
    if (!full) invalidateTiled(dynamicArea);
}

void VisualizerUi::drawCymaticMesh(const audio::AudioFrame& frame, bool full) {
    const bool fresh = frame.generation != cymaticLastGeneration_;
    if (fresh) {
        float high = 0.0f;
        for (std::size_t i = 15; i < audio::kBandCount; ++i) high += frame.bands[i];
        high /= static_cast<float>(audio::kBandCount - 15);
        cymaticPhase_ = std::fmod(cymaticPhase_ + 0.008f + high * 0.012f, 2.0f * kPi);
        cymaticLastGeneration_ = frame.generation;
    }
    if (!full && !fresh) return;

    constexpr lv_area_t dynamicArea{11, 64, 311, 188};
    clipArea_ = full ? lv_area_t{8, 51, 311, 197} : dynamicArea;
    rect(8, 51, 304, 147, kColdPanel);
    rect(11, 54, 298, 141, rgb(3, 17, 30));
    frameRect(8, 51, 304, 147, kColdGrid);

    for (int column = 0; column < 20; ++column) {
        const int baseX = 20 + column * 15;
        const float t = static_cast<float>(baseX - 17) / 286.0f;
        const float energy = spectrumAt(frame, t);
        int previousX = baseX, previousY = 72;
        for (int y = 72; y <= 178; y += 3) {
            const float warp = fastSin(y * 0.087f + cymaticPhase_ + column * 0.51f) *
                                   (1.4f + energy * 4.8f) +
                               fastSin((y + baseX) * 0.041f - cymaticPhase_ * 0.61f) * 2.0f;
            const int x = baseX + static_cast<int>(std::lround(warp));
            if (y > 72) line(previousX, previousY, x, y,
                             column % 3 == 0 ? rgb(45, 104, 139) : rgb(28, 67, 96));
            previousX = x;
            previousY = y;
        }
    }
    for (int row = 0; row < 14; ++row) {
        const int baseY = 74 + row * 8;
        int previousX = 17, previousY = baseY;
        for (int x = 17; x <= 303; x += 4) {
            const float t = static_cast<float>(x - 17) / 286.0f;
            const float energy = spectrumAt(frame, t);
            const float warp = fastSin(x * 0.055f - cymaticPhase_ + row * 0.64f) *
                                   (1.5f + energy * 4.2f) +
                               fastSin((x - baseY) * 0.036f + cymaticPhase_ * 0.53f) * 2.0f;
            const int y = baseY + static_cast<int>(std::lround(warp));
            if (x > 17) line(previousX, previousY, x, y,
                             row % 3 == 0 ? rgb(54, 112, 150) : rgb(31, 70, 101));
            previousX = x;
            previousY = y;
        }
    }

    // Stable intersections brighten on transients, making the mesh react
    // without resorting to fast camera motion or noisy particle churn.
    for (int column = 1; column < 20; column += 2) {
        const int baseX = 20 + column * 15;
        const float t = static_cast<float>(baseX - 17) / 286.0f;
        const float energy = spectrumAt(frame, t);
        for (int row = 1; row < 14; row += 2) {
            const int baseY = 74 + row * 8;
            const float pulse = 0.5f + 0.5f * fastSin(column * 0.83f + row * 0.57f +
                                                       cymaticPhase_ * 2.0f);
            if (energy * 0.72f + pulse * 0.28f < 0.24f) continue;
            const int x = baseX + static_cast<int>(std::lround(
                fastSin(baseY * 0.087f + cymaticPhase_ + column * 0.51f) *
                (1.4f + energy * 4.8f)));
            const int y = baseY + static_cast<int>(std::lround(
                fastSin(baseX * 0.055f - cymaticPhase_ + row * 0.64f) *
                (1.5f + energy * 4.2f)));
            disc(x, y, energy > 0.72f ? 2 : 1,
                 energy > 0.55f ? rgb(195, 247, 250) : rgb(86, 190, 211));
        }
    }
    line(16, 179, 303, 179, rgb(85, 127, 145));
    if (!full) invalidateTiled(dynamicArea);
}

void VisualizerUi::resetSpectrumHistory() {
    waterfall_ = {};
    ridgeline_ = {};
    waterfallLastGeneration_ = 0;
    ridgelineLastGeneration_ = 0;
}
void VisualizerUi::resetWaveform() {
    waveformPeak_.fill(0);
    waveformRms_.fill(0);
    waveformSourceValid_ = false;
}
void VisualizerUi::drawWaveformColumn(int x) {
    constexpr int top = 77, bottom = 178, center = 127;
    const auto palette = conceptColors(SpectrumStyle::Waveform);
    const auto column = static_cast<std::size_t>(x - kWaveformLeft);
    rect(x, top, 1, bottom - top + 1, palette.panel);
    if ((x - kWaveformLeft) % 36 == 0) line(x, top, x, bottom, rgb(30, 54, 70));
    for (const int y : {85, 106, 148, 169}) pixel(x, y, rgb(36, 61, 75));
    pixel(x, center, rgb(79, 130, 153));
    const int peak = waveformPeak_[column];
    const int rms = waveformRms_[column];
    if (peak > 0) {
        rect(x, center - peak, 1, peak * 2 + 1, rgb(54, 157, 190));
        rect(x, center - rms, 1, rms * 2 + 1, palette.accent);
        pixel(x, center - peak, palette.peak);
        pixel(x, center + peak, palette.peak);
    }
}
void VisualizerUi::drawWaveform(const audio::AudioFrame& frame, bool full) {
    const bool changed = waveformExternalHistory_ ?
        (!waveformSourceValid_ ||
         waveformLastHistorySequence_ != waveformExternalHistory_->sequence) :
        (!waveformSourceValid_ || waveformLastGeneration_ != frame.generation ||
         waveformLastHasAudio_ != frame.hasAudio ||
         waveformLastActivity_ != frame.visualActivity ||
         waveformLastScale_ != frame.scopeScale ||
         waveformLastLeft_ != frame.leftWave || waveformLastRight_ != frame.rightWave);
    std::array<std::uint8_t, kWaveformColumns> previousPeak{};
    std::array<std::uint8_t, kWaveformColumns> previousRms{};
    if (changed) {
        previousPeak = waveformPeak_;
        previousRms = waveformRms_;
        if (waveformExternalHistory_) {
            static_assert(kWaveformColumns == audio::kWaveformHistoryColumns,
                          "Desktop history must match the waveform plot width");
            waveformPeak_ = waveformExternalHistory_->peak;
            waveformRms_ = waveformExternalHistory_->rms;
            waveformLastHistorySequence_ = waveformExternalHistory_->sequence;
        } else {
            float peak = 0.0f, sumSquares = 0.0f;
            const float leftScale = frame.scopeScale[0] >= 0.0f ? frame.scopeScale[0] : 48.0f;
            const float rightScale = frame.scopeScale[1] >= 0.0f ? frame.scopeScale[1] : 48.0f;
            for (std::size_t i = 0; i < audio::kWavePoints; ++i) {
                // Apply each channel's adaptive gain before combining them.
                // Using the larger gain for both channels makes a loud channel
                // fill the plot whenever the quiet channel has a high gain.
                const float sample = std::max(std::abs(frame.leftWave[i]) * leftScale,
                                              std::abs(frame.rightWave[i]) * rightScale);
                peak = std::max(peak, sample);
                sumSquares += sample * sample;
            }
            const float rms = std::sqrt(sumSquares / static_cast<float>(audio::kWavePoints));
            const float activity = !frame.hasAudio ? 0.0f :
                frame.scopeScale[0] >= 0.0f || frame.scopeScale[1] >= 0.0f ?
                1.0f : std::clamp(frame.visualActivity, 0.0f, 1.0f);
            const int peakPixels = std::clamp(static_cast<int>(std::round(peak * activity)), 0, 48);
            const int rmsPixels = std::clamp(static_cast<int>(std::round(rms * activity)), 0, peakPixels);
            std::move(waveformPeak_.begin() + 1, waveformPeak_.end(), waveformPeak_.begin());
            std::move(waveformRms_.begin() + 1, waveformRms_.end(), waveformRms_.begin());
            waveformPeak_.back() = static_cast<std::uint8_t>(peakPixels);
            waveformRms_.back() = static_cast<std::uint8_t>(rmsPixels);
            waveformLastLeft_ = frame.leftWave;
            waveformLastRight_ = frame.rightWave;
            waveformLastScale_ = frame.scopeScale;
            waveformLastActivity_ = frame.visualActivity;
            waveformLastGeneration_ = frame.generation;
            waveformLastHasAudio_ = frame.hasAudio;
        }
        waveformSourceValid_ = true;
    }
    if (full) {
        clipArea_ = {8, 51, 311, 197};
        rect(10, 53, 304, 147, rgb(6, 16, 25));
        rect(8, 51, 304, 147, kColdPanel);
        frameRect(8, 51, 304, 147, kColdGrid);
        line(16, 74, 303, 74, rgb(44, 70, 84));
        for (int x = kWaveformLeft; x <= kWaveformRight; ++x) drawWaveformColumn(x);
        line(16, 179, 303, 179, rgb(85, 127, 145));
    } else if (changed) {
        // A horizontal scroll changes many columns; invalidate changed tiles.
        constexpr std::size_t tileWidth = 24;
        for (std::size_t tile = 0; tile < kWaveformColumns; tile += tileWidth) {
            const std::size_t end = std::min(tile + tileWidth, kWaveformColumns);
            int first = -1, last = -1, extent = 0;
            for (std::size_t i = tile; i < end; ++i) {
                if (previousPeak[i] == waveformPeak_[i] &&
                    previousRms[i] == waveformRms_[i]) continue;
                if (first < 0) first = static_cast<int>(i);
                last = static_cast<int>(i);
                extent = std::max({extent, static_cast<int>(previousPeak[i]),
                                   static_cast<int>(waveformPeak_[i])});
            }
            if (first < 0) continue;
            clipArea_ = {static_cast<lv_coord_t>(kWaveformLeft + first), 77,
                         static_cast<lv_coord_t>(kWaveformLeft + last), 178};
            for (int i = first; i <= last; ++i)
                if (previousPeak[static_cast<std::size_t>(i)] != waveformPeak_[static_cast<std::size_t>(i)] ||
                    previousRms[static_cast<std::size_t>(i)] != waveformRms_[static_cast<std::size_t>(i)])
                    drawWaveformColumn(kWaveformLeft + i);
            invalidate({static_cast<lv_coord_t>(kWaveformLeft + first),
                        static_cast<lv_coord_t>(127 - extent),
                        static_cast<lv_coord_t>(kWaveformLeft + last),
                        static_cast<lv_coord_t>(127 + extent)});
        }
    }
}
void VisualizerUi::drawScopeBase(int channel) {
    const int top = channel == 0 ? 51 : 127;
    rect(10, top + 2, 304, 69, rgb(3, 10, 20));
    rect(8, top, 304, 69, kColdPanel);
    frameRect(8, top, 304, 69, kColdGrid);
    for (int x = 48; x < 309; x += 32)
        line(x, top + 7, x, top + 62, kColdGrid);
    for (int y = top + 17; y < top + 68; y += 17)
        line(12, y, 307, y, kColdGrid);
    const int center = channel == 0 ? 90 : 166;
    line(12, center, 307, center, rgb(62, 102, 129));
}
void VisualizerUi::drawScopeTrace(int channel) {
    const auto& ys = waveY_[channel];
    const auto glow = channel == 0 ? rgb(35, 100, 125) : rgb(44, 76, 134);
    const auto ink = channel == 0 ? kCyan : rgb(142, 177, 238);
    for (std::size_t i = 1; i < audio::kWavePoints; ++i) {
        const int x0 = 13 + static_cast<int>((i - 1) * 292 / (audio::kWavePoints - 1));
        const int x1 = 13 + static_cast<int>(i * 292 / (audio::kWavePoints - 1));
        line(x0, ys[i - 1] + 1, x1, ys[i] + 1, glow);
        line(x0, ys[i - 1], x1, ys[i], ink);
    }
}
void VisualizerUi::drawScope(const audio::AudioFrame& frame, bool full) {
    if (full) {
        drawScopeBase(0);
        drawScopeBase(1);
    }
    for (int channel = 0; channel < 2; ++channel) {
        const auto& wave = channel == 0 ? frame.leftWave : frame.rightWave;
        float maximum = 0.0f;
        for (float sample : wave) maximum = std::max(maximum, std::abs(sample));
        // Live input supplies a peak-envelope gain; synthetic frames derive a
        // stateless visual gain.
        const float scale = frame.scopeScale[channel] >= 0.0f ? frame.scopeScale[channel] :
            frame.hasAudio ? std::min(3072.0f, 26.0f / std::max(maximum, 0.002f)) *
                             std::clamp(frame.visualActivity, 0.0f, 1.0f) : 0.0f;
        const int center = channel == 0 ? 90 : 166;
        std::array<std::int16_t, audio::kWavePoints> next;
        for (std::size_t i = 0; i < next.size(); ++i)
            next[i] = static_cast<std::int16_t>(center - static_cast<int>(wave[i] * scale));
        if (!full && next == waveY_[channel]) continue;
        if (!full) {
            const auto oldRange = std::minmax_element(waveY_[channel].begin(), waveY_[channel].end());
            const auto newRange = std::minmax_element(next.begin(), next.end());
            clipArea_ = {13, static_cast<lv_coord_t>(std::min(*oldRange.first, *newRange.first)),
                         305, static_cast<lv_coord_t>(std::max(*oldRange.second, *newRange.second) + 1)};
            drawScopeBase(channel);
        }
        waveY_[channel] = next;
        drawScopeTrace(channel);
        if (!full) {
            if (directCanvasUpdates_ && mode_ == ModeId::Spectrum &&
                spectrumStyle_ == SpectrumStyle::Oscilloscope)
                invalidateTiled(clipArea_);
            else
                invalidate(clipArea_);
        }
    }
}
void VisualizerUi::render(const audio::AudioFrame& frame,
                          const audio::WaveformHistory* waveformHistory,
                          const data::UtilityFrame* utilityFrame) {
    if (!ready()) return;
    static const data::UtilityFrame emptyUtility;
    const data::UtilityFrame& utility = utilityFrame ? *utilityFrame : emptyUtility;
    waveformExternalHistory_ = waveformHistory;
    directDirtyCount_ = 0;
    const bool full = modeChanged_;
    clipArea_ = {0, 0, kWidth - 1, kHeight - 1};
    if (full) {
        if (!shellDrawn_) drawShell();
        else rect(0, 44, kWidth, 157, rgb(9, 20, 31));
    }
    switch (mode_) {
    case ModeId::Vu: drawVu(frame, full); break;
    case ModeId::Spectrum: drawSpectrum(frame, full); break;
    case ModeId::LegacyScope: drawScope(frame, full); break;
    case ModeId::Market:
        if (full || utility.marketGeneration != marketLastGeneration_)
            drawMarket(utility, full);
        break;
    case ModeId::Clock: drawClock(frame, utility, full); break;
    case ModeId::NowPlaying: drawNowPlaying(frame, utility, full); break;
    case ModeId::DeskBoard: drawDeskBoard(frame, utility, full); break;
    }
    if (full) {
        // Repaint button corners against the underlying footer after a palette
        // switch; partial footer invalidation leaves old antialias colors.
        const lv_area_t changed{0, 0, kWidth - 1, kHeight - 1};
        invalidate(changed);
    }
    shellDrawn_ = true;
    modeChanged_ = false;
    utilityLastGeneration_ = utility.generation;
    marketLastGeneration_ = utility.marketGeneration;
    waveformExternalHistory_ = nullptr;
}
} // namespace ui
