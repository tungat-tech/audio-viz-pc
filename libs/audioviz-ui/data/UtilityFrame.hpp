#pragma once

#include "data/MediaTypes.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace data {

constexpr std::size_t kMarketHistoryPoints = 48;

struct MarketQuote {
    std::string symbol;
    std::string price;
    float changePercent = 0.0f;
    std::array<float, kMarketHistoryPoints> history{};
};

struct UtilityFrame {
    std::string time = "--:--";
    std::string seconds = "--";
    std::string date = "--- -- ---";
    std::string temperature = "27 C";
    std::string weatherLocation = "HANOI";
    std::string weatherStatus = "LOADING";
    int weatherCode = 0;
    bool weatherLive = false;
    std::array<MarketQuote, 3> market{};
    bool marketLive = false;
    std::string marketStatus = "LOADING";
    std::uint64_t marketGeneration = 0;
    std::string trackTitle = "MIDNIGHT SIGNAL";
    std::string trackArtist = "AUDIOVIZ";
    std::string trackPosition = "00:00 / 04:08";
    float trackProgress = 0.0f;
    bool mediaLive = false;
    bool mediaPlaying = false;
    std::string mediaStatus = "LOADING";
    std::shared_ptr<const AlbumArt> albumArt;
    int focusMinutes = 42;
    float audioActivity = 0.0f;
    std::uint64_t generation = 0;
};

} // namespace data
