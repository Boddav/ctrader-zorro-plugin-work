// ============================================================================
// main.cpp - TELJES IMPLEMENTÁCIÓ
// ============================================================================

#include "../stdafx.h"
#include "../zorro.h"

#include "../include/globals.h"
#include "../include/network.h"
#include "../include/auth.h"
#include "../include/trading.h"
#include "../include/account.h"
#include "../include/history.h"
#include "../include/http_api.h"
#include "../include/oauth_utils.h"
#include "../include/reconnect.h"
#include "../include/symbols.h"
#include "../include/utils.h"
#include "../include/csv_loader.h"


// Zorro callbacks
int(__cdecl* BrokerMessage)(const char* Text) = NULL;
int(__cdecl* BrokerProgress)(intptr_t Progress) = NULL;

// Global state
GLOBAL G;

// Forward declarations for symbols.cpp compatibility wrappers
void showMsg(const char* Text, const char* Detail = "");
void log_to_wesocket(const char* line1, const char* line2 = nullptr);
const char* get_msg_id();
bool body_indicates_error(const char* buffer);
bool tcp_send(const char* data);

// Wrapper implementations for symbols.cpp
void showMsg(const char* Text, const char* Detail) {
    Utils::ShowMsg(Text, Detail);
}

void log_to_wesocket(const char* line1, const char* line2) {
    Utils::LogToFile(line1, line2 ? line2 : "");
}

const char* get_msg_id() {
    return Utils::GetMsgId();
}

bool body_indicates_error(const char* buffer) {
    return Utils::BodyIndicatesError(buffer);
}

bool tcp_send(const char* data) {
    return Network::Send(data);
}

static const char* HostForEnv() {
    return (G.Env == CtraderEnv::Live) ? CTRADER_HOST_LIVE : CTRADER_HOST_DEMO;
}

static CtraderEnv DetectEnvFromType(const char* Type) {
    if (!Type) return CtraderEnv::Demo;
    if (Utils::StrContainsCaseInsensitive(Type, "live") ||
        Utils::StrContainsCaseInsensitive(Type, "real")) {
        return CtraderEnv::Live;
    }
    return CtraderEnv::Demo;
}

// ============================================================================
// NETWORK THREAD
// ============================================================================

