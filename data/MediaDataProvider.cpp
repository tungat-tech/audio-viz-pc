#include "data/MediaDataProvider.hpp"
#include "data/MediaSessionPlatform.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace data {

struct MediaDataProvider::State {
    explicit State(bool enabled) : enabled(enabled) {}
    bool enabled = true;
    bool stop = false;
    bool changed = false;
    bool live = false;
    MediaDataSnapshot latest;
    std::string status = "LOADING";
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
};

MediaDataProvider::MediaDataProvider(bool enabled)
    : state_(std::make_unique<State>(enabled)) {
    if (!enabled) {
        state_->status = "DEMO";
        return;
    }
    if (!platformMediaSessionSupported()) {
        state_->status = "UNAVAILABLE";
        state_->changed = true;
        return;
    }
    State* state = state_.get();
    state_->worker = std::thread([state] {
        for (;;) {
            const auto cycleStarted = std::chrono::steady_clock::now();
            MediaDataSnapshot next;
            std::string error;
            const bool success = readPlatformMediaSession(next, error);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->stop) break;
                if (success) state->latest = std::move(next);
                state->live = success && state->latest.active;
                state->status = !success ? "STALE" :
                    state->latest.active ? "LIVE" : "NO MEDIA";
                state->changed = true;
            }
            std::unique_lock<std::mutex> lock(state->mutex);
            if (state->wake.wait_until(lock,
                    cycleStarted + std::chrono::milliseconds(kMediaRefreshIntervalMs),
                    [state] { return state->stop; })) break;
        }
    });
}

MediaDataProvider::~MediaDataProvider() {
    if (!state_ || !state_->worker.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->stop = true;
    }
    state_->wake.notify_all();
    state_->worker.join();
}

bool MediaDataProvider::takeLatest(MediaDataSnapshot& snapshot, bool& live,
                                   std::string& status) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->changed) return false;
    snapshot = state_->latest;
    live = state_->live;
    status = state_->status;
    state_->changed = false;
    return true;
}

} // namespace data
