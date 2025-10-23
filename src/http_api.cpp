#include "../include/http_api.h"
#include "../include/globals.h"
#include "../include/utils.h"
#include <winhttp.h>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>

namespace HttpApi {

namespace {
    constexpr auto REQUEST_COOLDOWN = std::chrono::milliseconds(500);
    std::mutex g_rateMutex;
    std::chrono::steady_clock::time_point g_lastRequest;

    void ApplyRateLimit() {
        std::chrono::steady_clock::duration sleepDuration{0};
        {
            std::lock_guard<std::mutex> lock(g_rateMutex);
            auto now = std::chrono::steady_clock::now();
            if (g_lastRequest.time_since_epoch().count() == 0) {
                g_lastRequest = now;
                return;
            }

            auto earliest = g_lastRequest + REQUEST_COOLDOWN;
            if (now < earliest) {
                sleepDuration = earliest - now;
                g_lastRequest = earliest;
            } else {
                g_lastRequest = now;
            }
        }

        if (sleepDuration > std::chrono::steady_clock::duration::zero()) {
            Utils::LogToFile("HTTP_RATE_LIMIT", "Cooldown active, sleeping before next request");
            std::this_thread::sleep_for(sleepDuration);
        }
    }
}

std::string HttpRequest(const char* url, const char* data, const char* headers, const char* method) {
    if (!url) return "";

    ApplyRateLimit();

    std::string response;

    // Parse URL components
    std::wstring wurl(url, url + strlen(url));

    // Simple URL parsing (this could be enhanced)
    size_t schemeEnd = wurl.find(L"://");
    if (schemeEnd == std::wstring::npos) return "";

    bool isHttps = (wurl.substr(0, schemeEnd) == L"https");
    size_t hostStart = schemeEnd + 3;
    size_t pathStart = wurl.find(L'/', hostStart);

    std::wstring host;
    std::wstring path;

    if (pathStart != std::wstring::npos) {
        host = wurl.substr(hostStart, pathStart - hostStart);
        path = wurl.substr(pathStart);
    } else {
        host = wurl.substr(hostStart);
        path = L"/";
    }

    // Determine HTTP method
    std::wstring wmethod = L"GET";
    if (method) {
        std::string methodStr(method);
        wmethod = std::wstring(methodStr.begin(), methodStr.end());
    } else if (data && strlen(data) > 0) {
        wmethod = L"POST";
    }

    // Create WinHTTP session
    HINTERNET hSession = WinHttpOpen(L"Zorro-cTrader/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return "";

    // Connect to server
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Create request
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Add headers if provided
    if (headers && strlen(headers) > 0) {
        std::wstring wheaders(headers, headers + strlen(headers));
        WinHttpAddRequestHeaders(hRequest, wheaders.c_str(), (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    // Add default content type for POST requests
    if (data && strlen(data) > 0) {
        WinHttpAddRequestHeaders(hRequest,
            L"Content-Type: application/json\r\n", (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send request
    DWORD dataLen = data ? (DWORD)strlen(data) : 0;
    BOOL result = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)data, dataLen, dataLen, 0);

    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Read response data
    DWORD dwSize = 0;
    do {
        DWORD dwDownloaded = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        std::string chunk;
        chunk.resize(dwSize);
        if (!WinHttpReadData(hRequest, &chunk[0], dwSize, &dwDownloaded)) break;
        chunk.resize(dwDownloaded);
        response += chunk;
    } while (dwSize > 0);

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Log the request for debugging
    char logMsg[512];
    sprintf_s(logMsg, "HTTP %s %s -> %d bytes",
              method ? method : (data ? "POST" : "GET"),
              url, (int)response.length());
    Utils::LogToFile("HTTP_REQUEST", logMsg);

    return response;
}

std::string Get(const char* url, const char* headers) {
    return HttpRequest(url, nullptr, headers, "GET");
}

std::string Post(const char* url, const char* data, const char* headers) {
    return HttpRequest(url, data, headers, "POST");
}

std::string Put(const char* url, const char* data, const char* headers) {
    return HttpRequest(url, data, headers, "PUT");
}

std::string Delete(const char* url, const char* headers) {
    return HttpRequest(url, nullptr, headers, "DELETE");
}

// cTrader REST API specific functions
std::string GetAccountInfo(long long accountId) {
    char url[256];
    sprintf_s(url, "https://api.spotware.com/connect/tradingaccounts/%lld?access_token=%s",
              accountId, G.Token);

    return Get(url, "Accept: application/json");
}

std::string GetSymbolInfo(const char* symbol) {
    char url[256];
    sprintf_s(url, "https://api.spotware.com/connect/symbols?access_token=%s&symbol=%s",
              G.Token, symbol);

    return Get(url, "Accept: application/json");
}

std::string GetPositions(long long accountId) {
    char url[256];
    sprintf_s(url, "https://api.spotware.com/connect/tradingaccounts/%lld/positions?access_token=%s",
              accountId, G.Token);

    return Get(url, "Accept: application/json");
}

} // namespace HttpApi

