// ============================================================================
// main.cpp - TELJES IMPLEMENTÁCIÓ
// ============================================================================

#include "../stdafx.h"
#include "../zorro.h"
#include "../include/globals.h"

// Global state
GLOBAL G;
#include "../include/network.h"
#include "../include/auth.h"
#include "../include/trading.h"
#include "../include/account.h"
#include "../include/history.h"
#include "../include/history_rest.h"
#include "../include/http_api.h"
#include "../include/oauth_utils.h"
#include "../include/reconnect.h"
#include "../include/symbols.h"
#include "../include/utils.h"
#include "../include/csv_loader.h"


// Zorro callbacks
int(__cdecl* BrokerMessage)(const char* Text) = NULL;
int(__cdecl* BrokerProgress)(intptr_t Progress) = NULL;
static void PerformLogout(const char* contextTag, bool notifyHost) {
    bool wasGuardActive = G.stopGuardActive;
    if (contextTag) {
        Utils::LogToFile("LOGOUT_SEQ", contextTag);
    }

    G.userInitiatedLogout = true;
    G.stopGuardActive = true;
    G.stopGuardSetMs = GetTickCount64();
    G.HasLogin = false;
    G.bIsRunning = false;
    InterlockedExchange(&G.reconnecting, 0);

    Network::Disconnect();

    if (G.hNetworkThread) {
        DWORD wr = WaitForSingleObject(G.hNetworkThread, 3000);
        if (wr != WAIT_OBJECT_0) {
            Utils::LogToFile("LOGOUT_SEQ_WARN", "Network thread did not exit in time during logout");
        }
        CloseHandle(G.hNetworkThread);
        G.hNetworkThread = NULL;
    }

    if (notifyHost && !wasGuardActive) {
        Utils::ShowMsg("LOGOUT", "LOGOUT OK!!");
    }

    Utils::LogToFile("STOP_LOGOUT_OK", "Logout completed; connection torn down");
}

// Zorro BrokerCommand codes
static constexpr int GET_TIME = 0;
static constexpr int GET_DIGITS = 1;
static constexpr int GET_STOPLEVEL = 2;
static constexpr int GET_TRADEALLOWED = 3;
static constexpr int GET_MINLOT = 4;
static constexpr int GET_LOTSTEP = 5;
static constexpr int GET_MAXLOT = 6;
static constexpr int GET_MARGININIT = 7;
static constexpr int GET_MARGINMAINTAIN = 8;
static constexpr int GET_MARGINHEDGED = 9;
static constexpr int GET_COMPLIANCE = 10;
static constexpr int GET_SERVERSTATE = 11;
static constexpr int GET_NTRADES = 16;
static constexpr int GET_POSITION = 17;
static constexpr int GET_AVGENTRY = 18;
static constexpr int GET_ACCOUNT = 19;
static constexpr int GET_TRADES = 35;

static constexpr int SET_PRICETYPE = 128;
static constexpr int SET_ORDERTYPE = 129;
static constexpr int SET_SYMBOL = 130;
static constexpr int SET_DIAGNOSTICS = 138;
static constexpr int SET_WAIT = 139;

// Custom commands
static constexpr int CMD_CLOSE_ALL_SHORT = 7001;
static constexpr int CMD_CLOSE_ALL_LONG = 7002;
static constexpr int CMD_CLOSE_ALL_PROFIT = 7003;
static constexpr int CMD_CLOSE_ALL_LOSS = 7004;
static constexpr ULONGLONG SUB_FALLBACK_TIMEOUT_MS = 8000;
static constexpr int SUB_FALLBACK_MAX_ATTEMPTS = 3;
static constexpr bool SUB_FALLBACK_ENABLED = false; // Temporarily disable quote fallback during diagnostics

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

static const char* FindSection(const char* buffer, const char* key) {
    const char* section = strstr(buffer, key);
    if (!section) {
        return nullptr;
    }
    return section;
}

static void ParseDepthArray(const char* section, std::vector<DepthLevel>& out) {
    out.clear();
    if (!section) {
        return;
    }

    const char* start = strchr(section, '[');
    if (!start) {
        return;
    }

    const char* end = strchr(start, ']');
    if (!end) {
        return;
    }

    const char* cursor = start + 1;
    while (cursor && cursor < end) {
        double price = 0.0;
        double volume = 0.0;
        int orders = 0;

        const char* pricePtr = strstr(cursor, "\"price\":");
        if (!pricePtr || pricePtr >= end) {
            break;
        }
        sscanf_s(pricePtr, "\"price\":%lf", &price);

        const char* volumePtr = strstr(cursor, "\"volume\":");
        if (volumePtr && volumePtr < end) {
            sscanf_s(volumePtr, "\"volume\":%lf", &volume);
        } else {
            const char* qtyPtr = strstr(cursor, "\"quantity\":");
            if (qtyPtr && qtyPtr < end) {
                sscanf_s(qtyPtr, "\"quantity\":%lf", &volume);
            }
        }

        const char* ordersPtr = strstr(cursor, "\"orders\":");
        if (ordersPtr && ordersPtr < end) {
            sscanf_s(ordersPtr, "\"orders\":%d", &orders);
        }

        if (price != 0.0 || volume != 0.0) {
            DepthLevel level;
            level.price = price;
            level.volume = volume;
            level.orders = orders;
            out.push_back(level);
        }

        const char* next = strchr(cursor, '}');
        if (!next || next >= end) {
            break;
        }
        cursor = next + 1;
        while (cursor < end && (*cursor == ',' || *cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
            cursor++;
        }
    }
}

static std::string StripScheme(const std::string& value) {
    std::string result = value;
    if (result.rfind("wss://", 0) == 0) {
        result = result.substr(6);
    } else if (result.rfind("https://", 0) == 0) {
        result = result.substr(8);
    } else if (result.rfind("http://", 0) == 0) {
        result = result.substr(7);
    }

    while (!result.empty() && (result.back() == '/' || result.back() == '\\')) {
        result.pop_back();
    }

    return result;
}

static bool TryParseEnvString(const char* value, CtraderEnv& outEnv) {
    if (!value || !*value) {
        return false;
    }

    if (Utils::StrContainsCaseInsensitive(value, "demo") ||
        Utils::StrContainsCaseInsensitive(value, "test") ||
        Utils::StrContainsCaseInsensitive(value, "paper")) {
        outEnv = CtraderEnv::Demo;
        return true;
    }

    if (Utils::StrContainsCaseInsensitive(value, "live") ||
        Utils::StrContainsCaseInsensitive(value, "real") ||
        Utils::StrContainsCaseInsensitive(value, "prod")) {
        outEnv = CtraderEnv::Live;
        return true;
    }

    return false;
}

static bool TryParseEnvString(const std::string& value, CtraderEnv& outEnv) {
    if (value.empty()) {
        return false;
    }
    return TryParseEnvString(value.c_str(), outEnv);
}

static bool ResolveHostFromString(const std::string& value, std::string& outHost, CtraderEnv& outEnv) {
    if (value.empty()) {
        return false;
    }

    std::string trimmed = Utils::Trim(value);
    if (trimmed.empty()) {
        return false;
    }

    std::string stripped = StripScheme(trimmed);
    std::string lower = stripped;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "demo" || lower == "demo.ctraderapi.com" || lower.find("demo") != std::string::npos) {
        outHost = CTRADER_HOST_DEMO;
        outEnv = CtraderEnv::Demo;
        return true;
    }

    if (lower == "live" || lower == "real" || lower == "live.ctraderapi.com" ||
        lower == "real.ctraderapi.com" || lower.find("live") != std::string::npos ||
        lower.find("real") != std::string::npos) {
        outHost = CTRADER_HOST_LIVE;
        outEnv = CtraderEnv::Live;
        return true;
    }

    if (!lower.empty() && lower.find("ctraderapi.com") != std::string::npos) {
        // Unknown cTrader host variant – reject to avoid bad DNS
        char warn[256];
        sprintf_s(warn, "Unrecognized cTrader host '%s'", stripped.c_str());
        Utils::LogToFile("HOST_OVERRIDE_WARN", warn);
    }

    return false;
}

static const char* HostForEnv() {
    if (!G.hostOverride.empty()) {
        return G.hostOverride.c_str();
    }
    return (G.Env == CtraderEnv::Live) ? CTRADER_HOST_LIVE : CTRADER_HOST_DEMO;
}

