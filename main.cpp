#include "app/DemoSignal.hpp"
#include "app/Options.hpp"
#include "audio/AudioCapture.hpp"
#include "audio/AudioProcessor.hpp"
#include "data/UtilityDataProvider.hpp"
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
        std::cout << "AudioViz_PC [--demo] [--mode 1..5] [--style 0..12] [--frames N]"
                     " [--screenshot FILE.bmp]"
                     " [--audio-device NAME] [--weather-city CITY] [--list-audio-devices]\n"
                     "Modes: 1 Now Playing (default), 2 Spectrum, 3 Studio Clock,\n"
                     "       4 Market Pulse, 5 Desk Board. Oscilloscope is a Spectrum style.\n"
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
    // Demo/screenshot runs remain deterministic and never contact the network.
    data::UtilityDataProvider utilityData(!options.demo, options.weatherCity);
    ui::VisualizerUi interface;
    if (!interface.ready()) {
        std::cerr << "Canvas allocation failed\n";
        return 1;
    }
    interface.setMode(static_cast<unsigned>(options.initialMode));
    interface.setSpectrumStyle(static_cast<ui::VisualizerUi::SpectrumStyle>(options.initialStyle));
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
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                if (event.key.keysym.sym == SDLK_SPACE) interface.nextMode();
                else if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_5)
                    interface.setMode(static_cast<unsigned>(event.key.keysym.sym - SDLK_0));
            }
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
        const auto audioFrame = processor.snapshot();
        utilityData.update(elapsed, audioFrame);
        const auto waveformHistory = processor.waveformHistory();
        interface.render(audioFrame, &waveformHistory, &utilityData.snapshot());
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
