#include "data/HttpClient.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

namespace data {
namespace {
std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                            value.data(), static_cast<int>(value.size()),
                                            nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string winError(const char* operation) {
    return std::string(operation) + " failed (WinHTTP " +
           std::to_string(GetLastError()) + ")";
}
}

bool httpsGet(const std::string& host, const std::string& path,
              std::string& response, std::string& error) {
    response.clear();
    error.clear();
    const std::wstring wideHost = widen(host);
    const std::wstring widePath = widen(path);
    if (wideHost.empty() || widePath.empty()) {
        error = "Invalid UTF-8 in request URL";
        return false;
    }

    HINTERNET session = WinHttpOpen(L"AudioViz-PC/0.0.2",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        error = winError("WinHttpOpen");
        return false;
    }
    WinHttpSetTimeouts(session, 4000, 4000, 5000, 7000);
    HINTERNET connection = WinHttpConnect(session, wideHost.c_str(),
                                          INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) {
        error = winError("WinHttpConnect");
        WinHttpCloseHandle(session);
        return false;
    }
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", widePath.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    bool ok = request != nullptr;
    if (!ok) error = winError("WinHttpOpenRequest");
    if (ok && !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = winError("WinHttpSendRequest");
        ok = false;
    }
    if (ok && !WinHttpReceiveResponse(request, nullptr)) {
        error = winError("WinHttpReceiveResponse");
        ok = false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (ok && (!WinHttpQueryHeaders(request,
                                     WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                     WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                     WINHTTP_NO_HEADER_INDEX) || status != 200)) {
        error = status ? "HTTP status " + std::to_string(status)
                       : winError("WinHttpQueryHeaders");
        ok = false;
    }

    std::vector<char> buffer(8192);
    while (ok) {
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(),
                             static_cast<DWORD>(buffer.size()), &read)) {
            error = winError("WinHttpReadData");
            ok = false;
            break;
        }
        if (read == 0) break;
        response.append(buffer.data(), read);
    }

    if (request) WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

} // namespace data
