#include "data/MediaSessionPlatform.hpp"

namespace data {

bool platformMediaSessionSupported() { return false; }

bool readPlatformMediaSession(MediaDataSnapshot&, std::string& error) {
    error = "System media sessions are currently supported on Windows";
    return false;
}

} // namespace data
