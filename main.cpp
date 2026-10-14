#define SDL_MAIN_HANDLED
#include "app/DemoSignal.hpp"
#include "app/Options.hpp"
#include "audio/AudioCapture.hpp"
#include "audio/AudioProcessor.hpp"
#include "simulator/Simulator.hpp"
#include "ui/VisualizerUi.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
const char* liveStatus() {
#ifdef _WIN32
    return "WASAPI LOOPBACK";
#elif defined(__APPLE__)
    return "COREAUDIO INPUT";
#else
    return "PULSE MONITOR";
#endif
}
}

int main(int argc, char** argv) {
    app::Options options;
    std::string optionError;
    if (!app::parseOptions(argc, argv, options, optionError)) {
        std::cerr << optionError << "\nUse --help for usage.\n";
        return 2;
    }
    if (options.help) {
        std::cout << "AudioViz_PC [--demo] [--mode 1|2|3] [--frames N]"
                     " [--screenshot FILE.bmp]"
                     " [--audio-device NAME] [--list-audio-devices]\n"
                     "N is nonnegative; 0 runs until the window is closed.\n"
                     "Screenshots are saved on exit.\n";
        return 0;
    }
    if (options.listAudioDevices) {
        std::vector<std::string> devices;
        std::string error;
        if (!audio::AudioCapture::listDevices(devices, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        for (const auto& device : devices) std::cout << device << '\n';
        return 0;
    }

    simulator::Simulator simulator;
    if (!simulator.ready()) {
        std::cerr << "Display initialization failed: " << simulator.error() << '\n';
        return 1;
    }

    audio::AudioProcessor processor;
    audio::AudioCapture capture(processor);
    ui::VisualizerUi interface;
    if (!interface.ready()) {
        std::cerr << "Canvas allocation failed\n";
        return 1;
    }
    for (int mode = 1; mode < options.initialMode; ++mode) interface.nextMode();
    bool captureStarted = false;
    bool captureRunning = false;
    if (options.demo) {
        interface.setStatus("DEMO SIGNAL");
    } else {
        std::string error;
        if (!capture.start(error, options.audioDevice)) {
            std::cerr << error << '\n';
            interface.setStatus("AUDIO UNAVAILABLE  (try --demo)");
        } else {
            captureStarted = captureRunning = true;
            interface.setStatus(liveStatus());
        }
    }

    bool running = true;
    app::DemoSignal demoSignal;
    int renderedFrames = 0;
    std::uint64_t previous = SDL_GetTicks64();
    while (running) {
        const std::uint64_t frameStart = SDL_GetTicks64();
        const auto elapsed = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            frameStart - previous, 100));
        previous = frameStart;
        SDL_Event event;
        while (simulator.pollEvent(event)) {
            simulator.handleEvent(event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && !event.key.repeat &&
                event.key.keysym.sym == SDLK_SPACE) interface.nextMode();
        }
        if (options.demo) demoSignal.generate(processor, elapsed);
        else if (captureStarted) {
            capture.update();
            const bool nowRunning = capture.running();
            if (nowRunning != captureRunning) {
                captureRunning = nowRunning;
                interface.setStatus(captureRunning ? liveStatus() : "AUDIO STOPPED  (try restarting)");
                if (!captureRunning) std::cerr << "Loopback capture stopped; reconnect the playback device or restart.\n";
            }
        }
        simulator.tick(elapsed);
        interface.render(processor.snapshot());
        lv_timer_handler();
        simulator.present();
        if (options.frameLimit > 0 && ++renderedFrames >= options.frameLimit) {
            if (options.demo) {
                const auto finalFrame = processor.snapshot();
                std::cout << "Demo level: L " << finalFrame.leftDb << " dBFS, R "
                          << finalFrame.rightDb << " dBFS\n";
            }
            running = false;
        }
        const std::uint64_t spent = SDL_GetTicks64() - frameStart;
        if (spent < 16) SDL_Delay(static_cast<std::uint32_t>(16 - spent));
    }
    capture.stop();
    if (!options.screenshot.empty()) {
        // The last requested frame may fall between LVGL refresh timer ticks.
        lv_refr_now(nullptr);
        if (!simulator.saveScreenshot(options.screenshot.c_str())) {
            std::cerr << "Screenshot failed: " << SDL_GetError() << '\n';
            return 1;
        }
    }
    return 0;
}
