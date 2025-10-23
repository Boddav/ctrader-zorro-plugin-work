#include "../include/auth.h"
#include "../include/network.h"
#include "../include/utils.h"
#include <sstream>
#include <fstream>

namespace Auth {

bool ParseJsonTokenFields(const std::string& body, std::string& access, std::string& refresh) {
    access.clear();
    refresh.clear();

    auto extract = [&](const char* key)->std::string {
        size_t p = body.find(key);
        if (p == std::string::npos) return {};

        size_t colon = body.find(':', p + strlen(key));
        if (colon == std::string::npos) return {};

        size_t q = colon + 1;
        while (q < body.size() && (body[q] == ' ' || body[q] == '\t')) ++q;
        if (q >= body.size() || body[q] != '"') return {};

        size_t start = q + 1;
        size_t end = body.find('"', start);
        if (end == std::string::npos) return {};

        return body.substr(start, end - start);
    };

    access = extract("\"access_token\"");
    if (access.empty()) access = extract("\"accessToken\"");

    refresh = extract("\"refresh_token\"");
    if (refresh.empty()) refresh = extract("\"refreshToken\"");

    return !access.empty();
}

bool BodyIndicatesError(const std::string& body, const char* extraTag) {
    size_t p = body.find("\"errorCode\"");
    if (p == std::string::npos) return false; // no errorCode field
    size_t colon = body.find(':', p);
    if (colon == std::string::npos) return false;
    size_t v = colon + 1;
    while (v < body.size() && (body[v] == ' ' || body[v] == '\t' || body[v] == '"')) ++v;
    // Accept null or empty string as non-error
    if (v >= body.size()) return false;
    if (body.compare(v, 4, "null") == 0 || body[v] == '"') {
        // If quoted, extract content
        if (body[v] == '"') {
            size_t end = body.find('"', v + 1);
            if (end == std::string::npos) return false;
            if (end == v + 1) return false; // ""
            std::string val = body.substr(v + 1, end - v - 1);
            if (val.empty()) return false;
            // Sometimes API returns explicit strings for errors, treat non-empty as error
            if (val == "null" || val == "NULL") return false;
            return true;
        }
        return false; // null
    }
    // Non-null literal (number / word) � treat non-zero / non-null token as error
    if (extraTag && *extraTag && body.find(extraTag) != std::string::npos) return true;
    return true;
}

bool LoadTokenFromDisk() {
    char path[MAX_PATH];
    sprintf_s(path, "%soauth_token.json", G.DllPath);

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string content = buffer.str();
    if (content.empty()) return false;

    std::string token, refresh;
    if (!ParseJsonTokenFields(content, token, refresh)) return false;
    if (token.empty()) return false;

    strncpy_s(G.Token, sizeof(G.Token), token.c_str(), _TRUNCATE);
    if (!refresh.empty()) {
        strncpy_s(G.RefreshToken, sizeof(G.RefreshToken), refresh.c_str(), _TRUNCATE);
    }

    // Also load client_id and client_secret if available
    auto extractString = [&](const char* key)->std::string {
        size_t p = content.find(key);
        if (p == std::string::npos) return {};

        size_t colon = content.find(':', p + strlen(key));
        if (colon == std::string::npos) return {};

        size_t q = colon + 1;
        while (q < content.size() && (content[q] == ' ' || content[q] == '\t')) ++q;
        if (q >= content.size() || content[q] != '"') return {};

        size_t start = q + 1;
        size_t end = content.find('"', start);
        if (end == std::string::npos) return {};

        return content.substr(start, end - start);
    };

    std::string clientId = extractString("\"client_id\"");
    std::string clientSecret = extractString("\"client_secret\"");

    if (!clientId.empty()) {
        strncpy_s(G.ClientId, sizeof(G.ClientId), clientId.c_str(), _TRUNCATE);
        Utils::LogToFile("TOKEN_LOAD_DEBUG", "Loaded client_id from token file");
    }
    if (!clientSecret.empty()) {
        strncpy_s(G.ClientSecret, sizeof(G.ClientSecret), clientSecret.c_str(), _TRUNCATE);
        Utils::LogToFile("TOKEN_LOAD_DEBUG", "Loaded client_secret from token file");
    }

    return true;
}

bool SaveTokenToDisk(const std::string& access, const std::string& refresh) {
    char path[MAX_PATH];
    sprintf_s(path, "%soauth_token.json", G.DllPath);

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"access_token\": \"" << access << "\",\n";
    file << "  \"refresh_token\": \"" << refresh << "\",\n";
    file << "  \"client_id\": \"" << G.ClientId << "\",\n";
    file << "  \"client_secret\": \"" << G.ClientSecret << "\"\n";
    file << "}\n";
    file.close();

    return true;
}

bool WinHttpGetToken(const std::wstring& host, const std::wstring& path,
                    const std::map<std::string, std::string>& params,
                    std::string& outBody) {
    outBody.clear();

    std::string postData;
    bool first = true;

    for (const auto& param : params) {
        if (!first) {
            postData += "&";
        }
        postData += param.first + "=" + param.second;
        first = false;
    }

    HINTERNET hSession = WinHttpOpen(L"cTrader-Zorro/1.2.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring hdr = L"Content-Type: application/x-www-form-urlencoded\r\nAccept: application/json\r\n";
    WinHttpAddRequestHeaders(hRequest, hdr.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    DWORD dwLen = (DWORD)postData.size();
    BOOL result = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)postData.data(), dwLen, dwLen, 0);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0, size = sizeof(status);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        char m[128];
        sprintf_s(m, "HTTP status: %lu", status);
        Utils::ShowMsg(m);
    }

