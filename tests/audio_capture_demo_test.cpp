#include "audio/AudioCapture.hpp"
#include <iostream>
#include <string>
int main() {
    audio::AudioProcessor processor;
    audio::AudioCapture capture(processor);
    for (int i = 0; i < 3; ++i) {
        std::string error;
        if (capture.start(error) || capture.running() ||
            error.find("--demo") == std::string::npos) return 1;
        capture.update();
        capture.stop();
        if (capture.running()) return 1;
    }
    std::cout << "Portable demo correctly reports unavailable live capture.\n";
}
