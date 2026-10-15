#pragma once

#include "data/MediaDataProvider.hpp"
#include <string>

namespace data {

bool platformMediaSessionSupported();
bool readPlatformMediaSession(MediaDataSnapshot& snapshot, std::string& error);

} // namespace data
