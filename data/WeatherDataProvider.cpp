#include "data/WeatherDataProvider.hpp"
#include "data/HttpClient.hpp"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

namespace data {
namespace {
bool jsonNumber(const std::string& json, const std::string& key,
                std::size_t begin, std::size_t end, double& value) {
    const std::string needle = "\"" + key + "\"";
    const std::size_t position = json.find(needle, begin);
    if (position == std::string::npos || position >= end) return false;
    const std::size_t colon = json.find(':', position + needle.size());
    if (colon == std::string::npos || colon >= end) return false;
    const char* first = json.c_str() + colon + 1;
    char* tail = nullptr;
    value = std::strtod(first, &tail);
    return tail != first && static_cast<std::size_t>(tail - json.c_str()) <= end &&
           std::isfinite(value);
}

bool jsonString(const std::string& json, const std::string& key,
                std::size_t begin, std::size_t end, std::string& value) {
    const std::string needle = "\"" + key + "\"";
    const std::size_t position = json.find(needle, begin);
    if (position == std::string::npos || position >= end) return false;
    const std::size_t colon = json.find(':', position + needle.size());
    if (colon == std::string::npos || colon >= end) return false;
    const std::size_t first = json.find('"', colon + 1);
    if (first == std::string::npos || first >= end) return false;
    std::string result;
    bool escaped = false;
    for (std::size_t i = first + 1; i < end; ++i) {
        const char ch = json[i];
        if (escaped) {
            result.push_back(ch == 'n' ? '\n' : ch == 't' ? '\t' : ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            value = std::move(result);
            return true;
        } else {
            result.push_back(ch);
        }
    }
    return false;
}

std::string urlEncode(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (const unsigned char ch : value) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~')
            encoded << static_cast<char>(ch);
        else
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }
    return encoded.str();
}

bool fetchWeather(const std::string& city, WeatherDataSnapshot& snapshot,
                  std::string& error) {
    std::string response;
    const std::string geocodingPath = "/v1/search?name=" + urlEncode(city) +
        "&count=1&language=en&format=json";
    if (!httpsGet("geocoding-api.open-meteo.com", geocodingPath, response, error)) {
        error = "Geocoding: " + error;
        return false;
    }
    double latitude = 0.0;
    double longitude = 0.0;
    if (!WeatherDataProvider::parseGeocoding(response, latitude, longitude,
                                              snapshot.location)) {
        error = "Weather location was not found";
        return false;
    }
    std::ostringstream path;
    path.imbue(std::locale::classic());
    path << "/v1/forecast?latitude=" << std::fixed << std::setprecision(5) << latitude
         << "&longitude=" << longitude
         << "&current=temperature_2m,weather_code&timezone=auto";
    if (!httpsGet("api.open-meteo.com", path.str(), response, error)) {
        error = "Weather: " + error;
        return false;
    }
    if (!WeatherDataProvider::parseCurrentWeather(response, snapshot.temperatureCelsius,
                                                   snapshot.weatherCode)) {
        error = "Weather response was malformed";
        return false;
    }
    return true;
}
}

struct WeatherDataProvider::State {
    State(bool enabled, std::string city) : enabled(enabled), city(std::move(city)) {}
    bool enabled = true;
    std::string city;
    bool stop = false;
    bool changed = false;
    bool live = false;
    WeatherDataSnapshot latest;
    std::string status = "LOADING";
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
};

WeatherDataProvider::WeatherDataProvider(bool enabled, std::string city)
    : state_(std::make_unique<State>(enabled, std::move(city))) {
    if (!enabled) {
        state_->status = "DEMO";
        return;
    }
    State* state = state_.get();
    state_->worker = std::thread([state] {
        for (;;) {
            const auto cycleStarted = std::chrono::steady_clock::now();
            WeatherDataSnapshot next;
            std::string error;
            const bool success = fetchWeather(state->city, next, error);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->stop) break;
                if (success) state->latest = std::move(next);
                state->live = success;
                state->status = success ? "LIVE" : "STALE";
                state->changed = true;
            }
            std::unique_lock<std::mutex> lock(state->mutex);
            if (state->wake.wait_until(lock,
                    cycleStarted + std::chrono::milliseconds(kWeatherRefreshIntervalMs),
                    [state] { return state->stop; })) break;
        }
    });
}

WeatherDataProvider::~WeatherDataProvider() {
    if (!state_ || !state_->worker.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->stop = true;
    }
    state_->wake.notify_all();
    state_->worker.join();
}

bool WeatherDataProvider::takeLatest(WeatherDataSnapshot& snapshot, bool& live,
                                     std::string& status) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->changed) return false;
    snapshot = state_->latest;
    live = state_->live;
    status = state_->status;
    state_->changed = false;
    return true;
}

bool WeatherDataProvider::parseGeocoding(const std::string& json, double& latitude,
                                         double& longitude, std::string& location) {
    const std::size_t results = json.find("\"results\"");
    const std::size_t begin = json.find('{', results == std::string::npos ? 0 : results);
    const std::size_t end = json.find('}', begin == std::string::npos ? 0 : begin);
    if (results == std::string::npos || begin == std::string::npos ||
        end == std::string::npos) return false;
    return jsonNumber(json, "latitude", begin, end, latitude) &&
           jsonNumber(json, "longitude", begin, end, longitude) &&
           jsonString(json, "name", begin, end, location);
}

bool WeatherDataProvider::parseCurrentWeather(const std::string& json,
                                               double& temperatureCelsius,
                                               int& weatherCode) {
    const std::size_t current = json.find("\"current\"");
    const std::size_t begin = json.find('{', current == std::string::npos ? 0 : current);
    const std::size_t end = json.find('}', begin == std::string::npos ? 0 : begin);
    double code = 0.0;
    if (current == std::string::npos || begin == std::string::npos ||
        end == std::string::npos ||
        !jsonNumber(json, "temperature_2m", begin, end, temperatureCelsius) ||
        !jsonNumber(json, "weather_code", begin, end, code)) return false;
    weatherCode = static_cast<int>(std::llround(code));
    return true;
}

} // namespace data
