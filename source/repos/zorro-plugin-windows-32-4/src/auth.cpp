#include "../include/state.h"
#include "../include/auth.h"
#include "../include/protocol.h"
#include "../include/websocket.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <shellapi.h>

namespace Auth {

void DetectEnv(const char* type) {
    if (!type) {
        G.env = Env::Demo;
        return;
    }

    std::string t(type);
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);

    if (t.find("live") != std::string::npos || t.find("real") != std::string::npos) {
        G.env = Env::Live;
    } else {
        G.env = Env::Demo;
    }

    G.envLocked = true;
    Log::Info("AUTH", "Environment set to %s (locked)",
             G.env == Env::Live ? "LIVE" : "DEMO");
}

bool ApplicationAuth() {
    char payload[512];
    sprintf_s(payload, "\"clientId\":\"%s\",\"clientSecret\":\"%s\"",
              G.clientId, G.clientSecret);

    const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                             PayloadType::ApplicationAuthReq, payload);
    if (!WebSocket::Send(msg)) return false;

    char response[8192] = {0};
    ULONGLONG start = Utils::NowMs();
    while (Utils::NowMs() - start < (ULONGLONG)G.waitTime) {
        int n = WebSocket::Receive(response, sizeof(response));
        if (n > 0) {
            int pt = Protocol::ExtractPayloadType(response);
            if (pt == ToInt(PayloadType::ApplicationAuthRes)) {
                Log::Info("AUTH", "Application authenticated");
                return true;
            }
            if (pt == ToInt(PayloadType::ErrorRes)) {
                Log::Error("AUTH", "App auth failed: %s",
                          Protocol::ExtractString(response, "description"));
                return false;
            }
        }
        Sleep(10);
    }

    Log::Error("AUTH", "Application auth timeout");
    return false;
}

bool AccountAuth() {
    char payload[2560];
    sprintf_s(payload, "\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld",
              G.accessToken, G.accountId);

    const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                             PayloadType::AccountAuthReq, payload);
    if (!WebSocket::Send(msg)) return false;

    char response[8192] = {0};
    ULONGLONG start = Utils::NowMs();
    while (Utils::NowMs() - start < (ULONGLONG)G.waitTime) {
        int n = WebSocket::Receive(response, sizeof(response));
        if (n > 0) {
            int pt = Protocol::ExtractPayloadType(response);
            if (pt == ToInt(PayloadType::AccountAuthRes)) {
                Log::Info("AUTH", "Account %lld authenticated", G.accountId);
                return true;
            }
            if (pt == ToInt(PayloadType::ErrorRes)) {
                Log::Error("AUTH", "Account auth failed: %s",
                          Protocol::ExtractString(response, "description"));
                return false;
            }
        }
        Sleep(10);
    }

    Log::Error("AUTH", "Account auth timeout");
    return false;
}

