#include "audio/WaveformHistory.hpp"
#include <algorithm>
#include <cassert>

int main() {
    audio::WaveformHistoryBuffer history;
    audio::AudioFrame frame;
    frame.hasAudio = true;
    frame.scopeScale = {100.0f, 100.0f};
    frame.leftWave.fill(0.05f);
    frame.rightWave.fill(-0.05f);
    history.append(frame, 48000.0f);
    assert(history.snapshot().sequence == 0);
    history.append(frame, 48000.0f);
    assert(history.snapshot().sequence == 1);
    assert(history.snapshot().peak.back() == 5);
    for (int block = 0; block < 100; ++block) {
        frame.leftWave.fill(block == 40 ? 0.5f : 0.05f);
        history.append(frame, 48000.0f);
    }
    const auto afterDrag = history.snapshot();
    assert(afterDrag.sequence == 65);
    assert(*std::max_element(afterDrag.peak.begin(), afterDrag.peak.end()) == 48);
    frame.hasAudio = false;
    for (int block = 0; block < 500; ++block) history.append(frame, 48000.0f);
    const auto quiet = history.snapshot();
    assert(quiet.sequence > afterDrag.sequence + audio::kWaveformHistoryColumns);
    assert(*std::max_element(quiet.peak.begin(), quiet.peak.end()) == 0);
}
