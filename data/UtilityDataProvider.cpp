#include "data/UtilityDataProvider.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace data {
namespace {
constexpr float kPi = 3.14159265358979323846f;

std::tm localTime(std::time_t value) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

std::string money(double value, int decimals) {
    char text[24];
    if (decimals == 0) {
        const auto rounded = static_cast<long long>(std::llround(value));
        if (rounded >= 1000)
            std::snprintf(text, sizeof(text), "$%lld,%03lld",
                          rounded / 1000, rounded % 1000);
        else
            std::snprintf(text, sizeof(text), "$%lld", rounded);
    } else {
        std::snprintf(text, sizeof(text), "$%.*f", decimals, value);
    }
    return text;
}

void makeHistory(MarketQuote& quote, float phase, float trend) {
    for (std::size_t i = 0; i < quote.history.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(quote.history.size() - 1);
        const float broad = std::sin(t * kPi * 3.1f + phase) * 0.13f;
        const float texture = std::sin(t * kPi * 11.0f + phase * 0.7f) * 0.055f;
        quote.history[i] = std::clamp(0.28f + trend * t + broad + texture, 0.05f, 0.95f);
    }
}
}

UtilityDataProvider::UtilityDataProvider(bool enableLiveData, std::string weatherCity)
    : marketProvider_(enableLiveData),
      weatherProvider_(enableLiveData, std::move(weatherCity)),
      mediaProvider_(enableLiveData),
      demoMode_(!enableLiveData) {
    frame_.market[0].symbol = "BTC";
    frame_.market[1].symbol = "ETH";
    frame_.market[2].symbol = "SOL";
    refreshClock();
    refreshLocalData();
    if (enableLiveData) {
        for (auto& quote : frame_.market) {
            quote.price = "--";
            quote.history.fill(0.5f);
        }
        frame_.temperature = "-- C";
        frame_.weatherLocation = "LOADING";
        frame_.trackTitle = "WAITING FOR MEDIA";
        frame_.trackArtist = "WINDOWS MEDIA SESSION";
        frame_.trackPosition = "--:-- / --:--";
        frame_.trackProgress = 0.0f;
    } else {
        refreshDemoMarket();
        frame_.marketStatus = "DEMO";
        frame_.marketGeneration = 1;
        frame_.weatherStatus = "DEMO";
        frame_.mediaStatus = "DEMO";
    }
}

UtilityDataProvider::~UtilityDataProvider() = default;

void UtilityDataProvider::update(std::uint32_t elapsedMilliseconds,
                                 const audio::AudioFrame& audioFrame) {
    elapsedMilliseconds_ += elapsedMilliseconds;
    refreshAccumulator_ += elapsedMilliseconds;
    frame_.audioActivity = std::clamp(audioFrame.visualActivity, 0.0f, 1.0f);
    if (!demoMode_ && frame_.mediaLive && frame_.mediaPlaying)
        mediaPositionSeconds_ = std::min(mediaDurationSeconds_,
            mediaPositionSeconds_ + static_cast<double>(elapsedMilliseconds) / 1000.0);
    if (refreshAccumulator_ < 250) return;
    refreshAccumulator_ %= 250;
    refreshClock();
    consumeMarketUpdate();
    consumeWeatherUpdate();
    consumeMediaUpdate();
    refreshLocalData();
    ++frame_.generation;
}