bool LoadAccountsCsv(const char* user, const char* pwd) {
    // Header-based CSV loading (like v3 csv_loader.cpp)
    // Searches by header names, not fixed column positions.
    // Looks for: User/ClientId, Pass/ClientSecret/Password, AccountId/AccountNumber,
    //            ctidTraderAccountId, RedirectUri, Scope, Product, Name, Real, Plugin, Server

    // Try multiple paths
    const char* paths[] = { "accounts.csv", "Accounts.csv", "account.csv" };
    char csvPath[MAX_PATH];
    std::ifstream file;

    for (auto& fn : paths) {
        sprintf_s(csvPath, "%s%s", G.dllDir, fn);
        file.open(csvPath);
        if (file.is_open()) break;
    }

    if (!file.is_open()) {
        Log::Error("AUTH", "No accounts.csv found in %s", G.dllDir);
        return false;
    }

    Log::Info("AUTH", "Reading CSV: %s", csvPath);

    // Read header and build column index (case-insensitive)
    std::string headerLine;
    if (!std::getline(file, headerLine)) return false;
    if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();

    // Split header and build index
    std::vector<std::string> headers;
    std::map<std::string, int> colIdx;
    {
        std::istringstream ss(headerLine);
        std::string cell;
        int i = 0;
        while (std::getline(ss, cell, ',')) {
            cell = Utils::Trim(cell);
            // Remove trailing dots (ctidTraderAccountId.)
            while (!cell.empty() && cell.back() == '.') cell.pop_back();
            std::transform(cell.begin(), cell.end(), cell.begin(), ::tolower);
            headers.push_back(cell);
            colIdx[cell] = i++;
        }
    }

    Log::Info("AUTH", "CSV has %d columns (user=%s)", (int)headers.size(), user ? user : "(null)");

    // Helper to get cell value by column name (case-insensitive)
    auto getCol = [&](const std::vector<std::string>& row, const char* name) -> std::string {
        std::string key(name);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        auto it = colIdx.find(key);
        if (it == colIdx.end() || it->second >= (int)row.size()) return "";
        return row[it->second];
    };

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // Split row
        std::vector<std::string> row;
        std::istringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')) {
            row.push_back(Utils::Trim(cell));
        }

        if (row.size() < 3) continue;

        Log::Diag(1, "CSV row: %d fields, name=%s user=%.30s acctId=%s",
                  (int)row.size(),
                  getCol(row, "Name").c_str(),
                  getCol(row, "User").c_str(),
                  getCol(row, "ctidTraderAccountId").c_str());

        // Extract fields by header name (multiple alternatives like v3)
        std::string plugin = getCol(row, "Plugin");
        std::string server = getCol(row, "Server");
        std::string csvUser = getCol(row, "User");
        if (csvUser.empty()) csvUser = getCol(row, "ClientId");
        std::string csvPass = getCol(row, "Pass");
        if (csvPass.empty()) csvPass = getCol(row, "ClientSecret");
        if (csvPass.empty()) csvPass = getCol(row, "Password");
        std::string accountId = getCol(row, "AccountId");
        if (accountId.empty()) accountId = getCol(row, "AccountNumber");
        if (accountId.empty()) accountId = getCol(row, "ctidTraderAccountId");
        std::string redirectUri = getCol(row, "RedirectUri");
        if (redirectUri.empty()) redirectUri = getCol(row, "RedirectURL");
        // CSV columns may be misaligned - if redirectUri doesn't look like a URL, search all cells
        if (redirectUri.find("http") == std::string::npos) {
            redirectUri.clear();
            for (const auto& c : row) {
                if (c.find("http://") == 0 || c.find("https://") == 0) {
                    redirectUri = c;
                    break;
                }
            }
        }
        std::string scope = getCol(row, "Scope");
        std::string product = getCol(row, "Product");
        std::string name = getCol(row, "Name");
        std::string realFlag = getCol(row, "Real");

        // Filter: only cTrader rows - check server OR plugin OR any cell containing "ctrader"
        // NOTE: CSV columns may be misaligned (e.g. Plugin gets URL, NFA gets "cTrader.dll")
        // so we check server first, then do a broad search if server is inconclusive
        bool isCtraderRow = false;
        if (!server.empty() && Utils::ContainsCI(server.c_str(), "ctrader")) isCtraderRow = true;
        if (!isCtraderRow && !plugin.empty() && Utils::ContainsCI(plugin.c_str(), "ctrader")) isCtraderRow = true;
        if (!isCtraderRow) {
            // Broad search: check if any cell contains "ctrader"
            for (const auto& c : row) {
                if (Utils::ContainsCI(c.c_str(), "ctrader")) { isCtraderRow = true; break; }
            }
        }
        if (!isCtraderRow) continue;

        // NOTE: No env filtering here (v3 didn't filter by env in CSV)
        // The env only affects which server to connect to, not which CSV row to use.
        // Zorro passes the User value from the selected CSV row.

        // Match by user hint (Zorro passes User from CSV)
        if (user && *user) {
            bool match = false;
            if (csvUser == user) match = true;
            if (!match && !accountId.empty() && accountId == user) match = true;
            if (!match && Utils::ContainsCI(csvUser.c_str(), user)) match = true;
            if (!match) continue;
        }

        // Skip if missing essential fields
        if (csvUser.empty() || csvPass.empty()) continue;

        // Accept this row
        strcpy_s(G.clientId, csvUser.c_str());
        strcpy_s(G.clientSecret, csvPass.c_str());
        if (!accountId.empty()) {
            G.accountId = _atoi64(accountId.c_str());
        }
        if (!redirectUri.empty()) {
            G.redirectUri = redirectUri;
        } else {
            G.redirectUri = "http://127.0.0.1:53123/callback";
        }

        // Detect env from Name/Real fields
        if (!G.envLocked) {
            if (!realFlag.empty()) {
                int rf = atoi(realFlag.c_str());
                G.env = rf ? Env::Live : Env::Demo;
                G.envLocked = true;
            } else if (!name.empty()) {
                DetectEnv(name.c_str());
            }
        }

        Log::Info("AUTH", "CSV loaded: clientId=%.20s... accountId=%lld env=%s redirectUri=%s",
                  G.clientId, G.accountId,
                  G.env == Env::Live ? "LIVE" : "DEMO",
                  G.redirectUri.c_str());
        return true;
    }

    Log::Error("AUTH", "No matching account found in CSV");
    return false;
}

