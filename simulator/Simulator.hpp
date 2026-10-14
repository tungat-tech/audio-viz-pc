#pragma once

#include "simulator/PointerInput.hpp"
#include <lvgl.h>
#include <SDL.h>
#include <array>
#include <cstdint>

namespace simulator {

constexpr int kWidth = 320;
constexpr int kHeight = 240;

class Simulator {
public:
    Simulator();
    ~Simulator();
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    bool ready() const { return ready_; }
    const char* error() const { return error_; }
    bool pollEvent(SDL_Event& event) const;
    void handleEvent(const SDL_Event& event);
    void tick(std::uint32_t milliseconds);
    void present();
    bool saveScreenshot(const char* path);

private:
    static void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors);
    static void readPointer(lv_indev_drv_t* driver, lv_indev_data_t* data);
    void copyPixels(const lv_area_t* area, const lv_color_t* colors);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    lv_disp_draw_buf_t drawBuffer_{};
    lv_disp_drv_t displayDriver_{};
    lv_indev_drv_t pointerDriver_{};
    std::array<lv_color_t, kWidth * 24> drawA_{};
    std::array<lv_color_t, kWidth * 24> drawB_{};
    std::array<std::uint32_t, kWidth * kHeight> pixels_{};
    bool ready_ = false;
    PointerInput pointer_;
    const char* error_ = "";
};

} // namespace simulator