unsigned __stdcall NetworkThread(void*) {
    char buffer[131072];

    while (G.bIsRunning) {
        ULONGLONG now = GetTickCount64();

        // Process symbol subscription retries every second
        if (now - G.lastRetryProcessMs > 1000) {
            Symbols::ProcessRetries(G.CTraderAccountId);
            G.lastRetryProcessMs = now;
        }

        int bytes = Network::Receive(buffer, sizeof(buffer));

        // Connection lost
        if (bytes < 0) {
            Utils::ShowMsg("Connection lost");
            if (G.HasLogin && Reconnect::Attempt()) {
                Utils::ShowMsg("Reconnected");
                continue;
            }
            G.bIsRunning = false;
            break;
        }

        // No data, send heartbeat if needed
        if (bytes == 0) {
            if (now - G.lastPingMs >= PING_INTERVAL_MS) {
                char ping[128];
                sprintf_s(ping, "{\"payloadType\":2106,\"payload\":{}}");
                Network::Send(ping);
                G.lastPingMs = now;
            }
            continue;
        }

        // Got data, update ping time
        G.lastPingMs = GetTickCount64();

        // Parse payload type
        const char* pType = strstr(buffer, "\"payloadType\":");
        int payloadType = 0;
        if (pType) {
            sscanf_s(pType, "\"payloadType\":%d", &payloadType);
        }

        // Handle subscription response (2122)
        if (payloadType == 2122) {
            char clientMsgId[64] = {0};
            const char* pId = strstr(buffer, "\"clientMsgId\":\"");
            if (pId) {
                sscanf_s(pId, "\"clientMsgId\":\"%63[^\"]\"", clientMsgId,
                        (unsigned)_countof(clientMsgId));
            }

            bool success = !Utils::BodyIndicatesError(buffer);
            std::string error_details = "";

            if (!success) {
                const char* pDesc = strstr(buffer, "\"description\":\"");
                if (pDesc) {
                    char desc[256] = {0};
                    sscanf_s(pDesc, "\"description\":\"%255[^\"]\"", desc,
                            (unsigned)_countof(desc));
                    error_details = desc;
                }
            }

            Symbols::HandleSubscriptionResponse(clientMsgId, success, error_details);
        }
        // Handle spot event (2126) - quotes or executions
        else if (payloadType == 2126) {
            // Check if this is a trade execution
            if (strstr(buffer, "\"executionType\"")) {
                char clientMsgId[64] = {0};
                const char* pId = strstr(buffer, "\"clientMsgId\":\"");
                if (pId) {
                    sscanf_s(pId, "\"clientMsgId\":\"%63[^\"]\"", clientMsgId,
                            (unsigned)_countof(clientMsgId));
                }

                EnterCriticalSection(&G.cs_trades);

                auto it = G.pendingTrades.find(clientMsgId);
                if (it != G.pendingTrades.end()) {
                    int zorroId = it->second;

                    const char* pExec = strstr(buffer, "\"executionType\":\"");
                    if (pExec && (strncmp(pExec + 17, "ORDER_ACCEPTED", 14) == 0 ||
                                  strncmp(pExec + 17, "ORDER_FILLED", 12) == 0)) {

                        Trade newTrade = {};
                        newTrade.zorroId = zorroId;

                        // Extract position ID
                        const char* pPosId = strstr(buffer, "\"positionId\":");
                        if (pPosId) {
                            sscanf_s(pPosId, "\"positionId\":%lld", &newTrade.ctid);
                        }

                        // Extract execution price
                        const char* pPrice = strstr(buffer, "\"executionPrice\":");
                        if (pPrice) {
                            sscanf_s(pPrice, "\"executionPrice\":%lf", &newTrade.openPrice);
                        }

                        // Extract volume
                        const char* pVol = strstr(buffer, "\"executedVolume\":");
                        int vol = 0;
                        if (pVol) {
                            sscanf_s(pVol, "\"executedVolume\":%d", &vol);
                        }

                        // Get symbol and side from pending info
                        auto pit = G.pendingTradeInfo.find(zorroId);
                        if (pit != G.pendingTradeInfo.end()) {
                            newTrade.symbol = pit->second.first;
                            int side = pit->second.second;
                            newTrade.amount = side * (vol != 0 ? vol : 1);
                            G.pendingTradeInfo.erase(pit);
                        } else {
                            newTrade.amount = vol;
                        }

                        G.openTrades[zorroId] = newTrade;
                        G.ctidToZorroId[newTrade.ctid] = zorroId;
                    }

                    G.pendingTrades.erase(it);
                }

                LeaveCriticalSection(&G.cs_trades);
            }
            // Quote update
            else {
                long long symbolId = 0;
                const char* pSymId = strstr(buffer, "\"symbolId\":");
                if (pSymId) {
                    sscanf_s(pSymId, "\"symbolId\":%lld", &symbolId);
                }

                if (symbolId > 0) {
                    long long bid = 0, ask = 0;

                    const char* pBid = strstr(buffer, "\"bid\":");
                    if (pBid) {
                        sscanf_s(pBid, "\"bid\":%lld", &bid);
                    }

                    const char* pAsk = strstr(buffer, "\"ask\":");
                    if (pAsk) {
                        sscanf_s(pAsk, "\"ask\":%lld", &ask);
                    }

                    Symbols::UpdateQuote(symbolId, bid, ask);
                }
            }

            // Check for position close
            if (strstr(buffer, "\"positionStatus\":\"POSITION_CLOSED\"")) {
                const char* pPosId = strstr(buffer, "\"positionId\":");
                long long pid = 0;
                if (pPosId) {
                    sscanf_s(pPosId, "\"positionId\":%lld", &pid);
                }

                double profit = 0.0;
                const char* pProfit = strstr(buffer, "\"profit\":");
                if (pProfit) {
                    sscanf_s(pProfit, "\"profit\":%lf", &profit);
                }

                EnterCriticalSection(&G.cs_trades);
                auto zit = G.ctidToZorroId.find(pid);
                if (zit != G.ctidToZorroId.end()) {
                    Trade& tr = G.openTrades[zit->second];
                    tr.closed = true;
                    tr.profit = profit;
                    tr.closePrice = 0;
                }
                LeaveCriticalSection(&G.cs_trades);
            }
        }
        // Historical data response (2113)
        else if (payloadType == 2113) {
            History::ProcessHistoricalResponse(buffer);
        }
        // Account trader info response (2122)
        else if (payloadType == 2122) {
            Utils::LogToFile("ACCOUNT_RESPONSE", buffer);

            // Parse trader info for account data
            const char* payload = strstr(buffer, "\"payload\":");
            if (payload) {
                // Extract balance, equity, margin from trader object
                const char* trader = strstr(payload, "\"trader\":");
                if (trader) {
                    double balance = 0, equity = 0, margin = 0;
                    const char* currency = "USD";

                    // Parse balance
                    const char* pBalance = strstr(trader, "\"balance\":");
                    if (pBalance) sscanf_s(pBalance, "\"balance\":%lf", &balance);

                    // Parse equity
                    const char* pEquity = strstr(trader, "\"equity\":");
                    if (pEquity) sscanf_s(pEquity, "\"equity\":%lf", &equity);

                    // Parse margin
                    const char* pMargin = strstr(trader, "\"usedMargin\":");
                    if (pMargin) sscanf_s(pMargin, "\"usedMargin\":%lf", &margin);

                    // Parse currency
                    const char* pCurrency = strstr(trader, "\"depositCurrency\":\"");
                    if (pCurrency) {
                        char currencyBuf[8] = {0};
                        sscanf_s(pCurrency, "\"depositCurrency\":\"%7[^\"]\"", currencyBuf, (unsigned)sizeof(currencyBuf));
                        currency = currencyBuf;
                    }

                    Account::UpdateAccountInfo(balance, equity, margin, currency);

                    char msg[256];
                    sprintf_s(msg, "Account info updated: Balance=%.2f, Equity=%.2f, Margin=%.2f %s",
                             balance, equity, margin, currency);
                    Utils::LogToFile("ACCOUNT_UPDATE", msg);
                }
            }
        }
        // Heartbeat response (2106)
        else if (payloadType == 2106) {
            char pong[64];
            sprintf_s(pong, "{\"payloadType\":2106,\"payload\":{}}");
            Network::Send(pong);
        }
    }

    return 0;
}

