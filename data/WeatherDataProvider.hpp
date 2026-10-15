#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace data {

constexpr std::uint32_t kWeatherRefreshIntervalMs = 15 * 60 * 1000;

struct WeatherDataSnapshot {
    double temperatureCelsius = 0.0;
    int weatherCode = 0;
    std::string location;
};

class WeatherDataProvider {
public:
    WeatherDataProvider(bool enabled, std::string city);
    ~WeatherDataProvider();
    WeatherDataProvider(const WeatherDataProvider&) = delete;
    WeatherDataProvider& operator=(const WeatherDataProvider&) = delete;

    bool takeLatest(WeatherDataSnapshot& snapshot, bool& live, std::string& status);

    static bool parseGeocoding(const std::string& json, double& latitude,
                               double& longitude, std::string& location);
    static bool parseCurrentWeather(const std::string& json,
                                    double& temperatureCelsius, int& weatherCode);

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace data
