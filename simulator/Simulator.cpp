#include "simulator/Simulator.hpp"
#include <algorithm>

namespace simulator {

Simulator::Simulator() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        error_ = SDL_GetError();
        return;
    }
    window_ = SDL_CreateWindow("AudioViz PC", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, kWidth, kHeight,
                               SDL_WINDOW_SHOWN);
    if (!window_) {
        error_ = SDL_GetError();
        return;
    }
    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_) {
        error_ = SDL_GetError();
        return;
    }
    SDL_RenderSetLogicalSize(renderer_, kWidth, kHeight);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
    if (!texture_) {
        error_ = SDL_GetError();
        return;
    }

    lv_init();
    lv_disp_draw_buf_init(&drawBuffer_, drawA_.data(), drawB_.data(), drawA_.size());
    lv_disp_drv_init(&displayDriver_);
    displayDriver_.hor_res = kWidth;
    displayDriver_.ver_res = kHeight;
    displayDriver_.flush_cb = &Simulator::flush;
    displayDriver_.draw_buf = &drawBuffer_;
    displayDriver_.user_data = this;
    lv_disp_drv_register(&displayDriver_);
    lv_indev_drv_init(&pointerDriver_);
    pointerDriver_.type = LV_INDEV_TYPE_POINTER;
    pointerDriver_.read_cb = &Simulator::readPointer;
    pointerDriver_.user_data = this;
    lv_indev_drv_register(&pointerDriver_);
    ready_ = true;
}

Simulator::~Simulator() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool Simulator::pollEvent(SDL_Event& event) const { return SDL_PollEvent(&event) != 0; }

void Simulator::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        pointer_.record(event.motion.x, event.motion.y, pointer_.latest().pressed);
    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            pointer_.record(event.button.x, event.button.y, event.type == SDL_MOUSEBUTTONDOWN);
        }
    }
}

void Simulator::tick(std::uint32_t milliseconds) { lv_tick_inc(milliseconds); }

void Simulator::readPointer(lv_indev_drv_t* driver, lv_indev_data_t* data) {
    auto* self = static_cast<Simulator*>(driver->user_data);
    const auto sample = self->pointer_.read();
    data->point.x = static_cast<lv_coord_t>(std::clamp(sample.x, 0, kWidth - 1));
    data->point.y = static_cast<lv_coord_t>(std::clamp(sample.y, 0, kHeight - 1));
    data->state = sample.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = self->pointer_.hasPending();
}

void Simulator::flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors) {
    auto* self = static_cast<Simulator*>(driver->user_data);
    self->copyPixels(area, colors);
    lv_disp_flush_ready(driver);
}

void Simulator::copyPixels(const lv_area_t* area, const lv_color_t* colors) {
    const int width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y) {
        for (int x = area->x1; x <= area->x2; ++x) {
            const lv_color_t color = colors[(y - area->y1) * width + x - area->x1];
            if (x >= 0 && x < kWidth && y >= 0 && y < kHeight)
                pixels_[y * kWidth + x] = 0xff000000u | lv_color_to32(color);
        }
    }
}

void Simulator::present() {
    SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kWidth * sizeof(std::uint32_t));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

bool Simulator::saveScreenshot(const char* path) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels_.data(), kWidth, kHeight, 32, kWidth * sizeof(std::uint32_t),
        SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return false;
    const bool saved = SDL_SaveBMP(surface, path) == 0;
    SDL_FreeSurface(surface);
    return saved;
}

} // namespace simulator
