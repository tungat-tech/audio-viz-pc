#include "data/MarketDataProvider.hpp"
#include "data/HttpClient.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace data {
namespace {
constexpr const char* kHost = "api.binance.com";
constexpr std::array<const char*, 3> kSymbols{{"BTCUSDT", "ETHUSDT", "SOLUSDT"}};
constexpr const char* kTickerPath =
    "/api/v3/ticker/24hr?symbols=%5B%22BTCUSDT%22%2C%22ETHUSDT%22%2C%22SOLUSDT%22%5D";

bool jsonString(const std::string& json, std::string_view key,
                std::size_t begin, std::size_t end, std::string& value) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = json.find(needle, begin);
    if (keyPosition == std::string::npos || keyPosition >= end) return false;
    const std::size_t colon = json.find(':', keyPosition + needle.size());
    if (colon == std::string::npos || colon >= end) return false;
    const std::size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos || quote >= end) return false;
    const std::size_t close = json.find('"', quote + 1);
    if (close == std::string::npos || close > end) return false;
    value.assign(json, quote + 1, close - quote - 1);
    return true;
}

bool number(const std::string& text, double& value) {
    if (text.empty()) return false;
    char* tail = nullptr;
    value = std::strtod(text.c_str(), &tail);
    return tail == text.c_str() + text.size() && std::isfinite(value);
}

bool fetchSnapshot(MarketDataSnapshot& snapshot, std::string& error) {
    std::string response;
    if (!httpsGet(kHost, kTickerPath, response, error)) {
        error = "Ticker: " + error;
        return false;
    }
    if (!MarketDataProvider::parseTickerBatch(response, snapshot.prices,
                                               snapshot.changePercent)) {
        error = "Ticker response was malformed";
        return false;
    }
    for (std::size_t i = 0; i < kSymbols.size(); ++i) {
        const std::string path = "/api/v3/klines?symbol=" +
            std::string(kSymbols[i]) + "&interval=30m&limit=" +
            std::to_string(kMarketHistoryPoints);
        if (!httpsGet(kHost, path, response, error)) {
            error = std::string(kSymbols[i]) + " chart: " + error;
            return false;
        }
        if (!MarketDataProvider::parseKlines(response, snapshot.history[i])) {
            error = std::string(kSymbols[i]) + " chart response was malformed";
            return false;
        }
    }
    return true;
}
}

struct MarketDataProvider::State {
    explicit State(bool enabled) : enabled(enabled) {}
    bool enabled = true;
    bool stop = false;
    bool changed = false;
    bool live = false;
    MarketDataSnapshot latest;
    std::string status = "LOADING";
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
};

MarketDataProvider::MarketDataProvider(bool enabled)
    : state_(std::make_unique<State>(enabled)) {
    if (!enabled) {
        state_->status = "DEMO";
        return;
    }
    State* state = state_.get();
    state_->worker = std::thread([state] {
        for (;;) {
            const auto cycleStarted = std::chrono::steady_clock::now();
            MarketDataSnapshot next;
            std::string error;
            const bool success = fetchSnapshot(next, error);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->stop) break;
                if (success) state->latest = next;
                state->live = success;
                state->status = success ? "LIVE" : "STALE";
                state->changed = true;
            }
            std::unique_lock<std::mutex> lock(state->mutex);
            if (state->wake.wait_until(lock,
                    cycleStarted + std::chrono::milliseconds(kMarketRefreshIntervalMs),
                    [state] { return state->stop; })) break;
        }
    });
}

MarketDataProvider::~MarketDataProvider() {
    if (!state_ || !state_->worker.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->stop = true;
    }
    state_->wake.notify_all();
    state_->worker.join();
}

bool MarketDataProvider::takeLatest(MarketDataSnapshot& snapshot, bool& live,
                                    std::string& status) {
    if (!state_) return false;
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->changed) return false;
    snapshot = state_->latest;
    live = state_->live;
    status = state_->status;
    state_->changed = false;
    return true;
}

bool MarketDataProvider::parseTickerBatch(const std::string& json,
                                          std::array<double, 3>& prices,
                                          std::array<float, 3>& changes) {
    for (std::size_t i = 0; i < kSymbols.size(); ++i) {
        const std::string symbolNeedle = "\"symbol\":\"" +
                                         std::string(kSymbols[i]) + "\"";
        const std::size_t symbol = json.find(symbolNeedle);
        if (symbol == std::string::npos) return false;
        const std::size_t begin = json.rfind('{', symbol);
        const std::size_t end = json.find('}', symbol);
        if (begin == std::string::npos || end == std::string::npos) return false;
        std::string priceText;
        std::string changeText;
        double price = 0.0;
        double change = 0.0;
        if (!jsonString(json, "lastPrice", begin, end, priceText) ||
            !jsonString(json, "priceChangePercent", begin, end, changeText) ||
            !number(priceText, price) || !number(changeText, change) || price <= 0.0)
            return false;
        prices[i] = price;
        changes[i] = static_cast<float>(change);
    }
    return true;
}

bool MarketDataProvider::parseKlines(
        const std::string& json,
        std::array<float, kMarketHistoryPoints>& normalized) {
    std::vector<double> closes;
    closes.reserve(kMarketHistoryPoints);
    int depth = 0;
    bool quoted = false;
    bool escaped = false;
    std::size_t tokenStart = 0;
    int field = 0;
    for (std::size_t i = 0; i < json.size(); ++i) {
        const char ch = json[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') quoted = false;
            continue;
        }
        if (ch == '"') {
            quoted = true;
            continue;
        }
        if (ch == '[') {
            ++depth;
            if (depth == 2) {
                field = 0;
                tokenStart = i + 1;
            }
        } else if ((ch == ',' || ch == ']') && depth == 2) {
            if (field == 4) {
                std::string token = json.substr(tokenStart, i - tokenStart);
                token.erase(std::remove_if(token.begin(), token.end(),
                    [](unsigned char value) { return value == '"' || value == ' ' ||
                                                     value == '\n' || value == '\r' ||
                                                     value == '\t'; }), token.end());
                double close = 0.0;
                if (!number(token, close) || close <= 0.0) return false;
                closes.push_back(close);
            }
            ++field;
            tokenStart = i + 1;
            if (ch == ']') --depth;
        } else if (ch == ']') {
            --depth;
        }
    }
    if (closes.size() < kMarketHistoryPoints) return false;
    const auto start = closes.end() - static_cast<std::ptrdiff_t>(kMarketHistoryPoints);
    const auto range = std::minmax_element(start, closes.end());
    const double span = *range.second - *range.first;
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        normalized[i] = span < 1e-12 ? 0.5f : static_cast<float>(
            0.10 + (closes[closes.size() - normalized.size() + i] - *range.first) /
                   span * 0.80);
    }
    return true;
}

} // namespace data