    DWORD dwSize = 0;
    do {
        DWORD dwDownloaded = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        std::string chunk;
        chunk.resize(dwSize);
        if (!WinHttpReadData(hRequest, &chunk[0], dwSize, &dwDownloaded)) break;
        chunk.resize(dwDownloaded);
        outBody += chunk;
    } while (dwSize > 0);

    if (!outBody.empty()) Utils::ShowMsg("HTTP body:", outBody.c_str());

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return !outBody.empty();
}

bool RefreshAccessToken() {
    if (!G.RefreshToken[0]) {
        Utils::ShowMsg("No refresh token");
        return false;
    }

    Utils::ShowMsg("Refreshing token...");

    // Debug: Log the parameters being sent
    char debugMsg[1024];
    sprintf_s(debugMsg, "Refresh params: client_id='%s', client_secret='%s', refresh_token='%.20s...'",
              G.ClientId, G.ClientSecret, G.RefreshToken);
    Utils::LogToFile("TOKEN_REFRESH_DEBUG", debugMsg);

    std::map<std::string, std::string> params;
    params["grant_type"] = "refresh_token";
    params["refresh_token"] = G.RefreshToken;
    params["client_id"] = G.ClientId;
    params["client_secret"] = G.ClientSecret;

    std::string responseBody;
    if (!WinHttpGetToken(L"openapi.ctrader.com", L"/apps/token", params, responseBody)) {
        Utils::ShowMsg("Token refresh failed");
        return false;
    }

    std::string newAccess, newRefresh;
    if (!ParseJsonTokenFields(responseBody, newAccess, newRefresh)) {
        Utils::ShowMsg("Parse failed");
        Utils::LogToFile("TOKEN_REFRESH_FAIL", responseBody.c_str());
        return false;
    }

    if (newAccess.empty()) return false;

    strncpy_s(G.Token, sizeof(G.Token), newAccess.c_str(), _TRUNCATE);
    if (!newRefresh.empty()) {
        strncpy_s(G.RefreshToken, sizeof(G.RefreshToken), newRefresh.c_str(), _TRUNCATE);
    }

    SaveTokenToDisk(newAccess, newRefresh);
    Utils::ShowMsg("Token refreshed");
    return true;
}