void UtilityDataProvider::refreshClock() {
    const std::time_t now = std::time(nullptr);
    const std::tm current = localTime(now);
    char text[24];
    std::strftime(text, sizeof(text), "%H:%M", &current);
    frame_.time = text;
    std::strftime(text, sizeof(text), "%S", &current);
    frame_.seconds = text;
    std::strftime(text, sizeof(text), "%a %d %b", &current);
    frame_.date = text;
    std::transform(frame_.date.begin(), frame_.date.end(), frame_.date.begin(),
                   [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
}

void UtilityDataProvider::refreshLocalData() {
    if (demoMode_) {
        constexpr int durationSeconds = 4 * 60 + 8;
        mediaDurationSeconds_ = durationSeconds;
        mediaPositionSeconds_ = static_cast<int>(elapsedMilliseconds_ / 1000) % durationSeconds;
    }
    const int position = std::max(0, static_cast<int>(mediaPositionSeconds_));
    const int duration = std::max(0, static_cast<int>(mediaDurationSeconds_));
    frame_.trackProgress = duration > 0
        ? std::clamp(static_cast<float>(mediaPositionSeconds_ / mediaDurationSeconds_), 0.0f, 1.0f)
        : 0.0f;
    char positionText[40];
    if (duration > 0) {
        std::snprintf(positionText, sizeof(positionText), "%02d:%02d / %02d:%02d",
                      position / 60, position % 60, duration / 60, duration % 60);
        frame_.trackPosition = positionText;
    } else if (!frame_.mediaLive) {
        frame_.trackPosition = "--:-- / --:--";
    }
    frame_.focusMinutes = std::max(1, 42 - static_cast<int>(elapsedMilliseconds_ / 60000) % 42);
}

void UtilityDataProvider::refreshDemoMarket() {
    frame_.market[0].price = money(68421.0, 0);
    frame_.market[1].price = money(2513.0, 0);
    frame_.market[2].price = money(148.7, 1);
    frame_.market[0].changePercent = 2.84f;
    frame_.market[1].changePercent = 1.62f;
    frame_.market[2].changePercent = 3.11f;
    makeHistory(frame_.market[0], 0.0f, 0.47f);
    makeHistory(frame_.market[1], 1.1f, 0.24f);
    makeHistory(frame_.market[2], 2.2f, 0.34f);
}

void UtilityDataProvider::consumeMarketUpdate() {
    MarketDataSnapshot update;
    bool live = false;
    std::string status;
    if (!marketProvider_.takeLatest(update, live, status)) return;
    frame_.marketLive = live;
    frame_.marketStatus = status;
    if (live) {
        frame_.market[0].price = money(update.prices[0], 0);
        frame_.market[1].price = money(update.prices[1], 0);
        frame_.market[2].price = money(update.prices[2], 1);
        for (std::size_t i = 0; i < frame_.market.size(); ++i) {
            frame_.market[i].changePercent = update.changePercent[i];
            frame_.market[i].history = update.history[i];
        }
    }
    ++frame_.marketGeneration;
}

void UtilityDataProvider::consumeWeatherUpdate() {
    WeatherDataSnapshot update;
    bool live = false;
    std::string status;
    if (!weatherProvider_.takeLatest(update, live, status)) return;
    frame_.weatherLive = live;
    frame_.weatherStatus = status;
    if (!live) return;
    frame_.weatherCode = update.weatherCode;
    frame_.weatherLocation = update.location;
    std::transform(frame_.weatherLocation.begin(), frame_.weatherLocation.end(),
                   frame_.weatherLocation.begin(), [](unsigned char value) {
                       return static_cast<char>(std::toupper(value));
                   });
    char text[16];
    std::snprintf(text, sizeof(text), "%.0f C", update.temperatureCelsius);
    frame_.temperature = text;
}

void UtilityDataProvider::consumeMediaUpdate() {
    MediaDataSnapshot update;
    bool live = false;
    std::string status;
    if (!mediaProvider_.takeLatest(update, live, status)) return;
    frame_.mediaLive = live;
    frame_.mediaStatus = status;
    if (!live) {
        frame_.mediaPlaying = false;
        frame_.albumArt.reset();
        if (status == "NO MEDIA") {
            frame_.trackTitle = "NO ACTIVE MEDIA";
            frame_.trackArtist = "OPEN SPOTIFY / VLC / BROWSER";
            mediaPositionSeconds_ = 0.0;
            mediaDurationSeconds_ = 0.0;
        } else if (frame_.trackTitle == "WAITING FOR MEDIA") {
            frame_.trackTitle = "MEDIA SESSION UNAVAILABLE";
            frame_.trackArtist = "PLAYER MUST EXPOSE WINDOWS MEDIA CONTROLS";
        }
        return;
    }
    frame_.trackTitle = update.title;
    frame_.trackArtist = update.artist;
    frame_.mediaPlaying = update.playing;
    mediaPositionSeconds_ = update.positionSeconds;
    mediaDurationSeconds_ = update.durationSeconds;
    frame_.albumArt = std::move(update.albumArt);
}

} // namespace data
