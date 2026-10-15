#pragma once

#include "audio/AudioFrame.hpp"
#include "data/MarketDataProvider.hpp"
#include "data/MediaDataProvider.hpp"
#include "data/UtilityFrame.hpp"
#include "data/WeatherDataProvider.hpp"
#include <cstdint>
#include <string>

namespace data {

// Owns platform-neutral utility-mode state. Network and OS media adapters can
// replace the preview fields without coupling those services to the renderer.
class UtilityDataProvider {
public:
    explicit UtilityDataProvider(bool enableLiveData = true,
                                 std::string weatherCity = "Hanoi");
    ~UtilityDataProvider();
    UtilityDataProvider(const UtilityDataProvider&) = delete;
    UtilityDataProvider& operator=(const UtilityDataProvider&) = delete;
    void update(std::uint32_t elapsedMilliseconds, const audio::AudioFrame& audioFrame);
    const UtilityFrame& snapshot() const { return frame_; }

private:
    void refreshClock();
    void refreshLocalData();
    void refreshDemoMarket();
    void consumeMarketUpdate();
    void consumeWeatherUpdate();
    void consumeMediaUpdate();

    UtilityFrame frame_;
    MarketDataProvider marketProvider_;
    WeatherDataProvider weatherProvider_;
    MediaDataProvider mediaProvider_;
    bool demoMode_ = false;
    double mediaPositionSeconds_ = 0.0;
    double mediaDurationSeconds_ = 0.0;
    std::uint64_t elapsedMilliseconds_ = 0;
    std::uint32_t refreshAccumulator_ = 0;
};

} // namespace data
