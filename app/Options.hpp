#pragma once

#include <charconv>
#include <string>
#include <string_view>

namespace app {

struct Options {
    bool demo = false;
    bool help = false;
    bool listAudioDevices = false;
    int frameLimit = 0;
    int initialMode = 1;
    int initialStyle = 0;
    std::string screenshot;
    std::string audioDevice;
};

inline bool parseOptions(int argc, char* const argv[], Options& options, std::string& error) {
    options = Options{};
    error.clear();
    for (int i = 1; i < argc; ++i) {
        const std::string_view option(argv[i]);
        if (option == "--demo") options.demo = true;
        else if (option == "--list-audio-devices") options.listAudioDevices = true;
        else if (option == "--help") {
            options.help = true;
            return true;
        } else if (option == "--frames" || option == "--mode" || option == "--style" || option == "--screenshot" ||
                   option == "--audio-device") {
            if (i + 1 >= argc || std::string_view(argv[i + 1]).empty() ||
                std::string_view(argv[i + 1]).substr(0, 2) == "--") {
                error = std::string(option) + " requires a value";
                return false;
            }
            const std::string_view value(argv[++i]);
            if (option == "--screenshot") {
                options.screenshot = value;
                continue;
            }
            if (option == "--audio-device") {
                options.audioDevice = value;
                continue;
            }
            int number = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                (option == "--frames" && number < 0) ||
                (option == "--mode" && (number < 1 || number > 3)) ||
                (option == "--style" && (number < 0 || number > 6))) {
                error = std::string(option) + (option == "--frames"
                    ? " requires a nonnegative integer (0 means unlimited)"
                    : option == "--style" ? " requires an integer from 0 to 6"
                    : " requires 1, 2 or 3");
                return false;
            }
            if (option == "--frames") options.frameLimit = number;
            else if (option == "--mode") options.initialMode = number;
            else options.initialStyle = number;
        } else {
            error = "Unknown option: " + std::string(option);
            return false;
        }
    }
    if (options.demo && !options.audioDevice.empty()) {
        error = "--audio-device requires live audio; do not combine it with --demo";
        return false;
    }
    return true;
}

} // namespace app
