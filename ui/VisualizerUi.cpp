#include "ui/VisualizerUi.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace ui {
namespace {
constexpr float kPi = 3.14159265358979323846f;
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
    return style >= VisualizerUi::SpectrumStyle::NeonPulse &&
           style < VisualizerUi::SpectrumStyle::Count;
}
const char* spectrumStyleName(VisualizerUi::SpectrumStyle style) {
    switch (style) {
    case VisualizerUi::SpectrumStyle::Bars: return "BARS";
    case VisualizerUi::SpectrumStyle::Trace: return "TRACE";
    case VisualizerUi::SpectrumStyle::Dots: return "DOTS";
    case VisualizerUi::SpectrumStyle::NeonPulse: return "PULSE";
    case VisualizerUi::SpectrumStyle::Prism: return "PRISM";
    case VisualizerUi::SpectrumStyle::Mirror: return "MIRROR";
    case VisualizerUi::SpectrumStyle::Waveform: return "WAVEFORM";
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
    case Style::Prism: return {kColdPanel, kColdGrid, rgb(112, 210, 235),
                              rgb(151, 127, 234), rgb(193, 247, 248)};
    case Style::Mirror: return {kColdPanel, kColdGrid, kCyan,
                               kViolet, rgb(193, 247, 248)};
    case Style::Waveform: return {kColdPanel, kColdGrid, kCyan,
                                 rgb(142, 177, 238), rgb(193, 247, 248)};
    default: return {kColdPanel, kColdGrid, kCyan,
                     kViolet, rgb(193, 247, 248)};
    }
}
} // namespace

VisualizerUi::VisualizerUi()
    : ownedBuffer_(new (std::nothrow) lv_color_t[kCanvasPixels]),
      buffer_(ownedBuffer_.get()) {
    if (buffer_) initialize();
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
    std::fill_n(buffer_, kCanvasPixels, lv_color_t{});
    lv_obj_t* screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, kBackground, 0);
    canvas_ = lv_canvas_create(screen);
    lv_canvas_set_buffer(canvas_, buffer_, kWidth, kHeight, LV_IMG_CF_TRUE_COLOR);
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
        setSpectrumStyle(static_cast<SpectrumStyle>(
            (static_cast<int>(spectrumStyle_) + 1) % static_cast<int>(SpectrumStyle::Count)));
    }
}
void VisualizerUi::setTouchActionHandler(TouchActionHandler handler, void* context) {
    touchActionHandler_ = handler;
    touchActionContext_ = context;
}
void VisualizerUi::nextMode() {
    if (!ready()) return;
    modeChanged_ = true;
    mode_ = static_cast<Mode>((static_cast<int>(mode_) + 1) % 3);
    if (mode_ == Mode::Spectrum && spectrumStyle_ == SpectrumStyle::Waveform)
        resetWaveform();
    updateLabels();
}
void VisualizerUi::setSpectrumStyle(SpectrumStyle style) {
    if (!ready() || style == spectrumStyle_ || style >= SpectrumStyle::Count) return;
    spectrumStyle_ = style;
    modeChanged_ = true;
    if (style == SpectrumStyle::Waveform) resetWaveform();
    if (mode_ == Mode::Spectrum) updateLabels();
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
    lv_obj_add_flag(styleButton_, LV_OBJ_FLAG_HIDDEN);
    for (auto* item : annotations_) lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
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
    if (mode_ == Mode::Vu) {
        lv_obj_set_style_text_color(title_, rgb(241, 238, 224), 0);
        lv_obj_set_style_text_color(modeLabel_, rgb(174, 185, 184), 0);
        lv_obj_set_style_text_color(pageLabel_, rgb(215, 171, 105), 0);
        lv_obj_set_style_text_color(status_, rgb(171, 185, 181), 0);
        lv_obj_set_style_bg_color(button_, rgb(194, 151, 87), 0);
        lv_obj_set_style_bg_color(button_, rgb(173, 229, 238), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(leftValue_, rgb(72, 53, 35), 0);
        lv_obj_set_style_text_color(rightValue_, rgb(72, 53, 35), 0);
        lv_label_set_text(modeLabel_, "ANALOG VU  /  STEREO");
        lv_label_set_text(pageLabel_, "01 / 03");
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
    } else if (mode_ == Mode::Spectrum) {
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
        } else {
            const auto ink = rgb(142, 169, 182);
            annotation(0, 16, 181, "SUB", ink);
            annotation(1, 128, 181, "1K", ink);
            annotation(2, 207, 181, "5K", ink);
            annotation(3, 249, 181, "10K", ink);
            annotation(4, 281, 181, "20K", ink);
        }
    } else {
        lv_label_set_text(title_, "STUDIO MONITOR");
        lv_obj_set_style_text_color(title_, kText, 0);
        lv_obj_set_style_text_color(pageLabel_, rgb(126, 199, 218), 0);
        lv_obj_set_style_bg_color(button_, kCyan, 0);
        lv_label_set_text(modeLabel_, "OSCILLOSCOPE  /  STEREO");
        lv_label_set_text(pageLabel_, "03 / 03");
        annotation(0, 18, 55, "LEFT  /  CH 1", kCyan);
        annotation(1, 18, 131, "RIGHT  /  CH 2", rgb(142, 177, 238));
    }
}

