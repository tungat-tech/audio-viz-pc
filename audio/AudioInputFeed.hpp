#pragma once

#include "audio/AudioProcessor.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace audio {

// WASAPI loopback can stop delivering packets when no application is playing.
// Serialize real input and UI-driven silence so DSP keeps decaying without
// allowing two producer threads to enter AudioProcessor concurrently.
class AudioInputFeed {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit AudioInputFeed(AudioProcessor& processor) : processor_(processor) {}

    void start(TimePoint now = Clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = true;
        lastInput_ = lastPump_ = now;
        remainder_ = 0;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
    }

    void process(const float* input, std::uint32_t frames, TimePoint now = Clock::now()) {
        if (!input || frames == 0) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        processor_.processInterleaved(input, frames);
        lastInput_ = lastPump_ = now;
        remainder_ = 0;
    }

    std::uint32_t pumpSilence(TimePoint now = Clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Allow normal callback scheduling jitter before declaring input idle.
        if (!active_ || now - lastInput_ < std::chrono::milliseconds(100) || now <= lastPump_)
            return 0;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastPump_).count();
        // Bound work after a long UI pause; don't replay minutes of silence.
        const auto bounded = std::min<std::int64_t>(elapsed, 250000000);
        const auto scaled = static_cast<std::uint64_t>(bounded) * 48000 + remainder_;
        const auto frames = static_cast<std::uint32_t>(scaled / 1000000000);
        remainder_ = scaled % 1000000000;
        lastPump_ = now;
        std::array<float, kFftSize * 2> silence{};
        for (std::uint32_t remaining = frames; remaining > 0;) {
            const auto count = std::min<std::uint32_t>(remaining, kFftSize);
            processor_.processInterleaved(silence.data(), count);
            remaining -= count;
        }
        return frames;
    }

private:
    AudioProcessor& processor_;
    std::mutex mutex_;
    bool active_ = false;
    TimePoint lastInput_{};
    TimePoint lastPump_{};
    std::uint64_t remainder_ = 0;
};

} // namespace audio