// Auto-detect accounts using GetAccountsByAccessTokenReq (2149)
bool FetchAccountsList(std::vector<long long>& accountIds) {
    accountIds.clear();

    char payload[2560];
    sprintf_s(payload, "\"accessToken\":\"%s\"", G.accessToken);

    const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                             PayloadType::GetAccountsByAccessTokenReq, payload);
    if (!WebSocket::Send(msg)) return false;

    char response[32768] = {0};
    ULONGLONG start = Utils::NowMs();
    while (Utils::NowMs() - start < (ULONGLONG)G.waitTime) {
        int n = WebSocket::Receive(response, sizeof(response));
        if (n > 0) {
            int pt = Protocol::ExtractPayloadType(response);
            if (pt == ToInt(PayloadType::GetAccountsByAccessTokenRes)) {
                // Parse ctidTraderAccount array
                const char* arr = Protocol::ExtractArray(response, "ctidTraderAccount");
                int count = Protocol::CountArrayElements(arr);
                for (int i = 0; i < count; i++) {
                    const char* elem = Protocol::GetArrayElement(arr, i);
                    long long aid = Protocol::ExtractInt64(elem, "ctidTraderAccountId");
                    bool isLive = Protocol::ExtractBool(elem, "isLive");

                    // Filter by environment
                    if ((G.env == Env::Live && isLive) || (G.env == Env::Demo && !isLive)) {
                        accountIds.push_back(aid);
                    }
                }
                Log::Info("AUTH", "Found %d accounts for env %s",
                          (int)accountIds.size(),
                          G.env == Env::Live ? "LIVE" : "DEMO");
                return true;
            }
            if (pt == ToInt(PayloadType::ErrorRes)) {
                const char* errCode = Protocol::ExtractString(response, "errorCode");
                char codeBuf[64] = {};
                if (errCode && *errCode) strcpy_s(codeBuf, errCode);
                const char* desc = Protocol::ExtractString(response, "description");
                Log::Error("AUTH", "FetchAccountsList error: code=%s desc=%s",
                          codeBuf, desc ? desc : "(null)");
                Log::Diag(1, "FetchAccountsList raw response: %.500s", response);
                return false;
            }
        }
        Sleep(10);
    }
    return false;
}

