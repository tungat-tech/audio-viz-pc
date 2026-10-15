#pragma once

#include "data/UtilityFrame.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace data {

constexpr std::uint32_t kMarketRefreshIntervalMs = 5000;

struct MarketDataSnapshot {
    std::array<double, 3> prices{};
    std::array<float, 3> changePercent{};
    std::array<std::array<float, kMarketHistoryPoints>, 3> history{};
};

// Background Binance REST poller. It publishes complete snapshots only, so a
// partially failed refresh can never mix prices and charts from different runs.
class MarketDataProvider {
public:
    explicit MarketDataProvider(bool enabled = true);
    ~MarketDataProvider();
    MarketDataProvider(const MarketDataProvider&) = delete;
    MarketDataProvider& operator=(const MarketDataProvider&) = delete;

    // Returns true when live/stale state or data changed since the last call.
    bool takeLatest(MarketDataSnapshot& snapshot, bool& live, std::string& status);

    // Public for deterministic parser tests; no network access is performed.
    static bool parseTickerBatch(const std::string& json,
                                 std::array<double, 3>& prices,
                                 std::array<float, 3>& changes);
    static bool parseKlines(const std::string& json,
                            std::array<float, kMarketHistoryPoints>& normalized);

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace data