// ============================================================================
// BROKER API IMPLEMENTATIONS
// ============================================================================

DLLFUNC int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress) {
    strcpy_s(Name, 32, PLUGIN_NAME);
    BrokerMessage = (int(__cdecl*)(const char*))fpError;
    BrokerProgress = (int(__cdecl*)(intptr_t))fpProgress;
    return PLUGIN_TYPE;
}

DLLFUNC int BrokerLogin(char* User, char* Pwd, char* Type, char* Accounts) {
    // Stop existing connection if any
    if (G.bIsRunning) {
        G.bIsRunning = false;
        if (G.hNetworkThread) {
            WaitForSingleObject(G.hNetworkThread, 2000);
            CloseHandle(G.hNetworkThread);
            G.hNetworkThread = NULL;
        }
    }

    Network::Disconnect();

    // Selective clearing instead of ZeroMemory to preserve C++ objects
    ZeroMemory(G.Token, sizeof(G.Token));
    ZeroMemory(G.RefreshToken, sizeof(G.RefreshToken));
    G.CTraderAccountId = 0;

    // Initialize WebSocket handles
    G.hSession = NULL;
    G.hConnect = NULL;
    G.hWebSocket = NULL;
    G.wsConnected = false;
    G.Symbols.clear();
    G.pendingTrades.clear();
    G.openTrades.clear();
    G.ctidToZorroId.clear();
    G.pendingTradeInfo.clear();
    G.nextTradeId = 1;
    G.lastPingMs = GetTickCount64();

    Symbols::Cleanup();
    Symbols::Initialize();

    // Try to load from CSV
    CsvCreds cc;
    if (CsvLoader::LoadAccountsCsv(cc, User, Type)) {
        strncpy_s(G.ClientId, sizeof(G.ClientId), cc.clientId.c_str(), _TRUNCATE);
        strncpy_s(G.ClientSecret, sizeof(G.ClientSecret), cc.clientSecret.c_str(), _TRUNCATE);
        G.CTraderAccountId = _strtoui64(cc.accountId.c_str(), NULL, 10);
        G.Env = DetectEnvFromType(cc.type.c_str());

        if (!cc.accessToken.empty()) {
            strncpy_s(G.Token, sizeof(G.Token), cc.accessToken.c_str(), _TRUNCATE);
        }
    } else {
        // Fallback to parameters
        strncpy_s(G.ClientId, sizeof(G.ClientId), User ? User : "", _TRUNCATE);
        strncpy_s(G.ClientSecret, sizeof(G.ClientSecret), Pwd ? Pwd : "", _TRUNCATE);
        G.Env = DetectEnvFromType(Type);

        if (Accounts && *Accounts) {
            G.CTraderAccountId = _strtoui64(Accounts, NULL, 10);
        }
    }

    G.HasLogin = true;

    // Try to load token from disk
    if (G.Token[0] == '\0') {
        if (Auth::LoadTokenFromDisk()) {
            Utils::ShowMsg("Token loaded from disk");
        }
    }

    // If still no token, perform OAuth flow
    if (G.Token[0] == '\0') {
        Utils::ShowMsg("No token found, starting OAuth flow...");

        if (!G.ClientId[0] || !G.ClientSecret[0]) {
            Utils::ShowMsg("Error: Client ID/Secret missing");
            return 0;
        }

        if (!OAuth::PerformInteractiveFlow()) {
            Utils::ShowMsg("OAuth flow failed");
            return 0;
        }
    }

    // Connect to cTrader
    const char* host = HostForEnv();
    char msg[256];
    sprintf_s(msg, "Connecting to: %s", host);
    Utils::ShowMsg(msg);

    if (!Network::Connect(host, "5036")) {
        Utils::ShowMsg("Connection failed");
        return 0;
    }

    // Application authentication (payloadType 2100)
    char request[1024] = {0};
    char response[131072] = {0};

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":2100,\"payload\":{\"clientId\":\"%s\",\"clientSecret\":\"%s\"}}",
        Utils::GetMsgId(), G.ClientId, G.ClientSecret);

    if (!Network::Send(request)) {
        Utils::ShowMsg("App auth send failed");
        return 0;
    }

    if (Network::Receive(response, sizeof(response)) <= 0) {
        Utils::ShowMsg("App auth response failed");
        return 0;
    }

    if (!strstr(response, "\"payloadType\":2101")) {
        Utils::ShowMsg("App auth failed, trying refresh token...");
        Utils::LogToFile("APP_AUTH_RESPONSE", response);

        // Try to refresh the access token
        if (Auth::RefreshAccessToken()) {
            Utils::ShowMsg("Token refreshed, retrying app auth...");

            // Retry app authentication with new token
            sprintf_s(request,
                "{\"clientMsgId\":\"%s\",\"payloadType\":2100,\"payload\":{\"clientId\":\"%s\",\"clientSecret\":\"%s\"}}",
                Utils::GetMsgId(), G.ClientId, G.ClientSecret);

            if (!Network::Send(request)) {
                Utils::ShowMsg("App auth retry send failed");
                return 0;
            }

            if (Network::Receive(response, sizeof(response)) <= 0) {
                Utils::ShowMsg("App auth retry response failed");
                return 0;
            }

            if (!strstr(response, "\"payloadType\":2101")) {
                Utils::ShowMsg("App auth failed even after token refresh");
                Utils::LogToFile("APP_AUTH_RETRY_RESPONSE", response);
                return 0;
            }

            Utils::ShowMsg("App auth succeeded after token refresh");
        } else {
            Utils::ShowMsg("Token refresh failed, giving up");
            return 0;
        }
    }

    Utils::ShowMsg("App authenticated");

    // Auto-detect account if not specified
    if (G.CTraderAccountId == 0) {
        Utils::ShowMsg("Auto-detecting account...");
        std::vector<long long> accountIds;

        if (Auth::FetchAccountsList(accountIds) && !accountIds.empty()) {
            G.CTraderAccountId = accountIds[0];
            char accMsg[128];
            sprintf_s(accMsg, "Account detected: %lld", G.CTraderAccountId);
            Utils::ShowMsg(accMsg);
        } else {
            Utils::ShowMsg("Account detection failed");
            return 0;
        }
    }

    // Advanced account authentication with retry logic from kiindulasi2.txt
    auto try_account_auth = [&](long long acctId)->bool{
        char locRes[131072] = {0};
        char req[512] = {0};
        sprintf_s(req, "{\"clientMsgId\":\"%s\",\"payloadType\":2102,\"payload\":{\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld}}",
                 Utils::GetMsgId(), G.Token, acctId);

        Utils::LogToFile("AUTH", "Trying AccountAuth (2102) for:");

        if (!Network::Send(req)) return false;
        int r = Network::Receive(locRes, sizeof(locRes));
        if (r <= 0) return false;

        Utils::LogToFile("AUTH", "AccountAuth response received");
        Utils::LogToFile("AUTH_RESPONSE", locRes);

        if (strstr(locRes, "\"payloadType\":2103")) {
            Utils::LogToFile("AUTH", "Found payloadType 2103 - SUCCESS");
            return true;
        } else {
            Utils::LogToFile("AUTH", "PayloadType 2103 NOT found - FAILED");
            return false;
        }
    };

    bool auth_result = try_account_auth(G.CTraderAccountId);
    char result_msg[256];
    sprintf_s(result_msg, "try_account_auth returned: %s", auth_result ? "TRUE" : "FALSE");
    Utils::LogToFile("AUTH", result_msg);

    bool skip_secondary_auth = false;
    if (auth_result) {
        Utils::LogToFile("AUTH", "Authentication successful, skipping secondary auth");
        skip_secondary_auth = true;
    }

    if (!auth_result && !skip_secondary_auth) {
        Utils::ShowMsg("Account auth failed, fetching account list...");
        Utils::LogToFile("AUTH", "Account auth failed with provided ID, fetching list and retrying...");

        long long aid = 0;
        std::vector<long long> accountIds;

        if (Auth::FetchAccountsList(accountIds) && !accountIds.empty()) {
            aid = accountIds[0];
            if (aid != G.CTraderAccountId && try_account_auth(aid)) {
                G.CTraderAccountId = aid;
            } else {
                // Try all account IDs from the list
                Utils::LogToFile("AUTH", "Retrying AccountAuth over all IDs from 2104 list...");
                bool found = false;
                for (long long id : accountIds) {
                    if (id > 0) {
                        char msg[128];
                        sprintf_s(msg, "Trying accountId: %lld", id);
                        Utils::LogToFile("AUTH", msg);
                        if (try_account_auth(id)) {
                            G.CTraderAccountId = id;
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    Utils::LogToFile("AUTH", "Account auth still failed, attempting refresh token...");
                    if (G.CTraderAccountId == 0 || !strstr(response, "\"payloadType\":2103")) {
                        Utils::ShowMsg("Account auth failed, trying refresh_token...");
                        if (Auth::RefreshAccessToken()) {
                            if (!try_account_auth(G.CTraderAccountId)) {
                                Utils::ShowMsg("Error: Account auth failed after refresh.");
                                Utils::LogToFile("AUTH", "Account auth failed after refresh token.");
                                return 0;
                            }
                        } else {
                            Utils::ShowMsg("Refresh token not available or failed.");
                            Utils::LogToFile("AUTH", "Refresh token not available or failed.");
                            return 0;
                        }
                    }
                }
            }
        }
    }

    char authMsg[128];
    sprintf_s(authMsg, "Account auth OK. Using accountId: %lld", G.CTraderAccountId);
    Utils::LogToFile("AUTH", authMsg);

    // Account authentication legacy helper for compatibility
    auto TryAccountAuth = [&](long long acctId)->bool {
        char locReq[1024] = {0};
        char locRes[131072] = {0};

        sprintf_s(locReq,
            "{\"clientMsgId\":\"%s\",\"payloadType\":2102,\"payload\":{\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld}}",
            Utils::GetMsgId(), G.Token, acctId);

        Utils::LogToFile("AUTH", "TryAccountAuth: Sending request");
        if (!Network::Send(locReq)) {
            Utils::LogToFile("AUTH", "TryAccountAuth: Send failed");
            return false;
        }

        if (Network::Receive(locRes, sizeof(locRes)) <= 0) {
            Utils::LogToFile("AUTH", "TryAccountAuth: Receive failed");
            return false;
        }

        Utils::LogToFile("AUTH", "TryAccountAuth: Response received");
        Utils::LogToFile("AUTH_RESPONSE", locRes);

        if (!strstr(locRes, "\"payloadType\":2103")) {
            Utils::LogToFile("AUTH", "TryAccountAuth: PayloadType 2103 NOT found - FAILED");
            return false;
        }

        Utils::LogToFile("AUTH", "TryAccountAuth: Found payloadType 2103 - SUCCESS");

        const char* pAcct = strstr(locRes, "\"ctidTraderAccountId\":");
        long long parsed = 0;
        if (pAcct) {
            sscanf_s(pAcct, "\"ctidTraderAccountId\":%lld", &parsed);
        }
        if (parsed > 0) {
            G.CTraderAccountId = parsed;
        }

        return true;
    };

    // Try account auth, with retry logic (only if primary auth failed)
    if (!skip_secondary_auth && !TryAccountAuth(G.CTraderAccountId)) {
        Utils::ShowMsg("Account auth failed, trying account list and refresh...");

        // Try to get fresh account list and retry with all accounts
        std::vector<long long> accountIds;
        if (Auth::FetchAccountsList(accountIds)) {
            bool authSuccess = false;
            for (long long aid : accountIds) {
                if (TryAccountAuth(aid)) {
                    G.CTraderAccountId = aid;
                    authSuccess = true;
                    char msg[128];
                    sprintf_s(msg, "Authenticated with account: %lld", aid);
                    Utils::ShowMsg(msg);
                    break;
                }
            }

            if (!authSuccess) {
                Utils::ShowMsg("Account auth failed, trying token refresh...");
                if (Auth::RefreshAccessToken()) {
                    for (long long aid : accountIds) {
                        if (TryAccountAuth(aid)) {
                            G.CTraderAccountId = aid;
                            authSuccess = true;
                            Utils::ShowMsg("Authenticated with refreshed token");
                            break;
                        }
                    }
                }
            }

            if (!authSuccess) {
                Utils::ShowMsg("Error: Account auth failed after all attempts");
                return 0;
            }
        } else {
            // Fallback to simple refresh
            if (Auth::RefreshAccessToken() && TryAccountAuth(G.CTraderAccountId)) {
                Utils::ShowMsg("Authenticated with refreshed token");
            } else {
                Utils::ShowMsg("Error: Account auth failed");
                return 0;
            }
        }
    } else {
        Utils::ShowMsg("Account authenticated");
    }

    // Fetch symbols (payloadType 2114)
    Utils::ShowMsg("Fetching symbols...");

    ZeroMemory(request, sizeof(request));
    ZeroMemory(response, sizeof(response));

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":2114,\"payload\":{\"ctidTraderAccountId\":%lld}}",
        Utils::GetMsgId(), G.CTraderAccountId);

    if (!Network::Send(request)) {
        Utils::ShowMsg("Symbol request failed");
        return 0;
    }

    if (Network::Receive(response, sizeof(response)) <= 0) {
        Utils::ShowMsg("Symbol response failed");
        return 0;
    }

    if (!strstr(response, "\"payloadType\":2115")) {
        Utils::ShowMsg("Symbol fetch failed");
        return 0;
    }

    Utils::LogToFile("SYMBOLS_RESPONSE", response);

    // Parse symbols - match working version pattern
    const char* pSymbol = strstr(response, "\"symbol\":[");
    if (!pSymbol) pSymbol = strstr(response, "\"symbols\":[");

    Utils::LogToFile("SYMBOLS_DEBUG", pSymbol ? "Found symbol array" : "Symbol array NOT found");

    int count = 0;
    if (pSymbol) {
        pSymbol += strlen(pSymbol == strstr(response, "\"symbol\":[") ?
                         "\"symbol\":[" : "\"symbols\":[");
        const char* pEnd = strstr(pSymbol, "]");

        char debug_pend[128];
        sprintf_s(debug_pend, "pEnd pointer: %s", pEnd ? "FOUND" : "NULL");
        Utils::LogToFile("SYMBOLS_DEBUG", debug_pend);

        if (pEnd) {
            std::string all_symbols(pSymbol, pEnd - pSymbol);
            const char* current = all_symbols.c_str();

            char debug_len[128];
            sprintf_s(debug_len, "Symbol array length: %zu", all_symbols.length());
            Utils::LogToFile("SYMBOLS_DEBUG", debug_len);

            while ((current = strstr(current, "{"))) {
                Utils::LogToFile("SYMBOLS_DEBUG", "Found opening brace, starting parse...");
                long long id = 0;
                int digits = 5;
                char symbolName[64] = {0};

                const char* pId = strstr(current, "\"symbolId\":");
                if (pId) {
                    sscanf_s(pId, "\"symbolId\":%lld", &id);
                }

                const char* pName = strstr(current, "\"symbolName\":\"");
                if (pName) {
                    sscanf_s(pName, "\"symbolName\":\"%63[^\"]\"", symbolName,
                            (unsigned)_countof(symbolName));
                }

                const char* pDigits = strstr(current, "\"digits\":");
                if (pDigits) {
                    sscanf_s(pDigits, "\"digits\":%d", &digits);
                }

                char debug_msg[256];
                sprintf_s(debug_msg, "Parsed: id=%lld, name=%s, digits=%d", id, symbolName, digits);
                Utils::LogToFile("SYMBOLS_DEBUG", debug_msg);

                if (id > 0 && strlen(symbolName) > 0) {
                    Symbols::AddSymbol(symbolName, id, digits);
                    count++;
                    Utils::LogToFile("SYMBOLS_DEBUG", "Symbol added successfully");
                } else {
                    Utils::LogToFile("SYMBOLS_DEBUG", "Symbol skipped (invalid id or name)");
                }

                // Move past the current object
                const char* next_brace = strstr(current + 1, "{");
                const char* next_comma = strstr(current + 1, ",");
                if (next_brace && (!next_comma || next_brace < next_comma)) {
                    current = next_brace;
                } else if (next_comma) {
                    current = next_comma + 1;
                } else {
                    break; // No more objects
                }
            }
        }
    }

    sprintf_s(msg, "Loaded %d symbols", count);
    Utils::ShowMsg(msg);

    // Generate AssetList file
    Symbols::GenerateBrokerAssetsFile(G.DllPath);

    // Start network thread
    G.bIsRunning = true;
    G.lastPingMs = GetTickCount64();
    G.lastRetryProcessMs = GetTickCount64();

    G.hNetworkThread = (HANDLE)_beginthreadex(NULL, 0, &NetworkThread, NULL, 0, NULL);
    if (!G.hNetworkThread) {
        G.bIsRunning = false;
        Utils::ShowMsg("Thread creation failed");
        return 0;
    }

    Utils::ShowMsg("Login successful");
    return 1;
}