bool LoadToken() {
    // v3 compatible: read oauth_token.json
    char tokenPath[MAX_PATH];
    sprintf_s(tokenPath, "%soauth_token.json", G.dllDir);

    std::ifstream file(tokenPath);
    if (!file.is_open()) {
        Log::Info("AUTH", "No oauth_token.json found at %s", tokenPath);
        return false;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    file.close();

    std::string content = buf.str();
    if (content.empty()) return false;

    // Extract fields using Protocol helpers
    const char* at = Protocol::ExtractString(content.c_str(), "access_token");
    if (at && *at) {
        strcpy_s(G.accessToken, at);
    }

    const char* rt = Protocol::ExtractString(content.c_str(), "refresh_token");
    if (rt && *rt) {
        strcpy_s(G.refreshToken, rt);
    }

    // Load client_id and client_secret from token file ONLY if CSV didn't set them
    // CSV is the primary source for credentials, token file is fallback
    const char* cid = Protocol::ExtractString(content.c_str(), "client_id");
    if (cid && *cid && strlen(cid) > 5) {
        if (strlen(G.clientId) < 5) {
            strcpy_s(G.clientId, cid);
            Log::Info("AUTH", "Loaded client_id from token file (CSV had none)");
        } else {
            Log::Diag(1, "Token file client_id skipped (CSV already set)");
        }
    }

    const char* csec = Protocol::ExtractString(content.c_str(), "client_secret");
    if (csec && *csec && strlen(csec) > 5) {
        if (strlen(G.clientSecret) < 5) {
            strcpy_s(G.clientSecret, csec);
            Log::Info("AUTH", "Loaded client_secret from token file (CSV had none)");
        } else {
            Log::Diag(1, "Token file client_secret skipped (CSV already set)");
        }
    }

    if (strlen(G.accessToken) > 10) {
        Log::Info("AUTH", "Loaded token from oauth_token.json (token=%.20s...)", G.accessToken);
        return true;
    }

    return false;
}

void SaveToken() {
    // v3 compatible: write oauth_token.json
    char tokenPath[MAX_PATH];
    sprintf_s(tokenPath, "%soauth_token.json", G.dllDir);

    std::ofstream file(tokenPath);
    if (!file.is_open()) {
        Log::Error("AUTH", "Failed to save token to %s", tokenPath);
        return;
    }

    file << "{\n";
    file << "  \"access_token\": \"" << G.accessToken << "\",\n";
    file << "  \"refresh_token\": \"" << G.refreshToken << "\",\n";
    file << "  \"client_id\": \"" << G.clientId << "\",\n";
    file << "  \"client_secret\": \"" << G.clientSecret << "\"\n";
    file << "}\n";
    file.close();

    Log::Info("AUTH", "Token saved to oauth_token.json");
}

bool OAuthBrowserFlow() {
    // Use redirectUri from CSV (default: http://127.0.0.1:53123/callback)
    const std::string& rUri = G.redirectUri;
    if (rUri.empty()) {
        Log::Error("AUTH", "No redirect URI configured");
        return false;
    }

    // Extract port from redirectUri (e.g. "http://127.0.0.1:53123/callback" -> 53123)
    int listenPort = 53123; // default
    size_t colonPos = rUri.find(':', 7); // skip "http://"
    if (colonPos != std::string::npos) {
        listenPort = atoi(rUri.c_str() + colonPos + 1);
    }

    // Build OAuth URL (id.ctrader.com, NOT openapi.ctrader.com)
    char url[2048];
    sprintf_s(url, "https://id.ctrader.com/my/settings/openapi/grantingaccess/?client_id=%s&redirect_uri=%s&scope=trading&product=web",
              G.clientId, rUri.c_str());

    // Open browser
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    Log::Msg("Browser opened for OAuth login. Waiting for callback...");

    // Start simple HTTP listener (RAII WSA guard for Bug #14)
    struct WsaGuard {
        WsaGuard() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
        ~WsaGuard() { WSACleanup(); }
    } wsaGuard;

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        Log::Error("AUTH", "Failed to create listener socket");
        return false;
    }

    // Allow reuse
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)listenPort);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Log::Error("AUTH", "Failed to bind port %d", listenPort);
        closesocket(listenSock);
        return false;
    }

    listen(listenSock, 1);

    // Set timeout
    DWORD timeout = 120000; // 2 minutes
    setsockopt(listenSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    SOCKET clientSock = accept(listenSock, NULL, NULL);
    if (clientSock == INVALID_SOCKET) {
        Log::Error("AUTH", "OAuth callback timeout");
        closesocket(listenSock);
        return false;
    }

    char buf[4096] = {};
    recv(clientSock, buf, sizeof(buf) - 1, 0);

    // Extract code from GET /callback?code=XXXXX
    std::string request(buf);
    std::string code;
    size_t codePos = request.find("code=");
    if (codePos != std::string::npos) {
        codePos += 5;
        size_t end = request.find_first_of("& \r\n", codePos);
        code = request.substr(codePos, end - codePos);
    }

    // Send response to browser
    const char* htmlResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                               "<html><body><h2>Authorization successful!</h2>"
                               "<p>You can close this tab.</p></body></html>";
    send(clientSock, htmlResponse, (int)strlen(htmlResponse), 0);
    closesocket(clientSock);
    closesocket(listenSock);

    if (code.empty()) {
        Log::Error("AUTH", "No auth code received");
        return false;
    }

    Log::Info("AUTH", "Got auth code (%.10s...), exchanging for token...", code.c_str());

    // Clear old token so we detect failure properly
    G.accessToken[0] = '\0';
    G.refreshToken[0] = '\0';

    // Exchange code for token via HTTPS GET with query params
    // (cTrader API uses GET, not POST - see docs)
    HINTERNET hSession = WinHttpOpen(L"cTrader/4.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // Bug #16: Set HTTP timeouts
    DWORD connectTimeout = 10000, sendTimeout = 10000, receiveTimeout = 15000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &sendTimeout, sizeof(sendTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &receiveTimeout, sizeof(receiveTimeout));

    HINTERNET hConnect = WinHttpConnect(hSession, L"openapi.ctrader.com",
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    // Build GET URL with query params
    char pathBuf[4096];
    sprintf_s(pathBuf, "/apps/token?grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&client_secret=%s",
              code.c_str(), G.redirectUri.c_str(), G.clientId, G.clientSecret);

    // Convert to wide string for WinHTTP
    wchar_t wPath[4096];
    MultiByteToWideChar(CP_UTF8, 0, pathBuf, -1, wPath, 4096);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    // Bug #15: Read full response in loop
    char tokenBuf[8192] = {};
    DWORD totalRead = 0, bytesRead = 0;
    while (WinHttpReadData(hRequest, tokenBuf + totalRead,
                           sizeof(tokenBuf) - totalRead - 1, &bytesRead)) {
        if (bytesRead == 0) break;
        totalRead += bytesRead;
        if (totalRead >= sizeof(tokenBuf) - 1) break;
    }
    tokenBuf[totalRead] = '\0';

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    Log::Info("AUTH", "Token response (%d bytes): %.200s", totalRead, tokenBuf);

    // Check for error in response FIRST
    // Note: JSON null value means "no error" - must skip it!
    {
        const char* errCode = Protocol::ExtractString(tokenBuf, "errorCode");
        if (errCode && *errCode && strcmp(errCode, "null") != 0) {
            char codeBuf[128] = {};
            strcpy_s(codeBuf, errCode);
            const char* desc = Protocol::ExtractString(tokenBuf, "description");
            Log::Error("AUTH", "Token exchange failed: %s - %s", codeBuf, desc ? desc : "");
            return false;
        }
    }

    // Parse token response - careful: ExtractString uses static buffer!
    // Must copy each result before calling ExtractString again
    {
        const char* at = Protocol::ExtractString(tokenBuf, "accessToken");
        if (!at || !*at) at = Protocol::ExtractString(tokenBuf, "access_token");
        if (at && *at) strcpy_s(G.accessToken, at);
    }
    {
        const char* rt = Protocol::ExtractString(tokenBuf, "refreshToken");
        if (!rt || !*rt) rt = Protocol::ExtractString(tokenBuf, "refresh_token");
        if (rt && *rt) strcpy_s(G.refreshToken, rt);
    }

    if (strlen(G.accessToken) > 10) {
        SaveToken();
        Log::Info("AUTH", "Token obtained via OAuth (token=%.20s...)", G.accessToken);
        return true;
    }

    Log::Error("AUTH", "Failed to parse token response: %.200s", tokenBuf);
    return false;
}

bool RefreshAccessToken() {
    if (strlen(G.refreshToken) < 10) return false;

    Log::Info("AUTH", "Refreshing access token...");

    // Clear old token so we can detect failure
    char oldToken[2048];
    strcpy_s(oldToken, G.accessToken);
    G.accessToken[0] = '\0';

    // Use GET with query params (matching cTrader API docs)
    HINTERNET hSession = WinHttpOpen(L"cTrader/4.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { strcpy_s(G.accessToken, oldToken); return false; }

    // Bug #16: Set HTTP timeouts
    DWORD connectTimeout = 10000, sendTimeout = 10000, receiveTimeout = 15000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &sendTimeout, sizeof(sendTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &receiveTimeout, sizeof(receiveTimeout));

    HINTERNET hConnect = WinHttpConnect(hSession, L"openapi.ctrader.com",
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); strcpy_s(G.accessToken, oldToken); return false; }

    char pathBuf[4096];
    sprintf_s(pathBuf, "/apps/token?grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s",
              G.refreshToken, G.clientId, G.clientSecret);

    wchar_t wPath[4096];
    MultiByteToWideChar(CP_UTF8, 0, pathBuf, -1, wPath, 4096);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); strcpy_s(G.accessToken, oldToken); return false; }

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    // Bug #15: Read full response in loop
    char tokenBuf[8192] = {};
    DWORD totalRead = 0, bytesRead = 0;
    while (WinHttpReadData(hRequest, tokenBuf + totalRead,
                           sizeof(tokenBuf) - totalRead - 1, &bytesRead)) {
        if (bytesRead == 0) break;
        totalRead += bytesRead;
        if (totalRead >= sizeof(tokenBuf) - 1) break;
    }
    tokenBuf[totalRead] = '\0';

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    Log::Diag(1, "Refresh response (%d bytes): %.200s", totalRead, tokenBuf);

    // ExtractString uses static buffer - must copy before next call
    {
        const char* at = Protocol::ExtractString(tokenBuf, "accessToken");
        if (!at || !*at) at = Protocol::ExtractString(tokenBuf, "access_token");
        if (at && *at) strcpy_s(G.accessToken, at);
    }
    {
        const char* rt = Protocol::ExtractString(tokenBuf, "refreshToken");
        if (!rt || !*rt) rt = Protocol::ExtractString(tokenBuf, "refresh_token");
        if (rt && *rt) strcpy_s(G.refreshToken, rt);
    }

    if (strlen(G.accessToken) > 10) {
        SaveToken();
        Log::Info("AUTH", "Token refreshed (new token=%.20s...)", G.accessToken);
        return true;
    }

    // Refresh failed - restore old token
    Log::Error("AUTH", "Token refresh failed. Response: %.200s", tokenBuf);
    strcpy_s(G.accessToken, oldToken);
    return false;
}

