#include "data/HttpClient.hpp"

#include <curl/curl.h>
#include <mutex>

namespace data {
namespace {
std::once_flag curlInitialization;

std::size_t appendBody(char* contents, std::size_t size, std::size_t count,
                       void* context) {
    const std::size_t bytes = size * count;
    static_cast<std::string*>(context)->append(contents, bytes);
    return bytes;
}
}

bool httpsGet(const std::string& host, const std::string& path,
              std::string& response, std::string& error) {
    std::call_once(curlInitialization, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    response.clear();
    error.clear();
    CURL* request = curl_easy_init();
    if (!request) {
        error = "curl_easy_init failed";
        return false;
    }
    const std::string url = "https://" + host + path;
    curl_easy_setopt(request, CURLOPT_URL, url.c_str());
    curl_easy_setopt(request, CURLOPT_USERAGENT, "AudioViz-PC/0.0.2");
    curl_easy_setopt(request, CURLOPT_CONNECTTIMEOUT_MS, 4000L);
    curl_easy_setopt(request, CURLOPT_TIMEOUT_MS, 7000L);
    curl_easy_setopt(request, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(request, CURLOPT_MAXREDIRS, 2L);
    curl_easy_setopt(request, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, appendBody);
    curl_easy_setopt(request, CURLOPT_WRITEDATA, &response);
    const CURLcode result = curl_easy_perform(request);
    long status = 0;
    curl_easy_getinfo(request, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(request);
    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        return false;
    }
    if (status != 200) {
        error = "HTTP status " + std::to_string(status);
        return false;
    }
    return true;
}

} // namespace data