DLLFUNC void BrokerClose() {
    if (G.bIsRunning) {
        G.bIsRunning = false;
        if (G.hNetworkThread) {
            WaitForSingleObject(G.hNetworkThread, 2000);
            CloseHandle(G.hNetworkThread);
            G.hNetworkThread = NULL;
        }
    }
    Network::Disconnect();
}

DLLFUNC int BrokerBuy2(char* Symbol, int nAmount, double dStopDist, double dLimit,
                       double* pPrice, int* pFill) {
    return Trading::PlaceOrder(Symbol, nAmount, dStopDist, dLimit, pPrice, pFill);
}

DLLFUNC int BrokerBuy(char* Symbol, int nAmount, double dStopDist, double* pPrice) {
    return BrokerBuy2(Symbol, nAmount, dStopDist, 0, pPrice, nullptr);
}

DLLFUNC int BrokerAsset(char* Symbol, double* pPrice, double* pSpread,
                        double* pVolume, double* pPip, double* pPipCost,
                        double* pLotAmount, double* pMarginCost,
                        double* pRollLong, double* pRollShort) {
    if (!Symbol) {
        Utils::LogToFile("BROKERASSET", "Symbol is NULL");
        return 0;
    }

    char msg[256];
    sprintf_s(msg, "BrokerAsset called for: '%s'", Symbol);
    Utils::LogToFile("BROKERASSET", msg);

    SymbolInfo* info = Symbols::GetSymbol(Symbol);
    if (!info) {
        sprintf_s(msg, "BrokerAsset: Symbol '%s' not found via GetSymbol", Symbol);
        Utils::LogToFile("BROKERASSET", msg);

        // Try with GetSymbolByIdOrName as fallback
        info = Symbols::GetSymbolByIdOrName(Symbol);
        if (!info) {
            sprintf_s(msg, "BrokerAsset: Symbol '%s' not found via GetSymbolByIdOrName either", Symbol);
            Utils::LogToFile("BROKERASSET", msg);
            return 0;
        } else {
            sprintf_s(msg, "BrokerAsset: Symbol '%s' found via GetSymbolByIdOrName fallback", Symbol);
            Utils::LogToFile("BROKERASSET", msg);
        }
    } else {
        sprintf_s(msg, "BrokerAsset: Symbol '%s' found via GetSymbol", Symbol);
        Utils::LogToFile("BROKERASSET", msg);
    }

    // Ensure subscribed to get quotes
    Symbols::EnsureSubscribed(G.CTraderAccountId, Symbol);

    Symbols::Lock();

    double d_bid = 0, d_ask = 0;
    if (info->bid > 0 && info->ask > 0) {
        d_bid = (double)info->bid / pow(10, info->digits);
        d_ask = (double)info->ask / pow(10, info->digits);
    }
    int digits = info->digits;

    Symbols::Unlock();

    if (pPrice) {
        *pPrice = (d_bid > 0 && d_ask > 0) ? (d_bid + d_ask) / 2.0 : 0;
    }

    if (pSpread) {
        *pSpread = (d_bid > 0 && d_ask > 0) ? (d_ask - d_bid) : 0;
    }

    if (pPip) {
        *pPip = 1.0 / pow(10, digits);
    }

    if (pVolume) *pVolume = 0;
    if (pPipCost) *pPipCost = 0;
    if (pLotAmount) *pLotAmount = 0;
    if (pMarginCost) *pMarginCost = 0;
    if (pRollLong) *pRollLong = 0;
    if (pRollShort) *pRollShort = 0;

    return 1;
}