bool Login(const char* user, const char* pwd, const char* type) {
    // === v3 compatible login flow ===
    // 1. DetectEnv
    // 2. CSV -> clientId, clientSecret, accountId, redirectUri
    // 3. LoadToken (saved oauth_token.json)
    // 4. If no token -> OAuth browser flow
    // 5. Connect WebSocket
    // 6. ApplicationAuth
    // 7. If accountId==0 -> FetchAccountsList (auto-detect)
    // 8. AccountAuth (with retry: refresh -> OAuth -> reconnect)

    // Step 1: Detect environment from Zorro Type parameter
    DetectEnv(type);

    // Step 2: Load credentials from CSV
    bool csvOk = LoadAccountsCsv(user, pwd);

    // Step 3: Load saved token (also loads client_id/secret from token file)
    bool hasToken = LoadToken();

    // If CSV failed and no token, we have nothing
    if (!csvOk && !hasToken) {
        Log::Error("AUTH", "No credentials: CSV not found and no saved token");
        return false;
    }

    // Must have clientId/clientSecret at this point (from CSV or token file)
    if (strlen(G.clientId) < 5 || strlen(G.clientSecret) < 5) {
        Log::Error("AUTH", "Missing clientId or clientSecret");
        return false;
    }

    // Step 4: If no token, do OAuth browser flow
    if (!hasToken || strlen(G.accessToken) < 10) {
        Log::Info("AUTH", "No saved token - starting OAuth browser flow...");
        if (!OAuthBrowserFlow()) {
            Log::Error("AUTH", "OAuth flow failed, cannot proceed without token");
            return false;
        }
    }

    Log::Info("AUTH", "Credentials ready: clientId=%.20s... accountId=%lld token=%.20s...",
              G.clientId, G.accountId, G.accessToken);

    // Step 5: Connect WebSocket
    const char* host = G.hostOverride.empty()
        ? (G.env == Env::Live ? CTRADER_HOST_LIVE : CTRADER_HOST_DEMO)
        : G.hostOverride.c_str();

    if (!WebSocket::Connect(host, CTRADER_WS_PORT)) {
        Log::Error("AUTH", "WebSocket connection failed to %s:%d", host, CTRADER_WS_PORT);
        return false;
    }

    // Step 6: Application auth
    if (!ApplicationAuth()) {
        WebSocket::Disconnect();
        return false;
    }

    // Step 7: Auto-detect accountId if not set (like v3)
    if (G.accountId == 0) {
        Log::Info("AUTH", "AccountId is 0 - auto-detecting via GetAccountsByAccessTokenReq...");
        std::vector<long long> accounts;
        if (FetchAccountsList(accounts) && !accounts.empty()) {
            G.accountId = accounts[0];
            Log::Info("AUTH", "Auto-detected accountId: %lld (from %d candidates)",
                      G.accountId, (int)accounts.size());
        } else {
            Log::Error("AUTH", "Failed to auto-detect account ID");
            WebSocket::Disconnect();
            return false;
        }
    }

    // Step 8: Account auth
    if (AccountAuth()) {
        G.loggedIn = true;
        Log::Info("AUTH", "Login successful (env=%s, account=%lld)",
                  G.env == Env::Live ? "LIVE" : "DEMO", G.accountId);
        return true;
    }

    // Account auth failed - get a fresh token and re-discover accounts
    Log::Info("AUTH", "Account auth failed (accountId=%lld may be invalid)", G.accountId);
    WebSocket::Disconnect();

    // Try refresh first, then OAuth
    bool gotNewToken = RefreshAccessToken();
    if (!gotNewToken) {
        Log::Info("AUTH", "Refresh failed, trying OAuth browser flow...");
        gotNewToken = OAuthBrowserFlow();
    }

    if (!gotNewToken) {
        Log::Error("AUTH", "Cannot get a valid token");
        return false;
    }

    // Reconnect with new token
    if (!WebSocket::Connect(host, CTRADER_WS_PORT)) {
        Log::Error("AUTH", "WebSocket reconnection failed");
        return false;
    }
    if (!ApplicationAuth()) {
        WebSocket::Disconnect();
        return false;
    }

    // Re-discover accounts with the fresh token (accountId from CSV may be stale)
    Log::Info("AUTH", "Re-discovering accounts with fresh token...");
    std::vector<long long> retryAccounts;
    if (FetchAccountsList(retryAccounts) && !retryAccounts.empty()) {
        G.accountId = retryAccounts[0];
        Log::Info("AUTH", "Re-discovered accountId: %lld (from %d candidates)",
                  G.accountId, (int)retryAccounts.size());
    }

    if (AccountAuth()) {
        G.loggedIn = true;
        Log::Info("AUTH", "Login successful after retry (env=%s, account=%lld)",
                  G.env == Env::Live ? "LIVE" : "DEMO", G.accountId);
        return true;
    }

    // Still failing - last resort: OAuth + auto-detect
    Log::Info("AUTH", "Still failing, trying full OAuth + auto-detect...");
    WebSocket::Disconnect();
    if (!OAuthBrowserFlow()) {
        Log::Error("AUTH", "OAuth flow failed");
        return false;
    }
    if (!WebSocket::Connect(host, CTRADER_WS_PORT) || !ApplicationAuth()) {
        WebSocket::Disconnect();
        Log::Error("AUTH", "Reconnect after OAuth failed");
        return false;
    }
    // Auto-detect again
    retryAccounts.clear();
    if (FetchAccountsList(retryAccounts) && !retryAccounts.empty()) {
        G.accountId = retryAccounts[0];
        Log::Info("AUTH", "OAuth re-discovered accountId: %lld", G.accountId);
    }
    if (AccountAuth()) {
        G.loggedIn = true;
        Log::Info("AUTH", "Login successful via OAuth (env=%s, account=%lld)",
                  G.env == Env::Live ? "LIVE" : "DEMO", G.accountId);
        return true;
    }

    Log::Error("AUTH", "All auth methods exhausted");
    WebSocket::Disconnect();
    return false;
}

} // namespace Auth