bool FetchAccountsList(std::vector<long long>& accountIds, CtraderEnv requestedEnv) {
    accountIds.clear();

    // Use REST API instead of WebSocket for account detection
    std::wstring whost = L"api.spotware.com";
    std::wstring wpath = L"/connect/tradingaccounts?access_token=";
    std::wstring wtoken(G.Token, G.Token + strlen(G.Token));
    wpath += wtoken;

    // For REST API, we need a simple GET request, not the POST-based WinHttpGetToken
    // Let's create a simple GET request instead
    std::string response;

    HINTERNET hSession = WinHttpOpen(L"Zorro-cTrader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        Utils::ShowMsg("REST API session creation failed");
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        Utils::ShowMsg("REST API connection failed");
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        Utils::ShowMsg("REST API request creation failed");
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        Utils::ShowMsg("REST API request failed");
        return false;
    }

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

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (response.empty()) {
        Utils::ShowMsg("Empty REST API response");
        return false;
    }

    Utils::LogToFile("ACCOUNTS_REST_RESPONSE", response.c_str());

    // Parse JSON response: {"data":[{"accountId":44533070,...}]}
    const char* dataStart = strstr(response.c_str(), "\"data\":");
    if (!dataStart) {
        Utils::ShowMsg("No data field in accounts response");
        return false;
    }

    // Store original environment preference from parameter
    CtraderEnv originalEnv = requestedEnv;
    std::vector<long long> liveAccounts;
    std::vector<long long> demoAccounts;

    const char* current = dataStart;
    while ((current = strstr(current, "\"accountId\":"))) {
        long long aid = 0;
        if (sscanf_s(current, "\"accountId\":%lld", &aid) == 1 && aid > 0) {

            // Parse the "live" flag for this account
            const char* liveStart = strstr(current, "\"live\":");
            bool isLiveAccount = false;
            if (liveStart) {
                // Find the next account boundary or end of current account object
                const char* nextAccount = strstr(current + 15, "\"accountId\":");
                const char* endBoundary = nextAccount ? nextAccount : (current + strlen(current));

                // Check if "live":true is within this account's data
                if (liveStart < endBoundary) {
                    const char* truePos = strstr(liveStart, "\"live\":true");
                    const char* falsePos = strstr(liveStart, "\"live\":false");

                    if (truePos && truePos < endBoundary) {
                        isLiveAccount = true;
                    } else if (falsePos && falsePos < endBoundary) {
                        isLiveAccount = false;
                    }
                }
            }

            // Log account environment info
            char envMsg[256];
            sprintf_s(envMsg, "Account %lld is %s environment", aid, isLiveAccount ? "LIVE" : "DEMO");
            Utils::LogToFile("ACCOUNT_ENV_DETECT", envMsg);

            // Categorize accounts by environment
            if (isLiveAccount) {
                liveAccounts.push_back(aid);
            } else {
                demoAccounts.push_back(aid);
            }

            char msg[128];
            sprintf_s(msg, "Found account ID: %lld (%s)", aid, isLiveAccount ? "LIVE" : "DEMO");
            Utils::LogToFile("ACCOUNT_FOUND", msg);
        }
        current++;
    }

    bool envLocked = G.envLocked;

    // Environment selection based on account detection
    if (envLocked) {
        if (requestedEnv == CtraderEnv::Live) {
            if (liveAccounts.empty()) {
                Utils::LogToFile("ENV_SELECTION_FAIL", "Forced LIVE environment has no available accounts");
                Utils::ShowMsg("Error: No live accounts available for requested environment");
                return false;
            }
            accountIds = liveAccounts;
            G.Env = CtraderEnv::Live;
            Utils::LogToFile("ENV_SELECTION", "Using LIVE environment - forced by Zorro Type parameter");
        } else {
            if (demoAccounts.empty()) {
                Utils::LogToFile("ENV_SELECTION_FAIL", "Forced DEMO environment has no available accounts");
                Utils::ShowMsg("Error: No demo accounts available for requested environment");
                return false;
            }
            accountIds = demoAccounts;
            G.Env = CtraderEnv::Demo;
            Utils::LogToFile("ENV_SELECTION", "Using DEMO environment - forced by Zorro Type parameter");
        }
    } else if (!liveAccounts.empty() && !demoAccounts.empty()) {
        // We have both types - use the requested environment
        if (originalEnv == CtraderEnv::Live) {
            accountIds = liveAccounts;
            G.Env = CtraderEnv::Live;
            Utils::LogToFile("ENV_SELECTION", "Using LIVE environment - live accounts found and requested");
        } else {
            accountIds = demoAccounts;
            G.Env = CtraderEnv::Demo;
            Utils::LogToFile("ENV_SELECTION", "Using DEMO environment - demo accounts found and requested");
        }
    } else if (!liveAccounts.empty()) {
        // Only live accounts available - must use live
        accountIds = liveAccounts;
        G.Env = CtraderEnv::Live;
        Utils::LogToFile("ENV_SELECTION", "Using LIVE environment - only live accounts available");
    } else if (!demoAccounts.empty()) {
        // Only demo accounts available - must use demo
        accountIds = demoAccounts;
        G.Env = CtraderEnv::Demo;
        Utils::LogToFile("ENV_SELECTION", "Using DEMO environment - only demo accounts available");
    }

    // Log the final environment decision
    char finalMsg[256];
    sprintf_s(finalMsg, "Final environment: %s with %zu accounts",
             (G.Env == CtraderEnv::Live) ? "LIVE" : "DEMO", accountIds.size());
    Utils::LogToFile("ENV_FINAL", finalMsg);

    if (accountIds.empty()) {
        Utils::ShowMsg("No account IDs found in REST response");
        return false;
    }

    char msg[128];
    sprintf_s(msg, "Found %zu accounts via REST API", accountIds.size());
    Utils::ShowMsg(msg);
    return true;
}

