#pragma once

#include <string>

namespace data {

// Small HTTPS GET adapter. The Windows build uses WinHTTP so the public app
// does not need to ship an extra networking DLL; Unix builds use libcurl.
bool httpsGet(const std::string& host, const std::string& path,
              std::string& response, std::string& error);

} // namespace data