DLLFUNC int BrokerTime(DATE* pTime) {
    if (pTime) {
        SYSTEMTIME st;
        GetSystemTime(&st);
        SystemTimeToVariantTime(&st, pTime);
    }
    return 1;
}

DLLFUNC int BrokerSell(int nTradeID, int nAmount) {
    return Trading::ClosePosition(nTradeID, nAmount);
}

DLLFUNC int BrokerSell2(int nTradeID, int nAmount, double Limit) {
    return BrokerSell(nTradeID, nAmount);
}

DLLFUNC int BrokerTrade(int nTradeID, double* pOpen, double* pClose,
                        double* pRoll, double* pProfit) {
    return Trading::GetTradeInfo(nTradeID, pOpen, pClose, pRoll, pProfit);
}

DLLFUNC int BrokerAccount(char* Account, double* pBalance, double* pTradeVal,
                         double* pMarginVal) {
    double balance, equity, margin, freeMargin;
    char currency[32];

    if (Account::GetAccountData(&balance, &equity, &margin, &freeMargin, currency)) {
        if (pBalance) *pBalance = balance;
        if (pTradeVal) *pTradeVal = equity - balance; // Unrealized P&L
        if (pMarginVal) *pMarginVal = margin;
        if (Account) sprintf_s(Account, 1024, "cTrader-%s", currency);
    } else {
        // Fallback to default values if no account data available
        if (pBalance) *pBalance = 10000.0;
        if (pTradeVal) *pTradeVal = 0.0;
        if (pMarginVal) *pMarginVal = 0.0;
        if (Account) strcpy_s(Account, 1024, "cTrader");
    }
    return 1;
}