// Additional functions from grok2.txt integration

bool AppAuth(const std::string& clientId, const std::string& clientSecret, CtraderEnv env) {
    if (clientId.empty() || clientSecret.empty()) {
        Utils::ShowMsg("ERROR: AppAuth - Missing client credentials");
        return false;
    }

    Utils::LogToFile("APPAUTH", "Starting OAuth2 application authentication");

    // Use REST API for OAuth2 token
    std::map<std::string, std::string> params;
    params["grant_type"] = "client_credentials";
    params["client_id"] = clientId;
    params["client_secret"] = clientSecret;

    std::wstring host = (env == CtraderEnv::Live) ? L"openapi.ctrader.com" : L"demo-openapi.ctrader.com";
    std::string responseBody;

    if (!WinHttpGetToken(host, L"/apps/token", params, responseBody)) {
        Utils::ShowMsg("ERROR: AppAuth - HTTP request failed");
        return false;
    }

    if (BodyIndicatesError(responseBody)) {
        std::string errorDesc = Utils::GetErrorDescription(responseBody.c_str());
        Utils::ShowMsg("ERROR: AppAuth - OAuth failed", errorDesc.c_str());
        Utils::LogToFile("APPAUTH_ERROR", responseBody.c_str());
        return false;
    }

    std::string accessToken, refreshToken;
    if (!ParseJsonTokenFields(responseBody, accessToken, refreshToken)) {
        Utils::ShowMsg("ERROR: AppAuth - Failed to parse tokens");
        Utils::LogToFile("APPAUTH_PARSE_ERROR", responseBody.c_str());
        return false;
    }

    if (accessToken.empty()) {
        Utils::ShowMsg("ERROR: AppAuth - Empty access token");
        return false;
    }

    // Store tokens
    strncpy_s(G.Token, sizeof(G.Token), accessToken.c_str(), _TRUNCATE);
    if (!refreshToken.empty()) {
        strncpy_s(G.RefreshToken, sizeof(G.RefreshToken), refreshToken.c_str(), _TRUNCATE);
    }

    // Save to disk
    if (!SaveTokenToDisk(accessToken, refreshToken)) {
        Utils::LogToFile("APPAUTH_WARNING", "Failed to save tokens to disk");
    }

    Utils::LogToFile("APPAUTH", "OAuth2 application authentication successful");
    return true;
}

bool AccountAuth(long long accountId) {
    if (accountId <= 0) {
        Utils::ShowMsg("ERROR: AccountAuth - Invalid account ID");
        return false;
    }

    if (!G.Token[0]) {
        Utils::ShowMsg("ERROR: AccountAuth - No access token available");
        return false;
    }

    Utils::LogToFile("ACCOUNTAUTH", ("Starting account authentication for ID: " + std::to_string(accountId)).c_str());

    // This will be handled by WebSocket in main.cpp
    // Here we just validate the prerequisites
    return true;
}

bool SaveTokenToDisk() {
    return SaveTokenToDisk(G.Token, G.RefreshToken);
}

bool IsAuthenticated() {
    return G.Token[0] != '\0' && G.HasLogin;
}

void ClearAuth() {
    ZeroMemory(G.Token, sizeof(G.Token));
    ZeroMemory(G.RefreshToken, sizeof(G.RefreshToken));
    ZeroMemory(G.ClientId, sizeof(G.ClientId));
    ZeroMemory(G.ClientSecret, sizeof(G.ClientSecret));
    G.HasLogin = false;
    G.CTraderAccountId = 0;
    Utils::LogToFile("AUTH", "Authentication data cleared");
}

} // namespace Auth