static CtraderEnv DetectEnvFromType(const char* Type) {
    if (!Type || !*Type) {
        Utils::LogToFile("ENV_DETECT", "Type is empty, preserving current G.Env");
        return G.Env;  // Preserve current environment when Type is not specified
    }

    char debugMsg[256];
    sprintf_s(debugMsg, "DetectEnvFromType called with Type='%s'", Type);
    Utils::LogToFile("ENV_DETECT", debugMsg);

    CtraderEnv parsedEnv;
    if (TryParseEnvString(Type, parsedEnv)) {
        Utils::LogToFile("ENV_DETECT", (parsedEnv == CtraderEnv::Live)
            ? "Detected LIVE environment from Type"
            : "Detected DEMO environment from Type");
        return parsedEnv;
    }

    Utils::LogToFile("ENV_DETECT", "Type unresolved, defaulting to DEMO environment");
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
            Symbols::CheckStalledSubscriptions(G.CTraderAccountId);
            Trading::CheckPendingOrderTimeouts(); // Check for timed-out orders
            G.lastRetryProcessMs = now;
        }
        
        // Send heartbeat if needed (independent of receive) - SKIP DURING RECONNECT
        if (now - G.lastPingMs >= PING_INTERVAL_MS && G.reconnecting == 0) {
            char ping[128];
            sprintf_s(ping, "{\"payloadType\":%d,\"payload\":{}}", static_cast<int>(PayloadType::VersionReq));
            if (Network::Send(ping)) {
                Utils::LogToFile("PING_SENT", "Heartbeat sent to maintain connection");
            } else {
                Utils::LogToFile("PING_FAILED", "Failed to send heartbeat");
            }
            G.lastPingMs = now;
        } else if (G.reconnecting == 1) {
            Utils::LogToFile("PING_SKIP", "Skipping heartbeat during reconnection");
        }

        int bytes = Network::Receive(buffer, sizeof(buffer));
        
        // SUB_FALLBACK: If no quotes after configured interval, resubscribe or reconnect
        if (SUB_FALLBACK_ENABLED &&
            G.quoteCount == 0 && G.subscriptionStartMs > 0 &&
            (now - G.subscriptionStartMs) > SUB_FALLBACK_TIMEOUT_MS) {
            ULONGLONG elapsed = now - G.subscriptionStartMs;
            double elapsedSeconds = static_cast<double>(elapsed) / 1000.0;

            char logMsg[160];
            sprintf_s(logMsg, "No quotes received after %.1f seconds (attempt %d/%d)",
                      elapsedSeconds,
                      G.subFallbackAttempts + 1,
                      SUB_FALLBACK_MAX_ATTEMPTS);
            Utils::LogToFile("SUB_FALLBACK", logMsg);

            if (G.subFallbackAttempts < SUB_FALLBACK_MAX_ATTEMPTS) {
                G.subFallbackAttempts++;
                G.subscriptionStartMs = now; // restart timer for next attempt
                Symbols::SubscribeToSpotPrices(G.CTraderAccountId);
                Utils::LogToFile("SUB_FALLBACK", "Dispatched spot price resubscription request");
            } else {
                Utils::LogToFile("SUB_FALLBACK", "Resubscribe attempts exhausted, forcing reconnection");
                G.subscriptionStartMs = 0; // Prevent repeated fallback loops
                G.subFallbackAttempts = 0;
                bytes = -1; // Override to trigger reconnection logic
            }
        }

        // Connection lost
        if (bytes < 0) {
            if (G.userInitiatedLogout) {
                Utils::LogToFile("STOP_LOGOUT_OK", "User-initiated stop/logout acknowledged; skipping reconnection attempts");
                G.bIsRunning = false;
                break;
            }

            Utils::LogToFile("CONNECTION_LOST", "Network thread detected connection loss");

            // PREVENT PARALLEL RECONNECTION ATTEMPTS
            if (InterlockedExchange(&G.reconnecting, 1) == 0) {
                Utils::LogToFile("RECONNECT_START", "Starting reconnection attempt (single threaded)");

                // Preserve environment state before reconnection attempt
                CtraderEnv savedEnv = G.Env;

                if (G.HasLogin && Reconnect::Attempt()) {
                    // Ensure environment wasn't overwritten during reconnection
                    G.Env = savedEnv;
                    Utils::ShowMsg("Reconnected");
                    InterlockedExchange(&G.reconnecting, 0);
                    continue;
                } else {
                    Utils::LogToFile("RECONNECT_FAILED", "All reconnection attempts failed");
                    Utils::Notify(Utils::UserMessageType::Error,
                                  "RECONNECT",
                                  "Reconnect failed after repeated attempts");
                }

                InterlockedExchange(&G.reconnecting, 0);
            } else {
                Utils::LogToFile("RECONNECT_SKIP", "Reconnection already in progress, skipping");
            }

            G.bIsRunning = false;
            break;
        }

        // No data received, continue monitoring
        if (bytes == 0) {
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

        switch (payloadType) {
    case ToInt(PayloadType::AccountAuthRes): {
            Utils::LogToFile("ACCOUNT_AUTH_RES", "Received account auth response (PROTO_OA_ACCOUNT_AUTH_RES)");
            G.tokenRefreshInProgress = false;
            G.lastTokenRefreshMs = GetTickCount64();
            break;
        }
    case ToInt(PayloadType::SpotSubscribeRes): {
            char clientMsgId[64] = {0};
            const char* pId = strstr(buffer, "\"clientMsgId\":\"");
            if (pId) {
                sscanf_s(pId, "\"clientMsgId\":\"%63[^\"]\"", clientMsgId, (unsigned)_countof(clientMsgId));
            }

            bool success = !Utils::BodyIndicatesError(buffer);
            std::string error_details;

            if (!success) {
                const char* pDesc = strstr(buffer, "\"description\":\"");
                if (pDesc) {
                    char desc[256] = {0};
                    sscanf_s(pDesc, "\"description\":\"%255[^\"]\"", desc, (unsigned)_countof(desc));
                    error_details = desc;
                }
            }

            Symbols::HandleSubscriptionResponse(clientMsgId, success, error_details);
            break;
        }
        case ToInt(PayloadType::SpotEvent): {
            static int spotDebugSamples = 0;
            if (spotDebugSamples < 5 && buffer) {
                Utils::LogToFile("SPOT_EVENT_SAMPLE", buffer);
                spotDebugSamples++;
            }
            if (G.quoteCount == 0) {
                Utils::LogToFile("FIRST_QUOTE", "First spot event received (PROTO_OA_SPOT_EVENT)");
                G.subscriptionStartMs = 0;
                G.subFallbackAttempts = 0;
            }
            G.quoteCount++;

            long long symbolId = 0;
            const char* pSymId = strstr(buffer, "\"symbolId\":");
            if (pSymId) {
                sscanf_s(pSymId, "\"symbolId\":%lld", &symbolId);
            }

            if (symbolId > 0) {
                long long bid = 0, ask = 0;
                long long timestamp = 0;

                const char* pBid = strstr(buffer, "\"bid\":");
                if (pBid) {
                    sscanf_s(pBid, "\"bid\":%lld", &bid);
                }

                const char* pAsk = strstr(buffer, "\"ask\":");
                if (pAsk) {
                    sscanf_s(pAsk, "\"ask\":%lld", &ask);
                }

                // Extract timestamp for BrokerCommand GET_TIME
                const char* pTimestamp = strstr(buffer, "\"timestamp\":");
                if (pTimestamp) {
                    sscanf_s(pTimestamp, "\"timestamp\":%lld", &timestamp);
                    if (timestamp > 0) {
                        G.lastServerTimestamp = timestamp;
                    }
                }

                Symbols::UpdateQuote(symbolId, bid, ask);
            }
            break;
        }
        case ToInt(PayloadType::SubscribeDepthQuotesRes):
        case ToInt(PayloadType::UnsubscribeDepthQuotesRes): {
            char clientMsgId[64] = {0};
            const char* pId = strstr(buffer, "\"clientMsgId\":\"");
            if (pId) {
                sscanf_s(pId, "\"clientMsgId\":\"%63[^\"]\"", clientMsgId, (unsigned)_countof(clientMsgId));
            }

            bool success = !Utils::BodyIndicatesError(buffer);
            std::string error_details;

            if (!success) {
                const char* pDesc = strstr(buffer, "\"description\":\"");
                if (pDesc) {
                    char desc[256] = {0};
                    sscanf_s(pDesc, "\"description\":\"%255[^\"]\"", desc, (unsigned)_countof(desc));
                    error_details = desc;
                }
            }

            if (payloadType == ToInt(PayloadType::SubscribeDepthQuotesRes)) {
                Symbols::HandleDepthSubscriptionResponse(clientMsgId, success, error_details);
            } else {
                Symbols::HandleDepthUnsubscribeResponse(clientMsgId, success, error_details);
            }
            break;
        }
        case ToInt(PayloadType::AssetClassRes): {
            Symbols::HandleAssetClassListResponse(buffer);
            G.assetClassReceived = true;
            break;
        }
        case ToInt(PayloadType::SubscribeDepthQuotesReq):
        case ToInt(PayloadType::DepthEvent): {
            long long symbolId = 0;
            const char* pSym = strstr(buffer, "\"symbolId\":");
            if (pSym) {
                sscanf_s(pSym, "\"symbolId\":%lld", &symbolId);
            }

            if (symbolId == 0) {
                Utils::LogToFile("DEPTH_EVENT", "Received depth event without symbolId");
                break;
            }

            std::vector<DepthLevel> bids;
            std::vector<DepthLevel> asks;

            const char* bidsSection = FindSection(buffer, "\"bids\":");
            if (!bidsSection) {
                bidsSection = FindSection(buffer, "\"bidDepth\":");
            }
            const char* asksSection = FindSection(buffer, "\"asks\":");
            if (!asksSection) {
                asksSection = FindSection(buffer, "\"askDepth\":");
            }

            ParseDepthArray(bidsSection, bids);
            ParseDepthArray(asksSection, asks);

            if (bids.empty() && asks.empty()) {
                Utils::LogToFile("DEPTH_EVENT", "Depth event parsed without levels");
            }

            bool isDelta = payloadType == ToInt(PayloadType::DepthEvent);
            Symbols::UpdateDepth(symbolId, bids, asks, (uint64_t)GetTickCount64(), isDelta);
            break;
        }
        case ToInt(PayloadType::SymbolCategoryRes): {
            Symbols::HandleSymbolCategoryResponse(buffer);
            G.symbolCategoryReceived = true;
            break;
        }
        case 2117: {  // PROTO_OA_SYMBOL_BY_ID_RES
            Symbols::HandleSymbolByIdResponse(buffer);
            break;
        }
        case ToInt(PayloadType::ExecutionEvent): {
            // Hardened: Robust execution event handling (2126)
            try {
                Trading::HandleExecutionEvent(buffer);
            } catch (const std::exception& ex) {
                Utils::LogToFile("EXEC_EVENT_ERROR", ex.what());
            }
            break;
        }
        case ToInt(PayloadType::OrderErrorEvent): {
            // Hardened: Robust order error event handling (2132)
            try {
                Trading::HandleOrderErrorEvent(buffer);
            } catch (const std::exception& ex) {
                Utils::LogToFile("ORDER_ERROR_EVENT_ERROR", ex.what());
            }
            break;
        }
        case ToInt(PayloadType::OrderListRes): {
            Trading::HandleOrderListResponse(buffer);
            break;
        }
        case ToInt(PayloadType::PositionListRes): {
            Trading::HandlePositionListResponse(buffer);
            break;
        }
        case ToInt(PayloadType::GetTrendbarsRes): {
            History::ProcessHistoricalResponse(buffer);
            break;
        }
        case ToInt(PayloadType::GetTickdataRes): {
            History::HandleTickDataResponse(buffer);
            break;
        }
        case 2188: {  // ProtoOAGetPositionUnrealizedPnLRes
            Utils::LogToFile("PNL_RESPONSE", "Received ProtoOAGetPositionUnrealizedPnLRes (2188)");
            Account::HandlePositionPnLResponse(buffer);
            break;
        }
        case ToInt(PayloadType::TraderRes): {
            Utils::LogToFile("ACCOUNT_RESPONSE", buffer);

            // Parse trader info for account data
            const char* payload = strstr(buffer, "\"payload\":");
            if (payload) {
                // Extract balance, equity, margin from trader object
                const char* trader = strstr(payload, "\"trader\":");
                if (trader) {
                    double balance = 0, equity = 0, margin = 0;
                    const char* currency = "USD";
                    int moneyDigits = -1;

                    // Parse balance
                    const char* pBalance = strstr(trader, "\"balance\":");
                    if (pBalance) sscanf_s(pBalance, "\"balance\":%lf", &balance);

                    // Parse equity
                    const char* pEquity = strstr(trader, "\"equity\":");
                    if (pEquity) sscanf_s(pEquity, "\"equity\":%lf", &equity);

                    // Parse margin
                    const char* pMargin = strstr(trader, "\"usedMargin\":");
                    if (pMargin) sscanf_s(pMargin, "\"usedMargin\":%lf", &margin);

                    // Parse money digits
                    const char* pDigits = strstr(trader, "\"moneyDigits\":");
                    if (pDigits) sscanf_s(pDigits, "\"moneyDigits\":%d", &moneyDigits);

                    // Parse currency
                    const char* pCurrency = strstr(trader, "\"depositCurrency\":\"");
                    if (pCurrency) {
                        char currencyBuf[8] = {0};
                        sscanf_s(pCurrency, "\"depositCurrency\":\"%7[^\"]\"", currencyBuf, (unsigned)sizeof(currencyBuf));
                        currency = currencyBuf;
                        currency = currencyBuf;
                    }

                    if (moneyDigits < 0) {
                        // fallback to digits returned in deposit asset metadata if trader payload omits the field
                        const char* pMoneyDigits = strstr(payload, "\"moneyDigits\":");
                        if (pMoneyDigits) sscanf_s(pMoneyDigits, "\"moneyDigits\":%d", &moneyDigits);
                    }

                    if (moneyDigits > 0) {
                        double scale = 1.0;
                        for (int i = 0; i < moneyDigits; ++i) {
                            scale *= 10.0;
                        }
                        if (scale > 0.0) {
                            balance /= scale;
                            equity /= scale;
                            margin /= scale;
                        }
                    }

                    Account::UpdateAccountInfo(balance, equity, margin, currency);

                    char msg[256];
                    sprintf_s(msg, "Account info updated: Balance=%.2f, Equity=%.2f, Margin=%.2f %s",
                             balance, equity, margin, currency);
                    Utils::LogToFile("ACCOUNT_UPDATE", msg);
                }
            }

            // Temporarily disabled - CashFlowHistory causes connection drop
            // if (!G.cashFlowRequested) {
            //     Account::RequestCashFlowHistory(50);
            // }
            G.cashFlowRequested = true; // Mark as requested to skip it

            if (!G.assetMetadataRequested) {
                bool classReq = Symbols::RequestAssetClassList(G.CTraderAccountId);
                bool categoryReq = Symbols::RequestSymbolCategories(G.CTraderAccountId);
                G.assetMetadataRequested = classReq || categoryReq;
            }

            // Request symbol details (lotSize, minVolume, maxVolume) for volume conversion
            if (!G.symbolDetailsRequested) {
                std::vector<long long> symbolIds;
                Symbols::Lock();
                for (const auto& kv : G.Symbols) {
                    if (kv.second.id > 0) {
                        symbolIds.push_back(kv.second.id);
                    }
                }
                Symbols::Unlock();

                if (!symbolIds.empty()) {
                    bool detailsReq = Symbols::RequestSymbolDetails(G.CTraderAccountId, symbolIds);
                    G.symbolDetailsRequested = detailsReq;
                    if (detailsReq) {
                        char msg[256];
                        sprintf_s(msg, "Requested volume details for %zu symbols", symbolIds.size());
                        Utils::LogToFile("SYMBOL_DETAILS_TRIGGER", msg);
                    }
                }
            }

            // Order snapshots are requested in BrokerOpen after auth, not here
            break;
        }
        case ToInt(PayloadType::MarginChangedEvent): {
            Account::HandleMarginChangedEvent(buffer);
            break;
        }
        case ToInt(PayloadType::CashFlowHistoryRes): {
            Account::HandleCashFlowHistoryResponse(buffer);
            break;
        }
        case ToInt(PayloadType::ErrorRes): {
            char errorCode[64] = {0};
            char description[256] = {0};
            
            // Parse error code
            const char* pCode = strstr(buffer, "\"errorCode\":\"");
            if (pCode) {
                sscanf_s(pCode, "\"errorCode\":\"%63[^\"]\"", errorCode, (unsigned)sizeof(errorCode));
            }
            
            // Parse description
            const char* pDesc = strstr(buffer, "\"description\":\"");
            if (pDesc) {
                sscanf_s(pDesc, "\"description\":\"%255[^\"]\"", description, (unsigned)sizeof(description));
            }
            
            char errorMsg[512];
            sprintf_s(errorMsg, "ERROR_2142: %s - %s", errorCode, description);
            Utils::LogToFile("ERROR_2142", errorMsg);
            
            // Log specific diagnosis for UNSUPPORTED_MESSAGE
            if (strstr(errorCode, "UNSUPPORTED_MESSAGE")) {
                Utils::LogToFile("DIAGNOSIS", "Likely cause: Dual subscription implementation conflict or session race condition");
            }
            break;
        }
    case ToInt(PayloadType::SpotUnsubscribeRes): {
            // Phase 1: Depth unsubscribe/delta compression stub
            char clientMsgId[64] = {0};
            const char* pId = strstr(buffer, "\"clientMsgId\":\"");
            if (pId) {
                sscanf_s(pId, "\"clientMsgId\":\"%63[^\"]\"", clientMsgId, (unsigned)_countof(clientMsgId));
            }
            bool success = !Utils::BodyIndicatesError(buffer);
            char logMsg[160];
            if (clientMsgId[0] != '\0') {
                sprintf_s(logMsg, "Spot unsubscribe ack (%s) clientMsgId=%s", success ? "success" : "error", clientMsgId);
            } else {
                sprintf_s(logMsg, "Spot unsubscribe ack (%s)", success ? "success" : "error");
            }
            Utils::LogToFile("SPOT_UNSUB_RES", logMsg);
            // TODO: Add delta compression and depth unsubscribe stabilization here
            break;
        }
        case ToInt(PayloadType::TokenInvalidatedEvent): {
            Utils::LogToFile("TOKEN_INVALIDATED_EVENT", "Received token invalidated event (PROTO_OA_ACCOUNTS_TOKEN_INVALIDATED_EVENT)");
            if (!G.tokenRefreshInProgress) {
                G.tokenRefreshInProgress = true;
                if (Auth::RefreshAccessToken()) {
                    G.lastTokenRefreshMs = GetTickCount64();
                    if (!Account::SendAccountAuth(G.CTraderAccountId)) {
                        Utils::LogToFile("TOKEN_INVALIDATED_EVENT", "Failed to dispatch account auth request after refresh (PROTO_OA_ACCOUNT_AUTH_REQ), forcing reconnect");
                        Network::Disconnect();
                    } else {
                        Utils::LogToFile("TOKEN_INVALIDATED_EVENT", "Access token refreshed and account auth request sent (PROTO_OA_ACCOUNT_AUTH_REQ)");
                    }
                }
            }
            break;
        }
        case ToInt(PayloadType::ClientDisconnectEvent):
        case ToInt(PayloadType::AccountDisconnectEvent): {
            Utils::LogToFile(payloadType == ToInt(PayloadType::ClientDisconnectEvent) ? "CLIENT_DISCONNECT_EVENT" : "ACCOUNT_DISCONNECT_EVENT",
                             payloadType == ToInt(PayloadType::ClientDisconnectEvent) ? "Server issued client disconnect" : "Server issued account disconnect");
            G.tokenRefreshInProgress = false;
            break;
        }
        case ToInt(PayloadType::VersionRes): {
            Utils::LogToFile("PING_ACK", "Version keep-alive acknowledged");
            break;
        }
        case ToInt(PayloadType::NewOrderReq): {
            char pong[64];
            sprintf_s(pong, "{\"payloadType\":%d,\"payload\":{}}", ToInt(PayloadType::NewOrderReq));
            if (Network::Send(pong)) {
                Utils::LogToFile("PING_RESPONSE", "Legacy heartbeat response sent");
            } else {
                Utils::LogToFile("PING_RESPONSE_FAILED", "Failed to send legacy heartbeat response");
            }
            break;
        }
        // ---- Phase 1: Market data parity stubs ----
        case ToInt(PayloadType::SpotSubscribeReq):
        case ToInt(PayloadType::UnsubscribeDepthQuotesReq):
            Utils::LogToFile("PHASE1_STUB", "Phase 1 market data message received (stub handler)");
            break;
        case 51: {
            static bool keepaliveLogged = false;
            if (!keepaliveLogged) {
                Utils::LogToFile("KEEPALIVE", "Received keepalive frame (payloadType 51); suppressing further logs");
                keepaliveLogged = true;
            }
            break;
        }
        case 0:
            break;
        default: {
            char msg[192];
            sprintf_s(msg, "Unhandled payloadType=%d (%s)", payloadType, PayloadTypeName(payloadType));
            Utils::LogToFile("PAYLOAD_UNHANDLED", msg);
            break;
        }
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
    G.stopGuardActive = false;
    G.stopGuardSetMs = 0;
    G.userInitiatedLogout = false;
    return PLUGIN_TYPE;
}

DLLFUNC int BrokerLogin(char* User, char* Pwd, char* Type, char* Accounts) {
    if (InterlockedCompareExchange(&G.loginInProgress, 1, 0) != 0) {
        Utils::LogToFile("BROKERLOGIN_WARN", "Login request ignored because another login is already in progress");
        return 0;
    }

    struct LoginGuard {
        bool active;
        ~LoginGuard() {
            if (active) {
                InterlockedExchange(&G.loginInProgress, 0);
            }
        }
    } loginGuard{true};

    if (G.stopGuardActive && G.userInitiatedLogout) {
        Utils::LogToFile("STOP_GUARD", "Login suppressed while stop guard active");
        Utils::Notify(Utils::UserMessageType::Warning,
                      "STOP_GUARD",
                      "Stop/logout in progress; skipping auto reconnect");
        return 0;
    }

    G.userInitiatedLogout = false;
    G.stopGuardActive = false;
    G.stopGuardSetMs = 0;

    // Stop existing connection if any - PREVENT DOUBLE LOGIN
    if (G.bIsRunning) {
        Utils::LogToFile("BROKERLOGIN", "Stopping existing connection before new login");
        G.bIsRunning = false;

        // Disconnect WebSocket first to unblock the network thread
        Network::Disconnect();

        // Wait for network thread to actually stop
        if (G.hNetworkThread) {
            DWORD waitResult = WaitForSingleObject(G.hNetworkThread, 3000); // Reduced to 3s since we disconnect first
            if (waitResult != WAIT_OBJECT_0) {
                Utils::LogToFile("BROKERLOGIN_WARN", "Network thread did not stop cleanly, forcing termination");
                TerminateThread(G.hNetworkThread, 0);
            }
            CloseHandle(G.hNetworkThread);
            G.hNetworkThread = NULL;
        }

        // Ensure reconnect flag is reset
        InterlockedExchange(&G.reconnecting, 0);
        
        // Brief pause to ensure cleanup is complete
        Sleep(500);
    }

    Network::Disconnect();

    CtraderEnv previousEnv = G.Env;
    bool hasPriorEnv = G.HasLogin;
    bool previousEnvLocked = G.envLocked;

    // Selective clearing instead of ZeroMemory to preserve C++ objects
    ZeroMemory(G.Token, sizeof(G.Token));
    ZeroMemory(G.RefreshToken, sizeof(G.RefreshToken));
    G.CTraderAccountId = 0;
    G.Env = hasPriorEnv ? previousEnv : CtraderEnv::Demo;
    G.envLocked = hasPriorEnv ? previousEnvLocked : false;
    G.hostOverride.clear();

    // Initialize WebSocket handles
    G.hSession = NULL;
    G.hConnect = NULL;
    G.hWebSocket = NULL;
    G.wsConnected = false;
    G.Symbols.clear();
    G.pendingTrades.clear();
    G.openTrades.clear();
    G.ctidToZorroId.clear();
    G.zorroIdToOrderId.clear();
    G.orderIdToZorroId.clear();
    G.pendingTradeInfo.clear();
    G.nextTradeId = 1;
    G.lastPingMs = GetTickCount64();
    G.reconnecting = 0;  // Initialize reconnection flag
    G.quoteCount = 0;    // Initialize quote counter
    G.subscriptionStartMs = 0;  // Initialize subscription timer
    G.subFallbackAttempts = 0;
    G.snapshotsRequested = false;
    G.cashFlowRequested = false;
    G.cashFlowHydrated = false;
    G.lastMarginUpdateMs = 0;
    G.tokenRefreshInProgress = false;

    // Initialize BrokerCommand state
    G.lastServerTimestamp = 0;
    G.currentSymbol.clear();
    G.orderType = 0;  // Default to market orders
    G.loginCompleted = false;  // Login not yet complete
    G.lastTokenRefreshMs = 0;
    G.assetMetadataRequested = false;
    G.assetClassReceived = false;
    G.symbolCategoryReceived = false;
    G.symbolDetailsRequested = false;  // CRITICAL: Request symbol volume details for minVolume/maxVolume
    G.lastSessionEventMs = 0;
    G.userInitiatedLogout = false;
    G.stopGuardActive = false;
    G.stopGuardSetMs = 0;

    // Initialize unrealized P&L tracking
    G.lastPnLRequestMs = 0;
    G.pnlRequestPending = false;

    // NOTE: Do NOT call Symbols::Cleanup() and Symbols::Initialize() here!
    // This would clear all symbol prices and subscription states, causing
    // "No prices" errors when Zorro reconnects. Symbol initialization happens
    // once in DllMain and should persist across multiple login/logout cycles.

    // Re-enable Accounts.csv usage with validation
    bool usedCsv = false;
    CsvCreds csvCreds{};
    const char* hint = (Accounts && *Accounts) ? Accounts : (User && *User ? User : nullptr);
    CtraderEnv desiredEnv = G.Env;
    bool envDetermined = false;
    const char* envSourceLabel = G.envLocked ? "TYPE" : (hasPriorEnv ? "PREV" : "PARAM");

    bool typeProvided = (Type && *Type);
    CtraderEnv typeEnv = desiredEnv;
    bool typeParsed = typeProvided && TryParseEnvString(Type, typeEnv);

    if (typeProvided) {
        if (typeParsed) {
            desiredEnv = typeEnv;
            envDetermined = true;
            G.envLocked = true;
            envSourceLabel = "TYPE";
            char typeMsg[128];
            sprintf_s(typeMsg, "Environment forced by Zorro Type parameter: %s",
                      (desiredEnv == CtraderEnv::Live) ? "LIVE" : "DEMO");
            Utils::LogToFile("ENV_SOURCE", typeMsg);
        } else {
            Utils::LogToFile("ENV_SOURCE", "Zorro Type parameter provided but unrecognized; ignoring");
            if (!hasPriorEnv) {
                envSourceLabel = "PARAM";
                G.envLocked = false;
            }
        }
    } else if (!hasPriorEnv) {
        envSourceLabel = "PARAM";
        G.envLocked = false;
    }

    if (CsvLoader::LoadAccountsCsv(csvCreds, hint, Type)) {
        // Validate if user specified an explicit account but csv differs
        if (Accounts && *Accounts) {
            unsigned long long paramAcc = _strtoui64(Accounts, nullptr, 10);
            unsigned long long csvAcc = _strtoui64(csvCreds.accountId.c_str(), nullptr, 10);
            if (paramAcc != 0 && csvAcc != 0 && paramAcc != csvAcc) {
                Utils::ShowMsg("Error: Specified account does not match Accounts.csv entry");
                return 0;
            }
        }

        // Accept CSV credentials
        strncpy_s(G.ClientId, sizeof(G.ClientId), csvCreds.clientId.c_str(), _TRUNCATE);
        strncpy_s(G.ClientSecret, sizeof(G.ClientSecret), csvCreds.clientSecret.c_str(), _TRUNCATE);
        if (!csvCreds.accountId.empty()) {
            G.CTraderAccountId = _strtoui64(csvCreds.accountId.c_str(), nullptr, 10);
        }
        if (!csvCreds.accessToken.empty()) {
            strncpy_s(G.Token, sizeof(G.Token), csvCreds.accessToken.c_str(), _TRUNCATE);
        }

        std::string trimmedServer = Utils::Trim(csvCreds.server);
        if (!trimmedServer.empty()) {
            std::string resolvedHost;
            CtraderEnv hostEnv = desiredEnv;
            if (ResolveHostFromString(trimmedServer, resolvedHost, hostEnv)) {
                if (G.envLocked && hostEnv != desiredEnv) {
                    char warn[256];
                    sprintf_s(warn, "Ignoring Accounts.csv Server override '%s' (conflicts with forced environment)", trimmedServer.c_str());
                    Utils::LogToFile("HOST_OVERRIDE", warn);
                } else {
                    G.hostOverride = resolvedHost;
                    char hostLog[256];
                    sprintf_s(hostLog, "Using host override from Accounts.csv: %s", resolvedHost.c_str());
                    Utils::LogToFile("HOST_OVERRIDE", hostLog);

                    if (!G.envLocked) {
                        if (!envDetermined) {
                            desiredEnv = hostEnv;
                            envDetermined = true;
                            envSourceLabel = "CSV";
                            Utils::LogToFile("ENV_SOURCE", "Environment derived from Accounts.csv Server column");
                        } else if (desiredEnv != hostEnv) {
                            Utils::LogToFile("ENV_MISMATCH", "Accounts.csv Server host conflicts with other environment source; keeping existing environment");
                        }
                    }
                }
            } else {
                char warn[256];
                sprintf_s(warn, "Ignoring unrecognized Server value in Accounts.csv: %s", trimmedServer.c_str());
                Utils::LogToFile("HOST_OVERRIDE", warn);
            }
        }

        if (csvCreds.hasExplicitEnv) {
            if (G.envLocked) {
                if (csvCreds.explicitEnv != desiredEnv) {
                    Utils::LogToFile("ENV_MISMATCH", "Accounts.csv Real flag conflicts with forced environment; ignoring");
                }
            } else {
                desiredEnv = csvCreds.explicitEnv;
                envDetermined = true;
                envSourceLabel = "CSV";
                Utils::LogToFile("ENV_SOURCE", "Environment derived from Accounts.csv Real flag");
            }
        }

        if (!G.envLocked) {
            if (!envDetermined && TryParseEnvString(csvCreds.type, desiredEnv)) {
                envDetermined = true;
                envSourceLabel = "CSV";
                Utils::LogToFile("ENV_SOURCE", "Environment derived from Accounts.csv Type column");
            }
        } else if (!csvCreds.type.empty()) {
            CtraderEnv csvTypeEnv;
            if (TryParseEnvString(csvCreds.type, csvTypeEnv) && csvTypeEnv != desiredEnv) {
                Utils::LogToFile("ENV_MISMATCH", "Accounts.csv Type column conflicts with forced environment; ignoring");
            }
        }

        if (!envDetermined) {
            if (G.envLocked) {
                // Environment already forced by Type; nothing to adjust
            } else if (hasPriorEnv) {
                desiredEnv = previousEnv;
                envSourceLabel = "PREV";
                Utils::LogToFile("ENV_SOURCE", "Environment preserved from previous session (CSV ambiguous)");
            } else {
                desiredEnv = CtraderEnv::Demo;
                envSourceLabel = "CSV";
                Utils::LogToFile("ENV_SOURCE", "Environment ambiguous; defaulting to DEMO");
            }
        }

        usedCsv = true;
        Utils::LogToFile("ACCOUNTS_CSV", "Credentials loaded and validated from CSV");
    } else {
        // Fallback to parameters
        strncpy_s(G.ClientId, sizeof(G.ClientId), User ? User : "", _TRUNCATE);
        strncpy_s(G.ClientSecret, sizeof(G.ClientSecret), Pwd ? Pwd : "", _TRUNCATE);
        if (Accounts && *Accounts) {
            G.CTraderAccountId = _strtoui64(Accounts, NULL, 10);
        }

        if (!envDetermined) {
            if (G.envLocked) {
                // Already forced by Type; nothing further to do
            } else if (hasPriorEnv) {
                desiredEnv = previousEnv;
                envSourceLabel = "PREV";
                Utils::LogToFile("ENV_SOURCE", "Environment preserved from previous session (no CSV)");
            } else {
                desiredEnv = CtraderEnv::Demo;
                envSourceLabel = "PARAM";
                Utils::LogToFile("ENV_SOURCE", "Environment fallback to DEMO (no CSV)");
            }
        }

        Utils::LogToFile("ACCOUNTS_CSV", "CSV not used (missing or validation failed) - using parameters");
    }

    G.Env = desiredEnv;

    const char* envLabel = envSourceLabel;
    if (G.envLocked) {
        envLabel = "TYPE";
    } else if (usedCsv && envLabel != "TYPE" && envLabel != "CSV" && !typeParsed) {
        envLabel = "PARAM";
    }

    char envMsg[128];
    sprintf_s(envMsg, "G.Env set (%s) to: %s", envLabel, (G.Env == CtraderEnv::Live) ? "LIVE" : "DEMO");
    Utils::LogToFile("G_ENV_SET", envMsg);

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
            Utils::Notify(Utils::UserMessageType::Error,
                          "LOGIN",
                          "Error: Client ID/Secret missing");
            return 0;
        }

        if (!OAuth::PerformInteractiveFlow()) {
            Utils::Notify(Utils::UserMessageType::Error,
                          "LOGIN",
                          "OAuth flow failed");
            return 0;
        }
    }

    // Connect to cTrader
    // Debug: Log the current environment before host selection
    char envDebug[128];
    sprintf_s(envDebug, "Host selection with G.Env=%s", (G.Env == CtraderEnv::Live) ? "LIVE" : "DEMO");
    Utils::LogToFile("HOST_DEBUG", envDebug);

    const char* host = HostForEnv();
    char msg[256];
    sprintf_s(msg, "Connecting to: %s", host);
    Utils::ShowMsg(msg);

    if (!Network::Connect(host, "5036")) {
        Utils::Notify(Utils::UserMessageType::Error,
                      "LOGIN",
                      "Connection failed");
        return 0;
    }

    // Application authentication (PROTO_OA_APPLICATION_AUTH_REQ)
    char request[1024] = {0};
    char response[131072] = {0};

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"clientId\":\"%s\",\"clientSecret\":\"%s\"}}",
        Utils::GetMsgId(), ToInt(PayloadType::ApplicationAuthReq), G.ClientId, G.ClientSecret);

    if (!Network::Send(request)) {
        Utils::Notify(Utils::UserMessageType::Error,
                      "LOGIN",
                      "App auth send failed");
        return 0;
    }

    if (Network::Receive(response, sizeof(response)) <= 0) {
        Utils::Notify(Utils::UserMessageType::Error,
                      "LOGIN",
                      "App auth response failed");
        return 0;
    }

    if (!Utils::ContainsPayloadType(response, PayloadType::ApplicationAuthRes)) {
        Utils::Notify(Utils::UserMessageType::Warning,
                      "LOGIN",
                      "App auth failed, trying refresh token...");
        Utils::LogToFile("APP_AUTH_RESPONSE", response);

        // Try to refresh the access token
        if (Auth::RefreshAccessToken()) {
            Utils::Notify(Utils::UserMessageType::Success,
                          "LOGIN",
                          "Token refreshed, retrying app auth...");

            // Retry app authentication with new token
            sprintf_s(request,
                "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"clientId\":\"%s\",\"clientSecret\":\"%s\"}}",
                Utils::GetMsgId(), ToInt(PayloadType::ApplicationAuthReq), G.ClientId, G.ClientSecret);

            if (!Network::Send(request)) {
                Utils::Notify(Utils::UserMessageType::Error,
                              "LOGIN",
                              "App auth retry send failed");
                return 0;
            }

            if (Network::Receive(response, sizeof(response)) <= 0) {
                Utils::Notify(Utils::UserMessageType::Error,
                              "LOGIN",
                              "App auth retry response failed");
                return 0;
            }

            if (!Utils::ContainsPayloadType(response, PayloadType::ApplicationAuthRes)) {
                Utils::Notify(Utils::UserMessageType::Error,
                              "LOGIN",
                              "App auth failed even after token refresh");
                Utils::LogToFile("APP_AUTH_RETRY_RESPONSE", response);
                return 0;
            }

            Utils::Notify(Utils::UserMessageType::Success,
                          "LOGIN",
                          "App auth succeeded after token refresh");
        } else {
            Utils::Notify(Utils::UserMessageType::Error,
                          "LOGIN",
                          "Token refresh failed, giving up");
            return 0;
        }
    }

    Utils::Notify(Utils::UserMessageType::Success, "LOGIN", "App authenticated");

    // Auto-detect account if not specified
    if (G.CTraderAccountId == 0) {
        Utils::ShowMsg("Auto-detecting account...");
        std::vector<long long> accountIds;

        CtraderEnv requestedEnv = G.Env;
        if (Auth::FetchAccountsList(accountIds, requestedEnv) && !accountIds.empty()) {
            if (G.Env != requestedEnv) {
                char envAdjustMsg[128];
                sprintf_s(envAdjustMsg, "Environment adjusted during account detection: %s",
                          (G.Env == CtraderEnv::Live) ? "LIVE" : "DEMO");
                Utils::LogToFile("ENV_ADJUST", envAdjustMsg);
            }
            G.CTraderAccountId = accountIds[0];
            char accMsg[128];
            sprintf_s(accMsg, "Account detected: %lld (%s)", G.CTraderAccountId,
                     (G.Env == CtraderEnv::Live) ? "LIVE" : "DEMO");
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
        sprintf_s(req, "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld}}",
                 Utils::GetMsgId(), ToInt(PayloadType::AccountAuthReq), G.Token, acctId);

        Utils::LogToFile("AUTH", "Trying AccountAuth (PROTO_OA_ACCOUNT_AUTH_REQ) for:");

        if (!Network::Send(req)) return false;
        int r = Network::Receive(locRes, sizeof(locRes));
        if (r <= 0) return false;

        Utils::LogToFile("AUTH", "AccountAuth response received");
        Utils::LogToFile("AUTH_RESPONSE", locRes);

        if (Utils::ContainsPayloadType(locRes, PayloadType::AccountAuthRes)) {
            Utils::LogToFile("AUTH", "Found account auth response (PROTO_OA_ACCOUNT_AUTH_RES) - SUCCESS");
            return true;
        } else {
            Utils::LogToFile("AUTH", "Account auth response not found (PROTO_OA_ACCOUNT_AUTH_RES) - FAILED");
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

        // Store original environment to prevent overwriting
        CtraderEnv retryEnv = G.Env;
        if (Auth::FetchAccountsList(accountIds, retryEnv) && !accountIds.empty()) {
            // Keep original G.Env
            aid = accountIds[0];
            if (aid != G.CTraderAccountId && try_account_auth(aid)) {
                G.CTraderAccountId = aid;
            } else {
                // Try all account IDs from the list
                Utils::LogToFile("AUTH", "Retrying AccountAuth over all IDs from version list (PROTO_OA_VERSION_REQ)...");
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
                    if (G.CTraderAccountId == 0 || !Utils::ContainsPayloadType(response, PayloadType::AccountAuthRes)) {
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
            "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld}}",
            Utils::GetMsgId(), ToInt(PayloadType::AccountAuthReq), G.Token, acctId);

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

        if (!Utils::ContainsPayloadType(locRes, PayloadType::AccountAuthRes)) {
            Utils::LogToFile("AUTH", "TryAccountAuth: Account auth response missing (PROTO_OA_ACCOUNT_AUTH_RES) - FAILED");
            return false;
        }

        Utils::LogToFile("AUTH", "TryAccountAuth: Found account auth response (PROTO_OA_ACCOUNT_AUTH_RES) - SUCCESS");

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
        CtraderEnv finalEnv = G.Env;  // Preserve environment
        if (Auth::FetchAccountsList(accountIds, finalEnv)) {
            // Keep original G.Env
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

    // Request order snapshot immediately after authentication
    // Note: Positions are derived from orders, no separate position snapshot needed
    Trading::RequestOrderSnapshot();

    // Try to load cached assets first
    bool cacheOk = Symbols::LoadAssetCache(G.DllPath);
    int count = 0;
    if (cacheOk) {
        Utils::ShowMsg("Loaded symbols from cache (AssetList.txt)");
        // Count symbols
        for (auto& kv : G.Symbols) count++;
    } else {
        Utils::ShowMsg("Fetching symbols (cache miss or legacy)...");

        ZeroMemory(request, sizeof(request));
        ZeroMemory(response, sizeof(response));

        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"ctidTraderAccountId\":%lld}}",
            Utils::GetMsgId(), ToInt(PayloadType::SymbolsListReq), G.CTraderAccountId);

        if (!Network::Send(request)) {
            Utils::ShowMsg("Symbol request failed");
            return 0;
        }

        if (Network::Receive(response, sizeof(response)) <= 0) {
            Utils::ShowMsg("Symbol response failed");
            return 0;
        }

        if (!Utils::ContainsPayloadType(response, PayloadType::SymbolsListRes)) {
            Utils::ShowMsg("Symbol fetch failed");
            return 0;
        }

        Utils::LogToFile("SYMBOLS_RESPONSE", response);

        // Parse symbols - match working version pattern
        const char* pSymbol = strstr(response, "\"symbol\":[");
        if (!pSymbol) pSymbol = strstr(response, "\"symbols\":[");

        Utils::LogToFile("SYMBOLS_DEBUG", pSymbol ? "Found symbol array" : "Symbol array NOT found");

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
                    long long assetId = 0;
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

                    const char* pAssetId = strstr(current, "\"assetId\":");
                    if (pAssetId) {
                        sscanf_s(pAssetId, "\"assetId\":%lld", &assetId);
                    }

                    if (id > 0 && strlen(symbolName) > 0) {
                        Symbols::AddSymbol(symbolName, id, digits, assetId);
                        count++;
                    }

                    // Advance
                    const char* next_brace = strstr(current + 1, "{");
                    const char* next_comma = strstr(current + 1, ",");
                    if (next_brace && (!next_comma || next_brace < next_comma)) {
                        current = next_brace;
                    } else if (next_comma) {
                        current = next_comma;
                    } else {
                        break;
                    }
                }
            }
        }

        sprintf_s(msg, "Loaded %d symbols (fresh fetch)", count);
        Utils::ShowMsg(msg);
    }

    bool assetsSynced = Symbols::FetchAssetList(G.CTraderAccountId, G.DllPath);
    if (assetsSynced) {
        Utils::ShowMsg("Asset list synced from server (PROTO_OA_ASSET_LIST_RES)");
    } else {
        Utils::ShowMsg("Asset list sync failed, continuing with cached metadata");
    }

    Symbols::GenerateBrokerAssetsFile(G.DllPath);

    if (!G.assetMetadataRequested) {
    bool classReq = Symbols::RequestAssetClassList(G.CTraderAccountId);
        bool categoryReq = Symbols::RequestSymbolCategories(G.CTraderAccountId);
        G.assetMetadataRequested = classReq || categoryReq;
    }

    // Subscribe to priority symbols to keep connection alive
    // Note: Only major forex pairs, metals, indices, and commodities (~45 symbols)
    Utils::ShowMsg("Subscribing to priority symbols...");
    G.subFallbackAttempts = 0;
    G.subscriptionStartMs = GetTickCount64();
    Symbols::SubscribeToSpotPrices(G.CTraderAccountId);

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

    // Request initial account info (balance, equity, margin)
    Account::RequestAccountInfo();

    // Active wait for initial spot events to arrive
    Utils::LogToFile("BROKERLOGIN", "Waiting for initial quotes...");
    bool quotesReady = false;
    for (int i = 0; i < 50; i++) {  // Max 5 seconds (50 × 100ms)
        Sleep(100);

        // Check if we have quotes (check G.quoteCount > 0 means at least some quotes arrived)
        if (G.quoteCount > 0) {
            char msg[128];
            sprintf_s(msg, "First quotes received after %dms (%d quotes)", (i + 1) * 100, G.quoteCount);
            Utils::LogToFile("BROKERLOGIN", msg);
            quotesReady = true;
            break;
        }
    }

    if (!quotesReady) {
        Utils::LogToFile("BROKERLOGIN_WARN", "Timeout waiting for quotes, continuing anyway");
    }

    // Mark login as completed
    G.loginCompleted = true;

    // TEST: Send manual history request to check if server responds
    FILETIME ft;
    ZeroMemory(&ft, sizeof(ft));
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    ZeroMemory(&uli, sizeof(uli));
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    const long long EPOCH_DIFF = 116444736000000000LL;
    long long nowMs = (long long)((uli.QuadPart - EPOCH_DIFF) / 10000LL);
    long long startMs = nowMs - (7LL * 86400000LL);  // 7 days ago

    // DEBUG LOG
    char debugMsg[256];
    sprintf_s(debugMsg, "TIMESTAMP DEBUG: nowMs=%lld, startMs=%lld, QuadPart=%llu", nowMs, startMs, uli.QuadPart);
    Utils::LogToFile("TIMESTAMP_DEBUG", debugMsg);

    char testHistoryMsg[512];
    sprintf_s(testHistoryMsg,
        "{\"clientMsgId\":\"manual_test_hist\",\"payloadType\":2137,"
        "\"payload\":{\"ctidTraderAccountId\":%lld,\"symbolId\":1,\"period\":1,"
        "\"fromTimestamp\":%lld,\"toTimestamp\":%lld}}",
        G.CTraderAccountId, startMs, nowMs);
    Network::Send(testHistoryMsg);
    Utils::LogToFile("HISTORY_TEST", "Sent manual GetTrendbarsReq for EUR/USD (symbolId 1)");

    // Wait for first quote to arrive (important: Zorro immediately calls BrokerAsset after login)
    Utils::LogToFile("LOGIN_WAIT", "Waiting for first quote before returning from BrokerLogin...");
    int waitCount = 0;
    while (G.quoteCount == 0 && waitCount < 100) { // Max 10 seconds (100 * 100ms)
        Sleep(100);
        waitCount++;
    }

    if (G.quoteCount > 0) {
        char quoteMsg[128];
        sprintf_s(quoteMsg, "First quote received after %d ms, %d quotes total", waitCount * 100, G.quoteCount);
        Utils::LogToFile("LOGIN_WAIT", quoteMsg);
    } else {
        Utils::LogToFile("LOGIN_WAIT", "Warning: No quotes received after 10s wait");
    }

    Utils::Notify(Utils::UserMessageType::Success, "LOGIN", "Login successful");
    return 1;
}

DLLFUNC void BrokerClose() {
    Utils::LogToFile("BROKERCLOSE", "Initiating shutdown");
    PerformLogout("BROKERCLOSE", true);
    Utils::LogToFile("BROKERCLOSE", "WebSocket disconnected and thread stopped");
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

    // Check if login is complete
    if (!G.loginCompleted || G.CTraderAccountId == 0) {
        Utils::LogToFile("BROKERASSET_WAIT", "Login not yet complete, returning 0");
        return 0;
    }

    char msg[256];
    sprintf_s(msg, "BrokerAsset called for: '%s'", Symbol);
    Utils::LogToFile("BROKERASSET", msg);

    // Get normalized symbol name for subscription (AUDUSD not AUD/USD)
    std::string normalizedName = Symbols::NormalizeSymbol(Symbol);

    SymbolInfo* info = Symbols::GetSymbol(normalizedName.c_str());
    if (!info) {
        sprintf_s(msg, "BrokerAsset: Symbol '%s' (normalized: '%s') not found via GetSymbol", Symbol, normalizedName.c_str());
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
        sprintf_s(msg, "BrokerAsset: Symbol '%s' (normalized: '%s') found via GetSymbol", Symbol, normalizedName.c_str());
        Utils::LogToFile("BROKERASSET", msg);
    }

    // Ensure subscribed to get quotes (on-demand for non-priority symbols)
    Symbols::EnsureSubscribed(G.CTraderAccountId, normalizedName);
    // Note: Depth subscription (2155) not supported on this server, disabled

    // Wait briefly for first quote if subscription was just sent
    Symbols::Lock();
    bool needsWait = false;
    SymbolInfo* checkInfo = Symbols::GetSymbol(normalizedName.c_str());
    if (checkInfo && checkInfo->subscriptionPending && !checkInfo->hasFirstQuote) {
        needsWait = true;
    }
    Symbols::Unlock();

    if (needsWait) {
        Utils::LogToFile("BROKERASSET_WAIT", "Waiting for first quote after subscription...");
        // Active wait: check every 100ms if price arrived, max 5 seconds
        for (int i = 0; i < 50; i++) {  // 50 × 100ms = 5000ms max
            Sleep(100);
            Symbols::Lock();
            SymbolInfo* waitInfo = Symbols::GetSymbol(normalizedName.c_str());
            bool hasPrice = (waitInfo && waitInfo->bid > 0 && waitInfo->ask > 0);
            Symbols::Unlock();

            if (hasPrice) {
                sprintf_s(msg, "First quote received after %dms", (i + 1) * 100);
                Utils::LogToFile("BROKERASSET_WAIT", msg);
                break;
            }
        }
    }

    Symbols::Lock();

    double d_bid = 0, d_ask = 0;
    double bestBidVolume = 0;
    double bestAskVolume = 0;
    // cTrader API: bid/ask are ALWAYS in 1/100000 of unit (fixed scale, not based on digits)
    // See: https://help.ctrader.com/open-api/messages/#protoOAspotevent
    const double CTRADER_PRICE_SCALE = 100000.0;
    if (info->bid > 0 && info->ask > 0) {
        d_bid = static_cast<double>(info->bid) / CTRADER_PRICE_SCALE;
        d_ask = static_cast<double>(info->ask) / CTRADER_PRICE_SCALE;
    }
    int digits = info->digits;

    if (!info->bidDepth.empty()) {
        bestBidVolume = info->bidDepth.front().volume;
    }
    if (!info->askDepth.empty()) {
        bestAskVolume = info->askDepth.front().volume;
    }
    Symbols::Unlock();

    // IMPORTANT: Return 0 (failure) if prices not available yet
    // This prevents Zorro "Error 054: invalid asset price" when spot events haven't arrived
    if (d_bid <= 0.0 || d_ask <= 0.0) {
        sprintf_s(msg, "BrokerAsset: %s prices not ready yet (bid=%.6f ask=%.6f) - returning 0", Symbol, d_bid, d_ask);
        Utils::LogToFile("BROKERASSET_WAIT", msg);
        return 0;  // Signal Zorro to wait for prices
    }

    double midPrice = (d_bid + d_ask) / 2.0;
    double spread = d_ask - d_bid;

    if (pPrice) {
        *pPrice = midPrice;
    }

    if (pSpread) {
        *pSpread = spread;
    }

    if (pPip) {
        *pPip = (digits > 0) ? 1.0 / pow(10.0, digits) : 1.0;
    }

    if (pVolume) {
        *pVolume = (bestBidVolume > 0 || bestAskVolume > 0)
            ? (bestBidVolume > 0 ? bestBidVolume : bestAskVolume)
            : 0;
    }
    if (pPipCost) *pPipCost = 0;
    if (pLotAmount) *pLotAmount = 0;
    if (pMarginCost) *pMarginCost = 0;
    if (pRollLong) *pRollLong = 0;
    if (pRollShort) *pRollShort = 0;

    char resultMsg[256];
    sprintf_s(resultMsg,
              "BrokerAsset result: %s bid=%.6f ask=%.6f digits=%d mid=%.6f spread=%.6f",
              Symbol,
              d_bid,
              d_ask,
              digits,
              midPrice,
              spread);
    Utils::LogToFile("BROKERASSET_RESULT", resultMsg);

    return 1;
}

DLLFUNC int BrokerTime(DATE* pTime) {
    SYSTEMTIME st;
    GetSystemTime(&st);

    if (pTime) {
        SystemTimeToVariantTime(&st, pTime);
    }

    // Check if WebSocket is connected and logged in
    if (!G.wsConnected || !G.loginCompleted) {
        return 0; // Error - not connected
    }

    // Check if current symbol's market is open using tradingMode from server
    if (!G.currentSymbol.empty()) {
        SymbolInfo* info = Symbols::GetSymbol(G.currentSymbol);
        if (info) {
            // cTrader tradingMode values:
            // ENABLED (0) = market open for trading
            // DISABLED (1/2) = market closed
            // CLOSE_ONLY_MODE (3) = can only close existing positions
            if (info->tradingMode == TradingMode::ENABLED) {
                return 2; // Market open (2 = open, 1 = closed but allow orders)
            } else if (info->tradingMode == TradingMode::CLOSE_ONLY_MODE) {
                return 1; // Market closed, but allow closing existing positions
            } else {
                return 0; // Market closed (DISABLED)
            }
        }
    }

    // Periodic unrealized P&L refresh (every 3 seconds, rate limit safe)
    ULONGLONG now = GetTickCount64();
    constexpr ULONGLONG PNL_REFRESH_INTERVAL_MS = 3000; // 3 seconds
    if (G.CTraderAccountId > 0 &&
        (now - G.lastPnLRequestMs) > PNL_REFRESH_INTERVAL_MS &&
        !G.pnlRequestPending) {

        EnterCriticalSection(&G.cs_trades);
        int openPositions = 0;
        for (const auto& pair : G.openTrades) {
            if (!pair.second.closed) openPositions++;
        }
        LeaveCriticalSection(&G.cs_trades);

        // Only request if we have open positions
        if (openPositions > 0) {
            Account::RequestPositionPnL(G.CTraderAccountId);
        }
    }

    // Default: if no symbol info, assume market open (for backward compatibility)
    return 2;
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
    switch (Mode) {

    // GET commands
    case GET_TIME: {
        // Return last server timestamp in OLE DATE format
        // cTrader timestamps are in Unix milliseconds, convert to OLE DATE
        if (G.lastServerTimestamp > 0) {
            // Unix timestamp (ms) to OLE DATE: (ms / 86400000) + 25569
            // 25569 = days between 1900-01-01 and 1970-01-01
            double oleDate = (G.lastServerTimestamp / 86400000.0) + 25569.0;
            return oleDate;
        }
        return 0.0;
    }

    case GET_STOPLEVEL: {
        // Stop level in pips - cTrader doesn't provide this, use default
        // Typical forex brokers require 1-5 pips minimum
        return 5.0; // 5 pips minimum stop distance
    }

    case GET_TRADEALLOWED: {
        // Check if trading is allowed for current symbol
        // p is pointer to symbol name (or use G.currentSymbol if p is 0)
        const char* symbol = (const char*)p;
        if (!symbol && !G.currentSymbol.empty()) {
            symbol = G.currentSymbol.c_str();
        }
        if (!symbol) return 0.0;

        // Check if symbol exists and is subscribed
        SymbolInfo* info = Symbols::GetSymbolByIdOrName(symbol);
        if (info && info->bid > 0 && info->ask > 0) {
            return 1.0; // Trading allowed
        }
        return 0.0; // Symbol not available
    }

    case GET_MARGININIT: {
        // Initial margin requirement - calculate from leverage
        // For forex: margin = (lot size × contract size) / leverage
        // Assume 1:100 leverage for cTrader demo accounts
        const char* symbol = (const char*)p;
        if (!symbol && !G.currentSymbol.empty()) {
            symbol = G.currentSymbol.c_str();
        }
        if (!symbol) return 100.0; // Default 100 per lot

        // For forex pairs, typical margin is ~1000 per lot at 1:100 leverage
        // For indices/commodities, it varies
        return 1000.0; // $1000 margin per 1.0 lot at 1:100 leverage
    }

    case GET_MARGINMAINTAIN: {
        // Maintenance margin - typically same as initial for most brokers
        return BrokerCommand(GET_MARGININIT, p);
    }

    case GET_MARGINHEDGED: {
        // Hedged margin - cTrader uses netting, so return 0
        return 0.0;
    }

    case GET_COMPLIANCE: {
        // Trading restrictions: 0 = none, 1 = long only, 2 = short only
        return 0.0; // No restrictions
    }

    case GET_SERVERSTATE:
        return G.wsConnected ? 2.0 : 0.0; // 2 = connected and ready, 0 = disconnected

    case GET_NTRADES: {
        EnterCriticalSection(&G.cs_trades);
        int count = 0;
        for (const auto& pair : G.openTrades) {
            if (!pair.second.closed) count++;
        }
        LeaveCriticalSection(&G.cs_trades);
        return (double)count;
    }

    case GET_POSITION: {
        // p is pointer to symbol name
        const char* symbol = (const char*)p;
        if (!symbol) return 0.0;
        EnterCriticalSection(&G.cs_trades);
        double netPosition = 0.0;
        for (const auto& pair : G.openTrades) {
            const Trade& trade = pair.second;
            if (!trade.closed && trade.symbol == symbol) {
                netPosition += trade.amount;
            }
        }
        LeaveCriticalSection(&G.cs_trades);
        return netPosition / 1000.0; // Convert to lots
    }

    case GET_AVGENTRY: {
        // p is pointer to symbol name
        const char* symbol = (const char*)p;
        if (!symbol) return 0.0;
        EnterCriticalSection(&G.cs_trades);
        double totalVolume = 0.0;
        double weightedPrice = 0.0;
        for (const auto& pair : G.openTrades) {
            const Trade& trade = pair.second;
            if (!trade.closed && trade.symbol == symbol) {
                double volume = abs(trade.amount);
                totalVolume += volume;
                weightedPrice += trade.openPrice * volume;
            }
        }
        LeaveCriticalSection(&G.cs_trades);
        return (totalVolume > 0) ? (weightedPrice / totalVolume) : 0.0;
    }

    case GET_ACCOUNT: {
        char* accountStr = (char*)p;
        if (accountStr && G.CTraderAccountId > 0) {
            sprintf_s(accountStr, 256, "cTrader-%lld", G.CTraderAccountId);
            return 1.0;
        }
        return 0.0;
    }

    case GET_DIGITS: {
        const char* symbol = (const char*)p;
        if (!symbol) return 5.0; // Default for forex
        SymbolInfo* info = Symbols::GetSymbolByIdOrName(symbol);
        return info ? (double)info->digits : 5.0;
    }

    case GET_MINLOT: {
        // cTrader API doesn't provide this, use sensible defaults
        return 0.01; // 0.01 lots = 1000 units
    }

    case GET_MAXLOT: {
        // cTrader API doesn't provide this, use sensible defaults
        return 100.0; // 100 lots
    }

    case GET_LOTSTEP: {
        // cTrader API doesn't provide this, use sensible defaults
        return 0.01; // 0.01 lot step
    }

    // SET commands
    case SET_SYMBOL: {
        // Set current symbol for subsequent commands
        const char* symbol = (const char*)p;
        if (symbol) {
            G.currentSymbol = symbol;
            return 1.0;
        }
        return 0.0;
    }

    case SET_ORDERTYPE: {
        // Set order type: 0 = market, 2 = limit, 4 = stop, 8 = stop orders to broker
        G.orderType = (int)p;
        return 1.0;  // Command supported
    }

    case SET_DIAGNOSTICS:
        G.Diag = (int)p;
        return 1.0;  // Command supported

    case 139: { // SET_WAIT
        G.waitTime = (int)p;
        Network::SetReceiveTimeout(G.waitTime);
        return 1.0;  // Command supported
    }

    case SET_PRICETYPE:
        // p = 0 (ask/bid), 2 (trades), 6 (bid only)
        // cTrader uses bid/ask by default
        return 1.0;  // Command supported

    // Custom close commands
    case CMD_CLOSE_ALL_SHORT:
        if (Trading::ClosePositionsByFilter(Trading::CloseFilter::AllShort)) {
            Utils::ShowMsg("Close filter executed: ALL SHORT");
            return 1.0;
        }
        return 0.0;

    case CMD_CLOSE_ALL_LONG:
        if (Trading::ClosePositionsByFilter(Trading::CloseFilter::AllLong)) {
            Utils::ShowMsg("Close filter executed: ALL LONG");
            return 1.0;
        }
        return 0.0;

    case CMD_CLOSE_ALL_PROFIT:
        if (Trading::ClosePositionsByFilter(Trading::CloseFilter::AllProfitable)) {
            Utils::ShowMsg("Close filter executed: ALL PROFIT");
            return 1.0;
        }
        return 0.0;

    case CMD_CLOSE_ALL_LOSS:
        if (Trading::ClosePositionsByFilter(Trading::CloseFilter::AllLosing)) {
            Utils::ShowMsg("Close filter executed: ALL LOSS");
            return 1.0;
        }
        return 0.0;

    default:
        break;
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
    Utils::LogToFile("STOP_LOGOUT_REQUEST", "Stop/logout invoked by host application");
    PerformLogout("BROKERLOGOUT", true);
    return 1;
}

DLLFUNC int BrokerHistory(char* Symbol, DATE tStart, DATE tEnd,
                         int nTickMinutes, int nTicks, T6* ticks) {
    Utils::LogToFile("HISTORY_DEBUG", "===== BrokerHistory CALLED =====");

    if (!Symbol || !ticks) return 0;

    char msg[512];
    sprintf_s(msg, "BrokerHistory request: %s, %d minutes, %d ticks | tStart=%.8f tEnd=%.8f",
              Symbol, nTickMinutes, nTicks, tStart, tEnd);
    Utils::LogToFile("HISTORY_REQUEST", msg);

    // USE WEBSOCKET (REST API does not exist for cTrader history)
    Utils::LogToFile("HISTORY_DEBUG", "Using WebSocket for history");
    int actualTicks = 0;
    if (nTickMinutes <= 0) {
        Utils::LogToFile("HISTORY_DEBUG", "Calling History::GetTickData");
        actualTicks = History::GetTickData(Symbol, tStart, tEnd, nTicks, ticks);
    } else {
        Utils::LogToFile("HISTORY_DEBUG", "Calling History::RequestHistoricalData");
        actualTicks = History::RequestHistoricalData(Symbol, tStart, tEnd, nTickMinutes, nTicks, ticks);
    }

    char resultMsg[256];
    sprintf_s(resultMsg, "History function returned: %d ticks", actualTicks);
    Utils::LogToFile("HISTORY_DEBUG", resultMsg);

    if (actualTicks > 0) {
        char successMsg[256];
        sprintf_s(successMsg, "Historical data retrieved: %d ticks (requested: %d)", actualTicks, nTicks);
        Utils::LogToFile("HISTORY", successMsg);

        // Check if we got significantly less than requested
        if (nTicks > 0 && actualTicks < nTicks) {
            double percentage = (double)actualTicks / (double)nTicks * 100.0;
            if (percentage < 90.0) {  // Less than 90% received
                char warnMsg[256];
                sprintf_s(warnMsg, "BrokerHistory WARNING: Received only %d/%d bars (%.0f%%) for %s @ %dmin timeframe",
                          actualTicks, nTicks, percentage, Symbol, nTickMinutes);
                Utils::LogToFile("HISTORY_SHORTAGE", warnMsg);

                if (percentage < 70.0) {  // Less than 70% - critical shortage
                    Utils::ShowMsg("History Data Shortage", warnMsg);
                }
            }
        }

        // Log first and last tick for debugging
        if (actualTicks > 0 && ticks) {
            char tickMsg[512];
            sprintf_s(tickMsg, "HISTORY_FIRST_TICK: time=%.8f open=%.5f high=%.5f low=%.5f close=%.5f vol=%.2f",
                      ticks[0].time, ticks[0].fOpen, ticks[0].fHigh, ticks[0].fLow, ticks[0].fClose, ticks[0].fVal);
            Utils::LogToFile("HISTORY_DEBUG", tickMsg);

            if (actualTicks > 1) {
                sprintf_s(tickMsg, "HISTORY_LAST_TICK: time=%.8f open=%.5f high=%.5f low=%.5f close=%.5f vol=%.2f",
                          ticks[actualTicks-1].time, ticks[actualTicks-1].fOpen, ticks[actualTicks-1].fHigh,
                          ticks[actualTicks-1].fLow, ticks[actualTicks-1].fClose, ticks[actualTicks-1].fVal);
                Utils::LogToFile("HISTORY_DEBUG", tickMsg);
            }
        }

        return actualTicks;  // Return actual count received (e.g., 299 instead of 300)
    }

    Utils::LogToFile("HISTORY_ERROR", "Historical data request returned zero records");
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

