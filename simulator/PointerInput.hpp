#pragma once

#include <deque>

namespace simulator {

struct PointerSample {
    int x = 0;
    int y = 0;
    bool pressed = false;
};

// Preserve button edges even when SDL delivers down and up in one frame.
class PointerInput {
public:
    void record(int x, int y, bool pressed) {
        if (x == latest_.x && y == latest_.y && pressed == latest_.pressed) return;
        latest_ = {x, y, pressed};
        pending_.push_back(latest_);
    }

    PointerSample read() {
        if (pending_.empty()) return latest_;
        const auto sample = pending_.front();
        pending_.pop_front();
        return sample;
    }

    const PointerSample& latest() const { return latest_; }
    bool hasPending() const { return !pending_.empty(); }

private:
    PointerSample latest_{};
    std::deque<PointerSample> pending_;
};

} // namespace simulator