void VisualizerUi::pixel(int x, int y, lv_color_t color) {
    if (x >= clipArea_.x1 && x <= clipArea_.x2 &&
        y >= clipArea_.y1 && y <= clipArea_.y2 &&
        x >= 0 && x < kWidth && y >= 0 && y < kHeight) {
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
    for (int row = top; row < bottom; ++row)
        std::fill(buffer_ + static_cast<std::size_t>(row) * kWidth + left,
                  buffer_ + static_cast<std::size_t>(row) * kWidth + right, color);
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
    std::fill_n(buffer_, kCanvasPixels, color);
}
void VisualizerUi::invalidate(const lv_area_t& area) {
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

void VisualizerUi::drawPrismShard(std::size_t band) {
    // Each frequency gets a faceted crystal with a pointed top and tapered
    // base. Two colored faces make this legible even on the RGB332 panel.
    const auto palette = conceptColors(SpectrumStyle::Prism);
    const int cx = 21 + static_cast<int>(band) * 12;
    const int height = barHeights_[band] * 96 / 100;
    for (int row = 0; row < height; ++row) {
        const int third = std::max(1, height / 3);
        const int tail = std::max(1, height / 4);
        const int halfWidth = row < third ? 1 + row * 3 / third :
            row < height - tail ? 4 : 4 - (row - (height - tail)) * 2 / tail;
        const int y = 178 - height + row;
        rect(cx - halfWidth, y, halfWidth, 1, palette.accent);
        rect(cx, y, halfWidth + 1, 1, palette.secondary);
        pixel(cx, y, rgb(204, 238, 249));
    }
    const int peakY = 174 - peakHeights_[band] * 96 / 100;
    pixel(cx, peakY - 1, palette.peak);
    line(cx - 1, peakY, cx + 1, peakY, palette.peak);
    pixel(cx, peakY + 1, palette.peak);
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
    else if (style == SpectrumStyle::Prism) {
        for (std::size_t i = 0; i < audio::kBandCount; ++i) drawPrismShard(i);
    } else {
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
            if (style == SpectrumStyle::Prism) {
                const int top = std::max(77, std::min({178 - previousHeights[i] * 96 / 100,
                    178 - barHeights_[i] * 96 / 100,
                    173 - previousPeaks[i] * 96 / 100,
                    173 - peakHeights_[i] * 96 / 100}));
                invalidate({static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(top),
                            static_cast<lv_coord_t>(x + 8), 178});
            } else {
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
            for (std::size_t i = 0; i < audio::kWavePoints; ++i) {
                const float sample = std::max(std::abs(frame.leftWave[i]),
                                              std::abs(frame.rightWave[i]));
                peak = std::max(peak, sample);
                sumSquares += sample * sample;
            }
            const float rms = std::sqrt(sumSquares / static_cast<float>(audio::kWavePoints));
            const float scale = frame.scopeScale[0] >= 0.0f || frame.scopeScale[1] >= 0.0f ?
                std::max(frame.scopeScale[0], frame.scopeScale[1]) : 48.0f;
            const float activity = !frame.hasAudio ? 0.0f :
                frame.scopeScale[0] >= 0.0f || frame.scopeScale[1] >= 0.0f ?
                1.0f : std::clamp(frame.visualActivity, 0.0f, 1.0f);
            const int peakPixels = std::clamp(static_cast<int>(std::round(peak * scale * activity)), 0, 48);
            const int rmsPixels = std::clamp(static_cast<int>(std::round(rms * scale * activity)), 0, peakPixels);
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
        if (!full) invalidate(clipArea_);
    }
}
void VisualizerUi::render(const audio::AudioFrame& frame,
                          const audio::WaveformHistory* waveformHistory) {
    if (!ready()) return;
    waveformExternalHistory_ = waveformHistory;
    const bool full = modeChanged_;
    clipArea_ = {0, 0, kWidth - 1, kHeight - 1};
    if (full) {
        if (!shellDrawn_) drawShell();
        else rect(0, 44, kWidth, 157, rgb(9, 20, 31));
    }
    switch (mode_) {
    case Mode::Vu: drawVu(frame, full); break;
    case Mode::Spectrum: drawSpectrum(frame, full); break;
    case Mode::Scope: drawScope(frame, full); break;
    }
    if (full) {
        // Repaint button corners against the underlying footer after a palette
        // switch; partial footer invalidation leaves old antialias colors.
        const lv_area_t changed{0, 0, kWidth - 1, kHeight - 1};
        invalidate(changed);
    }
    shellDrawn_ = true;
    modeChanged_ = false;
    waveformExternalHistory_ = nullptr;
}
} // namespace ui