DLLFUNC int BrokerAccount2(char* Account, double* pBalance, double* pTradeVal,
                          double* pMargin, double* pMarginVal, double* pEquity) {
    double balance, equity, margin, freeMargin;
    char currency[32];

    if (Account::GetAccountData(&balance, &equity, &margin, &freeMargin, currency)) {
        if (pBalance) *pBalance = balance;
        if (pTradeVal) *pTradeVal = equity - balance; // Unrealized P&L
        if (pMargin) *pMargin = freeMargin;
        if (pMarginVal) *pMarginVal = margin;
        if (pEquity) *pEquity = equity;
        if (Account) sprintf_s(Account, 1024, "cTrader-%s", currency);
    } else {
        // Fallback to default values
        if (pBalance) *pBalance = 10000.0;
        if (pTradeVal) *pTradeVal = 0.0;
        if (pMargin) *pMargin = 0.0;
        if (pMarginVal) *pMarginVal = 0.0;
        if (pEquity) *pEquity = 10000.0;
        if (Account) strcpy_s(Account, 1024, "cTrader");
    }
    return 1;
}

DLLFUNC double BrokerCommand(int Mode, intptr_t p) {
    if (Mode == 138) { // SET_DIAGNOSTICS
        G.Diag = (int)p;
    }
    return 0;
}

