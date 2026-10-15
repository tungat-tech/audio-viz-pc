#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace data {

constexpr std::uint32_t kMediaRefreshIntervalMs = 1000;
constexpr std::size_t kAlbumArtWidth = 80;
constexpr std::size_t kAlbumArtHeight = 84;

struct AlbumArt {
    std::array<std::uint16_t, kAlbumArtWidth * kAlbumArtHeight> rgb565{};
};

struct MediaDataSnapshot {
    bool active = false;
    bool playing = false;
    std::string title;
    std::string artist;
    double positionSeconds = 0.0;
    double durationSeconds = 0.0;
    std::shared_ptr<const AlbumArt> albumArt;
};

class MediaDataProvider {
public:
    explicit MediaDataProvider(bool enabled);
    ~MediaDataProvider();
    MediaDataProvider(const MediaDataProvider&) = delete;
    MediaDataProvider& operator=(const MediaDataProvider&) = delete;

    bool takeLatest(MediaDataSnapshot& snapshot, bool& live, std::string& status);

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace data
