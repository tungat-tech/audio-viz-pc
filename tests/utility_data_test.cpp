#include "data/UtilityDataProvider.hpp"
#include "data/MarketDataProvider.hpp"
#include "data/MediaDataProvider.hpp"
#include "data/WeatherDataProvider.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}

int main() {
    require(data::kMarketRefreshIntervalMs == 5000,
            "Market refresh interval must remain 5 seconds");
    require(data::kMediaRefreshIntervalMs == 1000,
            "Media session refresh interval must remain 1 second");

    const std::string tickers = R"json([
      {"symbol":"ETHUSDT","priceChangePercent":"-1.25","lastPrice":"2513.40"},
      {"symbol":"SOLUSDT","priceChangePercent":"3.11","lastPrice":"148.70"},
      {"symbol":"BTCUSDT","priceChangePercent":"2.84","lastPrice":"68421.20"}
    ])json";
    std::array<double, 3> prices{};
    std::array<float, 3> changes{};
    require(data::MarketDataProvider::parseTickerBatch(tickers, prices, changes),
            "Binance ticker parser rejected valid data");
    require(std::abs(prices[0] - 68421.20) < 0.01 &&
            std::abs(changes[1] + 1.25f) < 0.001f,
            "Binance ticker fields were mapped to the wrong symbol");

    std::string klines = "[";
    for (std::size_t i = 0; i < data::kMarketHistoryPoints; ++i) {
        if (i) klines += ',';
        klines += "[1700000000000,\"1\",\"2\",\"0.5\",\"" +
                  std::to_string(100.0 + static_cast<double>(i)) +
                  "\",\"9\",1700000000999,\"0\",1,\"0\",\"0\",\"0\"]";
    }
    klines += ']';
    std::array<float, data::kMarketHistoryPoints> history{};
    require(data::MarketDataProvider::parseKlines(klines, history),
            "Binance kline parser rejected valid data");
    require(history.front() >= 0.09f && history.back() <= 0.91f &&
            history.front() < history.back(), "Kline normalization is invalid");

    const std::string geocoding = R"json({"results":[{"id":1,"name":"Hanoi","latitude":21.0245,"longitude":105.8412,"country_code":"VN"}]})json";
    double latitude = 0.0;
    double longitude = 0.0;
    std::string location;
    require(data::WeatherDataProvider::parseGeocoding(
                geocoding, latitude, longitude, location) &&
            std::abs(latitude - 21.0245) < 0.0001 && location == "Hanoi",
            "Open-Meteo geocoding parser failed");
    const std::string weather = R"json({"current":{"time":"2026-10-15T15:00","temperature_2m":27.4,"weather_code":3}})json";
    double temperature = 0.0;
    int weatherCode = -1;
    require(data::WeatherDataProvider::parseCurrentWeather(
                weather, temperature, weatherCode) &&
            std::abs(temperature - 27.4) < 0.01 && weatherCode == 3,
            "Open-Meteo current-weather parser failed");

    data::UtilityDataProvider provider(false);
    audio::AudioFrame audio;
    audio.visualActivity = 0.42f;
    provider.update(1000, audio);
    const auto& frame = provider.snapshot();
    require(frame.generation > 0, "Utility state did not refresh");
    require(frame.time.size() == 5 && frame.time[2] == ':', "Local time is malformed");
    require(frame.seconds.size() == 2, "Seconds are malformed");
    require(frame.market[0].symbol == "BTC" && !frame.market[0].price.empty(),
            "Market demo fallback is missing");
    require(frame.marketStatus == "DEMO" && !frame.marketLive,
            "Network-free test mode must be labeled DEMO");
    require(frame.weatherStatus == "DEMO" && frame.mediaStatus == "DEMO",
            "Network-free weather/media must be labeled DEMO");
    require(std::all_of(frame.market[0].history.begin(), frame.market[0].history.end(),
                        [](float value) { return value >= 0.0f && value <= 1.0f; }),
            "Market history escaped its normalized range");
    require(frame.trackProgress > 0.0f && frame.trackProgress < 1.0f,
            "Track preview progress is invalid");
    require(frame.audioActivity == 0.42f, "Audio activity was not propagated");
    std::cout << "Utility clock and preview data passed\n";
}