DLLFUNC char* BrokerRequest(const char* Url, const char* Data, const char* Headers) {
    if (!Url) return NULL;

    static std::string lastResponse;
    lastResponse = HttpApi::HttpRequest(Url, Data, Headers);

    // Return pointer to static string (Zorro will copy it)
    return lastResponse.empty() ? NULL : (char*)lastResponse.c_str();
}

DLLFUNC int BrokerLogout(void) {
    return 1;
}

DLLFUNC int BrokerHistory(char* Symbol, DATE tStart, DATE tEnd,
                         int nTickMinutes, int nTicks, T6* ticks) {
    if (!Symbol || !ticks) return 0;

    char msg[256];
    sprintf_s(msg, "BrokerHistory request: %s, %d minutes, %d ticks", Symbol, nTickMinutes, nTicks);
    Utils::LogToFile("HISTORY_REQUEST", msg);

    // RequestHistoricalData now waits synchronously for the response
    if (History::RequestHistoricalData(Symbol, tStart, tEnd, nTickMinutes, nTicks, ticks)) {
        Utils::LogToFile("HISTORY", "Historical data retrieved successfully");
        // Return a reasonable number - in practice this should be the actual count
        return nTicks; // The ticks array has been populated by ProcessHistoricalResponse
    }

    Utils::LogToFile("HISTORY", "Historical data request failed");
    return 0;
}

