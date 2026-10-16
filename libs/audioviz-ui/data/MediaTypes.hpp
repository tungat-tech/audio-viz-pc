#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace data {

constexpr std::size_t kAlbumArtWidth = 80;
constexpr std::size_t kAlbumArtHeight = 84;

struct AlbumArt {
    std::array<std::uint16_t, kAlbumArtWidth * kAlbumArtHeight> rgb565{};
};

} // namespace data