DLLFUNC int BrokerHistory2(char* Symbol, DATE tStart, DATE tEnd,
                          int nTickMinutes, int nTicks, T6* ticks) {
    return BrokerHistory(Symbol, tStart, tEnd, nTickMinutes, nTicks, ticks);
}

// ============================================================================
// DLL ENTRY POINT
// ============================================================================

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        Symbols::Initialize();

        InitializeCriticalSection(&G.cs_trades);
        InitializeCriticalSection(&G.cs_log);

        // Initialize history module critical section
        InitializeCriticalSection(&History::g_cs_history);

        GetModuleFileNameA((HMODULE)hModule, G.DllPath, MAX_PATH);
        char* pLastSlash = strrchr(G.DllPath, '\\');
        if (pLastSlash) {
            *(pLastSlash + 1) = '\0';
        }

        strcpy_s(G.LogPath, G.DllPath);
        strcat_s(G.LogPath, "ctrader.log");

        strcpy_s(G.Scope, "trading");
        strcpy_s(G.Product, "web");

        // Initialize WebSocket handles
        G.hSession = NULL;
        G.hConnect = NULL;
        G.hWebSocket = NULL;
        G.wsConnected = false;
        G.HasLogin = false;

        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        BrokerClose();
        Symbols::Cleanup();

        DeleteCriticalSection(&G.cs_trades);
        DeleteCriticalSection(&G.cs_log);

        WSACleanup();
    }

    return TRUE;
}