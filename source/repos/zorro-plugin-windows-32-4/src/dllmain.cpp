#include "../include/state.h"
#include "../include/protocol.h"
#include "../include/websocket.h"
#include "../include/auth.h"
#include "../include/symbols.h"
#include "../include/account.h"
#include "../include/trading.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include "../include/zorro_constants.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <process.h>
#include <oleauto.h>  // VariantTimeToSystemTime

// ============================================================
// Network Thread - receives messages and dispatches
// ============================================================

static unsigned __stdcall NetworkThread(void* param) {
    const int NET_BUF_SIZE = 2 * 1024 * 1024;  // 2MB for large history responses
    char* buffer = (char*)malloc(NET_BUF_SIZE);
    if (!buffer) {
        Log::Error("NET", "NetworkThread: malloc failed!");
        return 1;
    }

    Log::Info("NET", "NetworkThread started");
    ULONGLONG lastAliveLog = Utils::NowMs();

    while (G.running) {
        if (!WebSocket::IsConnected()) {
            // Log once when we notice disconnection
            static bool loggedDisconnect = false;
            if (!loggedDisconnect) {
                Log::Warn("NET", "NetworkThread: WS not connected");
                loggedDisconnect = true;
            }

            // Auto-reconnect: only if we had a successful login before
            if (G.loggedIn && !G.isReconnecting && G.reconnectAttempts < 10) {
                // Exponential backoff: 5s, 10s, 20s, 40s, 60s (capped)
                ULONGLONG delay = 5000ULL << (G.reconnectAttempts > 3 ? 3 : G.reconnectAttempts);
                if (delay > 60000) delay = 60000;
                ULONGLONG now = GetTickCount64();
                if (now - G.lastReconnectMs >= delay) {
                    G.isReconnecting = true;
                    G.lastReconnectMs = now;
                    loggedDisconnect = false;
                    Log::Info("NET", "Auto-reconnect attempt %d/%d (delay=%llums)",
                              G.reconnectAttempts + 1, 10, delay);

                    // Soft reset connection state (preserve trades/symbols)
                    StateInit::ResetConnection();
                    WebSocket::Disconnect();

                    const char* host = G.hostOverride.empty()
                        ? (G.env == Env::Live ? CTRADER_HOST_LIVE : CTRADER_HOST_DEMO)
                        : G.hostOverride.c_str();

                    Auth::LoadToken();

                    bool ok = WebSocket::Connect(host, CTRADER_WS_PORT);
                    if (ok) ok = Auth::ApplicationAuth();
                    if (ok) {
                        if (!Auth::AccountAuth()) {
                            Log::Warn("NET", "Reconnect: AccountAuth failed, refreshing token...");
                            WebSocket::Disconnect();
                            if (Auth::RefreshAccessToken()) {
                                ok = WebSocket::Connect(host, CTRADER_WS_PORT) && Auth::ApplicationAuth() && Auth::AccountAuth();
                            } else {
                                ok = false;
                            }
                        }
                    }

                    if (ok) {
                        // Resubscribe and reconcile
                        Symbols::BatchResubscribe();
                        Trading::RequestReconcile();
                        G.reconnectAttempts = 0;
                        G.lastHeartbeatMs = GetTickCount64();
                        Log::Info("NET", "Auto-reconnect SUCCESSFUL");
                    } else {
                        G.reconnectAttempts++;
                        WebSocket::Disconnect();
                        Log::Error("NET", "Auto-reconnect FAILED (attempt %d/10)", G.reconnectAttempts);
                        if (G.reconnectAttempts >= 10) {
                            Log::Error("NET", "Auto-reconnect exhausted, giving up. Manual BrokerLogin required.");
                        }
                    }
                    G.isReconnecting = false;
                }
            }

            Sleep(100);
            continue;
        }

        // Send heartbeat if needed
        ULONGLONG now = Utils::NowMs();
        if (now - G.lastHeartbeatMs > PING_INTERVAL_MS) {
            const char* hb = Protocol::BuildMessage(Utils::NextMsgId(),
                                                    PayloadType::HeartbeatEvent, "");
            bool sent = WebSocket::Send(hb);
            if (!sent) {
                Log::Warn("NET", "Heartbeat send FAILED! wsConnected=%d hWebSocket=%p",
                          (int)G.wsConnected, (void*)G.hWebSocket);
            }
            G.lastHeartbeatMs = now;
        }

        // Periodic alive log (every 60s)
        if (now - lastAliveLog > 60000) {
            Log::Info("NET", "NetworkThread alive: wsConn=%d quotes=%d loggedIn=%d",
                      (int)G.wsConnected, G.quoteCount, (int)G.loggedIn);
            lastAliveLog = now;
        }

        // Try to receive
        int n = WebSocket::Receive(buffer, NET_BUF_SIZE);
        if (n <= 0) {
            Sleep(10);
            continue;
        }

        int pt = Protocol::ExtractPayloadType(buffer);

        // If waiting for history response, forward GetTrendbarsRes/ErrorRes
        if (G.waitingForHistory &&
            (pt == ToInt(PayloadType::GetTrendbarsRes) ||
             pt == ToInt(PayloadType::GetTickDataRes))) {
            CsLock lock(G.csHistory);
            int copyLen = (n < State::HIST_BUF_SIZE - 1) ? n : State::HIST_BUF_SIZE - 1;
            memcpy(G.historyResponseBuf, buffer, copyLen);
            G.historyResponseBuf[copyLen] = '\0';
            G.historyResponsePt = pt;
            G.historyResponseReady = true;
            continue;
        }

        // ErrorRes during history mode → also forward to history
        if (G.waitingForHistory && pt == ToInt(PayloadType::ErrorRes)) {
            CsLock lock(G.csHistory);
            int copyLen = (n < State::HIST_BUF_SIZE - 1) ? n : State::HIST_BUF_SIZE - 1;
            memcpy(G.historyResponseBuf, buffer, copyLen);
            G.historyResponseBuf[copyLen] = '\0';
            G.historyResponsePt = pt;
            G.historyResponseReady = true;
            continue;
        }

        // Trading response forwarding: ExecutionEvent, OrderErrorEvent
        if (pt == ToInt(PayloadType::ExecutionEvent)) {
            Trading::HandleExecutionEvent(buffer, n);
            continue;
        }
        if (pt == ToInt(PayloadType::OrderErrorEvent)) {
            Trading::HandleOrderErrorEvent(buffer, n);
            continue;
        }

        // ErrorRes during trading mode → forward to trading
        if (G.waitingForTrading && pt == ToInt(PayloadType::ErrorRes)) {
            CsLock lock(G.csTrading);
            int copyLen = (n < State::TRADE_BUF_SIZE - 1) ? n : State::TRADE_BUF_SIZE - 1;
            memcpy(G.tradingResponseBuf, buffer, copyLen);
            G.tradingResponseBuf[copyLen] = '\0';
            G.tradingResponsePt = pt;
            G.tradingResponseExecType = 0;
            G.tradingResponseReady = true;
            continue;
        }

        switch (pt) {
            case ToInt(PayloadType::SpotEvent):
                Symbols::HandleSpotEvent(buffer);
                break;

            case ToInt(PayloadType::MarginChangedEvent):
                Account::HandleMarginChangedEvent(buffer);
                break;

            case ToInt(PayloadType::TraderUpdateEvent):
                Account::HandleTraderUpdateEvent(buffer);
                break;

            case ToInt(PayloadType::SubscribeSpotsRes):
                Log::Diag(1, "SubscribeSpotsRes received");
                break;

            case ToInt(PayloadType::HeartbeatEvent):
                Log::Diag(2, "Heartbeat received");
                break;

            case ToInt(PayloadType::ReconcileRes):
                Trading::HandleReconcileRes(buffer);
                break;

            case ToInt(PayloadType::ErrorRes):
                Log::Error("NET", "Error from server: %s",
                          Protocol::ExtractString(buffer, "description"));
                break;

            case ToInt(PayloadType::AccountsTokenInvalidatedEvent):
                Log::Error("NET", "Token invalidated! Triggering reconnect...");
                G.wsConnected = false;  // triggers auto-reconnect with token refresh
                break;

            case ToInt(PayloadType::ClientDisconnectEvent):
                Log::Warn("NET", "Client disconnect event, triggering reconnect...");
                G.wsConnected = false;  // triggers auto-reconnect
                break;

            default:
                Log::Diag(1, "Unhandled payloadType: %d", pt);
                break;
        }
    }

    free(buffer);
    Log::Info("NET", "NetworkThread exiting (G.running=%d)", (int)G.running);
    return 0;
}

static void StartNetworkThread() {
    if (G.hThread) return;
    G.running = true;
    G.hThread = (HANDLE)_beginthreadex(NULL, 0, NetworkThread, NULL, 0, NULL);
}

static void StopNetworkThread() {
    G.running = false;
    if (G.hThread) {
        WaitForSingleObject(G.hThread, 3000);
        CloseHandle(G.hThread);
        G.hThread = NULL;
    }
}

// ============================================================
// DllMain
// ============================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Get DLL directory
        GetModuleFileNameA(hModule, G.dllDir, MAX_PATH);
        char* lastSlash = strrchr(G.dllDir, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';

        StateInit::Init();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        StopNetworkThread();
        WebSocket::Disconnect();
        StateInit::Destroy();
    }
    return TRUE;
}

// ============================================================
// Broker API Exports
// ============================================================

DLLFUNC int BrokerOpen(char* Name, int(__cdecl* fpMessage)(const char*),
                       int(__cdecl* fpProgress)(intptr_t)) {
    BrokerMessage = fpMessage;
    BrokerProgress = fpProgress;

    // Name==NULL signals shutdown (Zorro is closing)
    if (!Name) {
        Log::Info("BROKER", "BrokerOpen(NULL): graceful shutdown");
        StopNetworkThread();
        WebSocket::Disconnect();
        G.loggedIn = false;
        G.loginCompleted = false;
        return 0;
    }

    strcpy_s(Name, 32, PLUGIN_NAME);

    // Debug: write version + dllDir + log test to known path
    {
        char dbgPath[MAX_PATH];
        sprintf_s(dbgPath, "%scTrader_debug.txt", G.dllDir);
        FILE* dbg = nullptr;
        fopen_s(&dbg, dbgPath, "w");
        if (dbg) {
            fprintf(dbg, "BrokerOpen v%s\ndllDir=[%s]\n", PLUGIN_VERSION, G.dllDir);

            // Test: can we open cTrader.log?
            char logPath[MAX_PATH];
            sprintf_s(logPath, "%scTrader.log", G.dllDir);
            FILE* logTest = nullptr;
            errno_t logErr = fopen_s(&logTest, logPath, "a");
            fprintf(dbg, "log fopen_s(\"%s\",\"a\") = %d (f=%p)\n", logPath, (int)logErr, (void*)logTest);
            if (logTest) {
                fprintf(logTest, "[DEBUG] direct write test from BrokerOpen v%s\n", PLUGIN_VERSION);
                fclose(logTest);
                fprintf(dbg, "log direct write OK\n");
            }

            // Test: is csLog initialized?
            fprintf(dbg, "csLog ptr=%p\n", (void*)&G.csLog);
            BOOL tryResult = TryEnterCriticalSection(&G.csLog);
            fprintf(dbg, "TryEnterCriticalSection(csLog) = %d\n", tryResult);
            if (tryResult) {
                LeaveCriticalSection(&G.csLog);
                fprintf(dbg, "csLog OK - enter+leave succeeded\n");
            } else {
                fprintf(dbg, "csLog LOCKED by another thread!\n");
            }

            fclose(dbg);
        }
    }

    Log::Info("BROKER", "BrokerOpen - %s v%s", PLUGIN_NAME, PLUGIN_VERSION);

    // Show version in Zorro message window
    if (BrokerMessage) {
        char verMsg[128];
        sprintf_s(verMsg, "cTrader v%s loaded", PLUGIN_VERSION);
        BrokerMessage(verMsg);
    }

    return PLUGIN_TYPE;
}

DLLFUNC int BrokerLogin(char* User, char* Pwd, char* Type, char* Accounts) {
    if (!User) {
        // Logout
        Log::Info("BROKER", "BrokerLogin: logout requested");
        StopNetworkThread();
        WebSocket::Disconnect();
        G.loggedIn = false;
        G.loginCompleted = false;
        return 0;
    }

    // === Reconnect path: if we had a previous successful login ===
    if (G.loginCompleted) {
        // If already connected and logged in, just confirm — don't tear down!
        // Zorro calls BrokerLogin when transitioning from lookback to live trading.
        if (G.loggedIn && WebSocket::IsConnected()) {
            Log::Info("BROKER", "BrokerLogin: already connected (quotes=%d), returning 1", G.quoteCount);
            if (Accounts) {
                sprintf_s(Accounts, 1024, "%lld", G.accountId);
            }
            return 1;
        }

        Log::Info("BROKER", "BrokerLogin RECONNECT: user=%s accountId=%lld (preserving trades/symbols)",
                  User, G.accountId);

        // Wait for NetworkThread auto-reconnect to finish if active
        for (int i = 0; i < 50 && G.isReconnecting; i++) Sleep(100);

        // Stop network thread before reconnecting
        StopNetworkThread();
        WebSocket::Disconnect();
        G.loggedIn = false;

        // Soft reset: preserve trades, symbols, account data
        StateInit::ResetConnection();

        // Fast reconnect: use stored credentials (no CSV re-parse, no Auth::Login)
        const char* host = G.hostOverride.empty()
            ? (G.env == Env::Live ? CTRADER_HOST_LIVE : CTRADER_HOST_DEMO)
            : G.hostOverride.c_str();

        // Reload token in case it was refreshed and saved to disk
        Auth::LoadToken();

        if (!WebSocket::Connect(host, CTRADER_WS_PORT)) {
            Log::Error("BROKER", "Reconnect: WebSocket connect failed");
            return 0;
        }

        if (!Auth::ApplicationAuth()) {
            Log::Error("BROKER", "Reconnect: AppAuth failed");
            WebSocket::Disconnect();
            return 0;
        }

        if (!Auth::AccountAuth()) {
            // Token may have expired — try refresh and retry
            Log::Warn("BROKER", "Reconnect: AccountAuth failed, refreshing token...");
            WebSocket::Disconnect();

            if (!Auth::RefreshAccessToken()) {
                Log::Error("BROKER", "Reconnect: Token refresh failed");
                return 0;
            }

            if (!WebSocket::Connect(host, CTRADER_WS_PORT) || !Auth::ApplicationAuth()) {
                Log::Error("BROKER", "Reconnect: Re-connect after refresh failed");
                WebSocket::Disconnect();
                return 0;
            }

            if (!Auth::AccountAuth()) {
                Log::Error("BROKER", "Reconnect: AccountAuth still fails after refresh");
                WebSocket::Disconnect();
                return 0;
            }
        }

        G.loggedIn = true;
        G.reconnectAttempts = 0;
        G.isReconnecting = false;

        // Try to reload symbol list, but continue with cached symbols if it fails
        bool symOk = Symbols::RequestSymbolList();
        if (symOk) {
            Symbols::RequestSymbolDetails();
        } else {
            CsLock lock(G.csSymbols);
            int existingSyms = (int)G.symbols.size();
            if (existingSyms > 0) {
                Log::Warn("BROKER", "Reconnect: symbol list failed but have %d cached symbols, continuing", existingSyms);
            } else {
                Log::Error("BROKER", "Reconnect: no symbols available");
                WebSocket::Disconnect();
                G.loggedIn = false;
                return 0;
            }
        }

        // Refresh account info
        Account::RequestTraderInfo();

        // Re-reconcile positions to sync with server
        Trading::RequestReconcile();

        // Resubscribe to spot events for previously subscribed symbols
        Symbols::BatchResubscribe();

        // Restart network thread
        StartNetworkThread();

        Log::Info("BROKER", "Reconnect complete. %d symbols, %d trades.",
                  (int)G.symbols.size(), (int)G.trades.size());

        if (Accounts) {
            sprintf_s(Accounts, 1024, "%lld", G.accountId);
        }
        return 1;
    }

    // === First login path ===
    Log::Info("BROKER", "BrokerLogin FIRST: user=%s type=%s", User, Type ? Type : "demo");

    // Full reset for first login
    StateInit::Reset();

    // Perform login
    if (!Auth::Login(User, Pwd, Type)) {
        return 0;
    }

    // Request symbol list
    if (!Symbols::RequestSymbolList()) {
        Log::Error("BROKER", "Failed to load symbol list");
        WebSocket::Disconnect();
        return 0;
    }

    // Request symbol details (batched)
    Symbols::RequestSymbolDetails();

    // Request account info
    Account::RequestTraderInfo();

    // Reconcile open positions before starting network thread
    Trading::RequestReconcile();

    // Start network thread for async events
    StartNetworkThread();

    G.loginCompleted = true;
    Log::Info("BROKER", "Login complete. %d symbols loaded.", (int)G.symbols.size());

    // Fill Accounts if provided
    if (Accounts) {
        sprintf_s(Accounts, 1024, "%lld", G.accountId);
    }

    return 1;
}

DLLFUNC int BrokerTime(DATE* pTimeGMT) {
    if (!G.loggedIn) {
        Log::Warn("TIME", "BrokerTime=0 (loggedIn=false)");
        return 0;
    }
    if (!WebSocket::IsConnected()) {
        Log::Warn("TIME", "BrokerTime=0 (WS disconnected: wsConnected=%d hWebSocket=%p)",
                  (int)G.wsConnected, (void*)G.hWebSocket);
        return 0;
    }

    if (pTimeGMT) {
        // Use system GMT time — lastServerTimestamp is unreliable because
        // SpotEvent delta updates often omit the timestamp field
        SYSTEMTIME st;
        GetSystemTime(&st);
        DATE oleDate;
        SystemTimeToVariantTime(&st, &oleDate);
        *pTimeGMT = oleDate;
    }

    if (G.quoteCount == 0) return 1;

    // If no SpotEvent received for >60 seconds, check if WS is still alive
    ULONGLONG now = GetTickCount64();
    if (G.lastQuoteRecvMs > 0 && (now - G.lastQuoteRecvMs) > 60000) {
        // WS is still connected, market is likely closed (weekend/holiday)
        // But if heartbeats are also not going through, connection may be stale
        if (G.lastHeartbeatMs > 0 && (now - G.lastHeartbeatMs) > 35000) {
            // No heartbeat in 35s (API timeout is 30s) -> connection is dead
            Log::Warn("TIME", "No heartbeat in %llums, connection stale", now - G.lastHeartbeatMs);
            G.wsConnected = false;  // trigger reconnect
            return 0;
        }
        return 1;  // connected but no live data (market closed)
    }

    return 2;
}

DLLFUNC int BrokerAsset(char* Asset, double* pPrice, double* pSpread,
                        double* pVolume, double* pPip, double* pPipCost,
                        double* pLotAmount, double* pMarginCost,
                        double* pRollLong, double* pRollShort) {
    if (!Asset || !G.loggedIn) return 0;

    // Subscribe if not already
    Symbols::Subscribe(Asset);

    SymbolInfo sym;
    if (!Symbols::GetSymbol(Asset, sym)) return 0;

    if (sym.bid <= 0.0 && sym.ask <= 0.0) return 0;

    if (pPrice) *pPrice = sym.ask > 0.0 ? sym.ask : sym.bid;
    if (pSpread) *pSpread = (sym.ask > 0.0 && sym.bid > 0.0) ? (sym.ask - sym.bid) : 0.0;

    if (pPip) {
        // pipPosition: e.g. 4 means pip = 0.0001
        *pPip = 1.0 / pow(10.0, (double)sym.pipPosition);
    }

    if (pLotAmount) {
        // Micro-lot convention: 1 Zorro lot = 0.01 standard lots
        // Amount=1 → 0.01 std lot, Amount=100 → 1.0 std lot
        // lotSize/100 = cTrader volume per Zorro lot, /100 again = currency units
        *pLotAmount = (double)sym.lotSize / 10000.0;
    }

    if (pPipCost) {
        // PipCost = value of 1 pip per 1 Zorro lot in quote currency
        double pip = 1.0 / pow(10.0, (double)sym.pipPosition);
        double lotAmount = (double)sym.lotSize / 10000.0;
        *pPipCost = pip * lotAmount;
    }

    if (pMarginCost) {
        // Negative value = leverage ratio (e.g. -500 means 500:1)
        // leverageInCents from TraderRes: 50000 = 500:1
        if (G.leverageInCents > 0) {
            *pMarginCost = -(double)G.leverageInCents / 100.0;
        } else {
            *pMarginCost = -100.0;  // safe fallback: 100:1
        }
    }

    if (pRollLong) *pRollLong = sym.swapLong;
    if (pRollShort) *pRollShort = sym.swapShort;

    G.currentSymbol = Asset;

    Log::Diag(1, "ASSET %s: lotSize=%lld pip=%d LotAmt=%.1f PipCost=%.5f MCost=%.1f",
              Asset, sym.lotSize, sym.pipPosition,
              pLotAmount ? *pLotAmount : -1.0,
              pPipCost ? *pPipCost : -1.0,
              pMarginCost ? *pMarginCost : 0.0);

    return 1;
}

DLLFUNC int BrokerAccount(char* Account, double* pBalance, double* pTradeVal,
                          double* pMarginVal) {
    if (!G.loggedIn) return 0;

    if (pBalance) *pBalance = G.balance;
    if (pTradeVal) *pTradeVal = G.equity - G.balance;  // unrealized P&L
    if (pMarginVal) *pMarginVal = G.margin;

    return 1;
}

DLLFUNC int BrokerBuy2(char* Asset, int Amount, double StopDist, double Limit,
                       double* pPrice, int* pFill) {
    Log::Info("BROKER", "BrokerBuy2 ENTER: Asset=%s Amount=%d StopDist=%.5f Limit=%.5f pPrice=%p pFill=%p",
              Asset ? Asset : "NULL", Amount, StopDist, Limit, pPrice, pFill);
    int result = Trading::BuyOrder(Asset, Amount, StopDist, Limit, pPrice, pFill);
    Log::Info("BROKER", "BrokerBuy2 RETURN: %d pPrice=%.5f pFill=%d",
              result, pPrice ? *pPrice : 0.0, pFill ? *pFill : -999);
    return result;
}

DLLFUNC int BrokerSell2(int TradeID, int Amount, double Limit,
                        double* pClose, double* pCost, double* pProfit, int* pFill) {
    Log::Info("BROKER", "BrokerSell2 ENTER: TradeID=%d Amount=%d Limit=%.5f", TradeID, Amount, Limit);
    int result = Trading::SellOrder(TradeID, Amount, Limit, pClose, pCost, pProfit, pFill);
    Log::Info("BROKER", "BrokerSell2 RETURN: %d pClose=%.5f pProfit=%.2f pFill=%d",
              result, pClose ? *pClose : 0.0, pProfit ? *pProfit : 0.0, pFill ? *pFill : -999);
    return result;
}

DLLFUNC int BrokerTrade(int TradeID, double* pOpen, double* pClose,
                        double* pCost, double* pProfit) {
    int result = Trading::GetTradeStatus(TradeID, pOpen, pClose, pCost, pProfit);
    if (result > 0) {
        Log::Info("BROKER", "BrokerTrade: ID=%d result=%d Open=%.5f Close=%.5f Profit=%.2f",
                  TradeID, result, pOpen ? *pOpen : 0.0, pClose ? *pClose : 0.0, pProfit ? *pProfit : 0.0);
    } else if (result == -1) {
        Log::Info("BROKER", "BrokerTrade: ID=%d CLOSED (result=-1)", TradeID);
    } else {
        Log::Warn("BROKER", "BrokerTrade: ID=%d result=%d (unexpected)", TradeID, result);
    }
    return result;
}

// Map nTickMinutes to cTrader TrendbarPeriod
static int MinutesToPeriod(int minutes) {
    switch (minutes) {
        case 1:     return 1;   // M1
        case 2:     return 2;   // M2
        case 3:     return 3;   // M3
        case 4:     return 4;   // M4
        case 5:     return 5;   // M5
        case 10:    return 6;   // M10
        case 15:    return 7;   // M15
        case 30:    return 8;   // M30
        case 60:    return 9;   // H1
        case 240:   return 10;  // H4
        case 720:   return 11;  // H12
        case 1440:  return 12;  // D1
        case 10080: return 13;  // W1
        case 43200: return 14;  // MN1
        default:
            Log::Warn("HIST", "Unknown nTickMinutes=%d, falling back to H1", minutes);
            return 9;
    }
}

// ============================================================
// History file cache - read/write T6 files in History\ folder
// ============================================================

// Extract year from OLE DATE
static int OleDateToYear(DATE oleDate) {
    SYSTEMTIME st = {0};
    VariantTimeToSystemTime(oleDate, &st);
    return st.wYear;
}

// Build cache filepath: History\{Asset}_{year}.t6
static void BuildHistoryCachePath(char* out, int maxLen, const char* asset, int year) {
    // Strip slashes from asset name: "EUR/USD" -> "EURUSD"
    char cleanAsset[64] = {0};
    int j = 0;
    for (const char* p = asset; *p && j < 62; p++) {
        if (*p != '/' && *p != '\\' && *p != ' ')
            cleanAsset[j++] = *p;
    }
    cleanAsset[j] = '\0';
    sprintf_s(out, maxLen, "History\\%s_%d.t6", cleanAsset, year);
}

// Read T6 bars from History cache file
// Returns bars matching [tStart, tEnd] range, newest first, up to nTicks
static int ReadHistoryCache(const char* asset, DATE tStart, DATE tEnd,
                            int nTickMinutes, int nTicks, T6* bars) {
    if (nTickMinutes == 0) return 0;  // no tick cache yet

    int year = OleDateToYear(tEnd);
    char path[MAX_PATH];
    BuildHistoryCachePath(path, MAX_PATH, asset, year);

    FILE* f = nullptr;
    fopen_s(&f, path, "rb");
    if (!f) return 0;

    // Get file size to determine bar count
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    int totalInFile = (int)(fileSize / sizeof(T6));
    if (totalInFile <= 0) { fclose(f); return 0; }

    // Allocate temp buffer for file contents
    int readCount = totalInFile;
    if (readCount > 500000) readCount = 500000;  // safety limit ~16MB
    T6* fileBars = (T6*)malloc(readCount * sizeof(T6));
    if (!fileBars) { fclose(f); return 0; }

    int nRead = (int)fread(fileBars, sizeof(T6), readCount, f);
    fclose(f);

    if (nRead <= 0) { free(fileBars); return 0; }

    // File is newest-first (index 0 = newest). Filter by time range.
    int count = 0;
    for (int i = 0; i < nRead && count < nTicks; i++) {
        if (fileBars[i].time >= tStart && fileBars[i].time <= tEnd) {
            bars[count++] = fileBars[i];
        }
    }

    free(fileBars);

    if (count > 0) {
        Log::Info("HIST", "Cache HIT: %s %d bars from %s (file has %d total)",
                  asset, count, path, nRead);
    }
    return count;
}

// Write T6 bars to History cache file
// Merges with existing file data to keep the largest dataset
static void WriteHistoryCache(const char* asset, T6* bars, int count) {
    if (count <= 0) return;

    int year = OleDateToYear(bars[0].time);  // newest bar = index 0
    char path[MAX_PATH];
    BuildHistoryCachePath(path, MAX_PATH, asset, year);

    // Read existing file to check size
    FILE* existing = nullptr;
    fopen_s(&existing, path, "rb");
    int existingCount = 0;
    T6* existingBars = nullptr;

    if (existing) {
        fseek(existing, 0, SEEK_END);
        long fileSize = ftell(existing);
        existingCount = (int)(fileSize / sizeof(T6));
        fseek(existing, 0, SEEK_SET);

        if (existingCount > count) {
            // Existing file has MORE bars - merge: keep existing, append newer
            existingBars = (T6*)malloc(existingCount * sizeof(T6));
            if (existingBars) {
                fread(existingBars, sizeof(T6), existingCount, existing);
            }
        }
        fclose(existing);
    }

    FILE* f = nullptr;
    fopen_s(&f, path, "wb");
    if (!f) {
        Log::Warn("HIST", "Cache WRITE failed: %s", path);
        if (existingBars) free(existingBars);
        return;
    }

    if (existingBars && existingCount > count) {
        // Merge: find where new data starts relative to existing
        // Both are newest-first. Find overlap point.
        DATE oldestNew = bars[count - 1].time;
        DATE newestExisting = existingBars[0].time;

        if (bars[0].time > newestExisting) {
            // New data extends beyond existing - write new data first, then old non-overlapping
            fwrite(bars, sizeof(T6), count, f);
            for (int i = 0; i < existingCount; i++) {
                if (existingBars[i].time < oldestNew) {
                    fwrite(&existingBars[i], sizeof(T6), 1, f);
                }
            }
            Log::Info("HIST", "Cache MERGED: %s new=%d + existing older bars -> %s", asset, count, path);
        } else {
            // Existing data is newer or same - just keep existing
            fwrite(existingBars, sizeof(T6), existingCount, f);
            Log::Info("HIST", "Cache KEPT: %s existing %d bars (newer) in %s", asset, existingCount, path);
        }
        free(existingBars);
    } else {
        // No existing file or new data has more bars - write new data
        fwrite(bars, sizeof(T6), count, f);
        Log::Info("HIST", "Cache WROTE: %s %d bars -> %s", asset, count, path);
        if (existingBars) free(existingBars);
    }

    fclose(f);
}

// Helper: wait for history response from NetworkThread (via shared buffer)
static bool WaitForHistoryResponse(int timeoutMs) {
    ULONGLONG start = Utils::NowMs();
    while (Utils::NowMs() - start < (ULONGLONG)timeoutMs) {
        if (G.historyResponseReady) return true;
        Sleep(10);
        if (BrokerProgress) BrokerProgress(1);
    }
    return false;
}

// Raw tick: just time + price (used for ASK tick temp storage and BID intermediate)
struct RawTick {
    double oleTime;
    float price;
};

// FetchRawTicks: fetch tick data from cTrader API (shared helper for BID and ASK)
// tickType: 1=BID, 2=ASK
// Returns number of ticks filled into outTicks[] (newest first at index 0)
static int FetchRawTicks(SymbolInfo& sym, long long startMs, long long endMs,
                         int maxTicks, int tickType, RawTick* outTicks) {
    int totalTicks = 0;
    long long chunkEnd = endMs;

    while (chunkEnd > startMs && totalTicks < maxTicks) {
        long long chunkStart = startMs;

        char payload[512];
        sprintf_s(payload,
            "\"ctidTraderAccountId\":%lld,\"symbolId\":%lld,"
            "\"type\":%d,\"fromTimestamp\":%lld,\"toTimestamp\":%lld",
            G.accountId, sym.symbolId, tickType, chunkStart, chunkEnd);

        const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                                 PayloadType::GetTickDataReq, payload);

        Log::Info("HIST", "RawTicks(type=%d) request: sym=%s from=%lld to=%lld",
                  tickType, sym.name.c_str(), chunkStart, chunkEnd);

        {
            CsLock lock(G.csHistory);
            G.historyResponseReady = false;
            G.historyResponsePt = 0;
            G.historyResponseBuf[0] = '\0';
            G.waitingForHistory = true;
        }

        if (!WebSocket::Send(msg)) {
            Log::Error("HIST", "RawTicks(type=%d) send failed!", tickType);
            G.waitingForHistory = false;
            break;
        }

        bool gotResponse = WaitForHistoryResponse(15000);
        G.waitingForHistory = false;

        if (!gotResponse) {
            Log::Warn("HIST", "RawTicks(type=%d) timeout (no response in 15s)", tickType);
            break;
        }

        CsLock lock(G.csHistory);
        int pt = G.historyResponsePt;

        if (pt == ToInt(PayloadType::ErrorRes)) {
            Log::Error("HIST", "RawTicks(type=%d) server error: %s",
                      tickType, Protocol::ExtractString(G.historyResponseBuf, "description"));
            break;
        }

        if (pt != ToInt(PayloadType::GetTickDataRes)) {
            Log::Warn("HIST", "RawTicks(type=%d) unexpected response: %d", tickType, pt);
            break;
        }

        // Parse tickData array
        const char* arr = Protocol::ExtractArray(G.historyResponseBuf, "tickData");
        if (!arr || *arr == '\0' || (*arr == '[' && *(arr + 1) == ']')) {
            Log::Info("HIST", "RawTicks(type=%d) empty response", tickType);
            break;
        }

        int count = Protocol::CountArrayElements(arr);
        Log::Info("HIST", "RawTicks(type=%d) chunk: %d ticks", tickType, count);

        // Delta decode ticks
        // Server returns ticks newest-first: first tick is absolute (newest),
        // subsequent deltas are NEGATIVE (going backward in time)
        long long absTimestamp = 0;
        long long absPrice = 0;
        long long lastTimestamp = 0;

        for (int i = 0; i < count && totalTicks < maxTicks; i++) {
            const char* elem = Protocol::GetArrayElement(arr, i);
            if (!elem || !*elem) continue;

            long long ts = Protocol::ExtractInt64(elem, "timestamp");
            long long tick = Protocol::ExtractInt64(elem, "tick");

            if (i == 0) {
                absTimestamp = ts;
                absPrice = tick;
            } else {
                absTimestamp += ts;
                absPrice += tick;
            }

            outTicks[totalTicks].oleTime = Utils::UnixToOle(absTimestamp);
            outTicks[totalTicks].price = (float)((double)absPrice / PRICE_SCALE);

            // Debug: log first 3 BID ticks
            if (tickType == 1 && i < 3) {
                Log::Diag(1, "HIST Tick[%d] type=%d: ts=%lld tick=%lld -> absTs=%lld price=%.5f",
                          i, tickType, ts, tick, absTimestamp, outTicks[totalTicks].price);
            }

            lastTimestamp = absTimestamp;
            totalTicks++;
        }

        // Check hasMore for pagination
        bool hasMore = false;
        if (Protocol::HasField(G.historyResponseBuf, "hasMore")) {
            const char* hm = Protocol::ExtractString(G.historyResponseBuf, "hasMore");
            hasMore = (hm && strcmp(hm, "true") == 0);
        }

        if (hasMore && totalTicks < maxTicks && lastTimestamp > 0) {
            // Oldest tick timestamp - 1ms for next page
            chunkEnd = lastTimestamp - 1;
            Log::Info("HIST", "RawTicks(type=%d) hasMore, next chunkEnd=%lld (total=%d)",
                      tickType, chunkEnd, totalTicks);
        } else {
            break;
        }
    }

    Log::Info("HIST", "RawTicks(type=%d) %s returned %d ticks",
              tickType, sym.name.c_str(), totalTicks);
    return totalTicks;
}

// MergeSpread: for each BID tick, find the closest ASK tick (time <= bid_time)
// and set fVal = askPrice - bidPrice. Both arrays are newest-first (descending time).
// O(n) time, O(1) extra memory.
static void MergeSpread(T6* bars, int nBars, RawTick* askTicks, int nAskTicks) {
    if (nAskTicks == 0) return;

    int askIdx = 0;
    for (int i = 0; i < nBars; i++) {
        double bidTime = bars[i].time;

        // Advance ASK pointer: skip ASK ticks that are newer than this BID tick
        // (both arrays sorted newest-first = time decreasing with index)
        while (askIdx < nAskTicks && askTicks[askIdx].oleTime > bidTime) {
            askIdx++;
        }

        if (askIdx < nAskTicks) {
            float spread = askTicks[askIdx].price - bars[i].fClose;
            bars[i].fVal = (spread > 0.0f) ? spread : 0.0f;
        }
        // If no matching ASK tick found, fVal stays 0.0f
    }
}

// Fetch tick data: BID ticks + ASK ticks, merge spread
// Returns number of ticks filled into bars[] (newest first at index 0)
static int FetchTickData(SymbolInfo& sym, long long startMs, long long endMs,
                         int nTicks, T6* bars) {
    // Step 1: Fetch BID ticks into temp RawTick array
    RawTick* bidRaw = (RawTick*)malloc(nTicks * sizeof(RawTick));
    if (!bidRaw) return 0;

    int nBid = FetchRawTicks(sym, startMs, endMs, nTicks, 1, bidRaw);

    if (nBid == 0) {
        free(bidRaw);
        return 0;
    }

    // Convert BID raw ticks to T6 bars
    for (int i = 0; i < nBid; i++) {
        memset(&bars[i], 0, sizeof(T6));
        bars[i].time   = bidRaw[i].oleTime;
        bars[i].fHigh  = bidRaw[i].price;
        bars[i].fLow   = bidRaw[i].price;
        bars[i].fOpen  = bidRaw[i].price;
        bars[i].fClose = bidRaw[i].price;
        bars[i].fVal   = 0.0f;   // MergeSpread will set this
        bars[i].fVol   = 1.0f;
    }

    free(bidRaw);

    // Step 2: Fetch ASK ticks
    RawTick* askRaw = (RawTick*)malloc(nTicks * sizeof(RawTick));
    if (askRaw) {
        int nAsk = FetchRawTicks(sym, startMs, endMs, nTicks, 2, askRaw);

        if (nAsk > 0) {
            Log::Info("HIST", "ASK ticks fetched: %d (BID: %d)", nAsk, nBid);
            // Step 3: Merge spread into BID bars
            MergeSpread(bars, nBid, askRaw, nAsk);
        }

        free(askRaw);
    }

    Log::Info("HIST", "TickData %s returned %d ticks (with spread)", sym.name.c_str(), nBid);
    return nBid;
}

DLLFUNC int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd,
                           int nTickMinutes, int nTicks, void* ticks) {
    Log::Info("HIST", ">>> BrokerHistory2 ENTRY: Asset=%s nTickMin=%d nTicks=%d ticks=%p loggedIn=%d",
              Asset ? Asset : "NULL", nTickMinutes, nTicks, ticks, (int)G.loggedIn);

    if (!Asset || !ticks || !G.loggedIn || nTicks <= 0) {
        Log::Info("HIST", "<<< BrokerHistory2 EARLY EXIT 0 (Asset=%p ticks=%p loggedIn=%d nTicks=%d)",
                  (void*)Asset, ticks, (int)G.loggedIn, nTicks);
        return 0;
    }

    // Try reading from History cache first (bar data only, not ticks)
    if (nTickMinutes > 0) {
        int cached = ReadHistoryCache(Asset, tStart, tEnd, nTickMinutes, nTicks, (T6*)ticks);
        if (cached > 0) {
            Log::Info("HIST", "<<< BrokerHistory2 from cache: %s returned %d bars", Asset, cached);
            return cached;
        }
    }

    // Get symbol info
    SymbolInfo sym;
    if (!Symbols::GetSymbol(Asset, sym)) {
        Log::Error("BROKER", "History: symbol %s not found", Asset);
        return 0;
    }

    // Convert OLE DATE to Unix milliseconds
    long long startMs = Utils::OleToUnix(tStart);
    long long endMs = Utils::OleToUnix(tEnd);

    // Tick data: use GetTickData API
    if (nTickMinutes == 0) {
        Log::Info("HIST", "TickData request: %s from=%lld to=%lld nTicks=%d",
                  sym.name.c_str(), startMs, endMs, nTicks);
        return FetchTickData(sym, startMs, endMs, nTicks, (T6*)ticks);
    }

    int period = MinutesToPeriod(nTickMinutes);

    Log::Info("HIST", "Request: %s (id=%lld) period=%d(%dmin) from=%lld to=%lld nTicks=%d",
              sym.name.c_str(), sym.symbolId, period, nTickMinutes, startMs, endMs, nTicks);

    T6* bars = (T6*)ticks;
    int totalBars = 0;

    // Compute live spread for bar fVal (trendbars are BID-only, no ASK available)
    float liveSpread = 0.0f;
    if (sym.bid > 0.0 && sym.ask > 0.0 && sym.ask > sym.bid) {
        liveSpread = (float)(sym.ask - sym.bid);
    }
    Log::Diag(1, "HIST Live spread for bars: %.5f (bid=%.5f ask=%.5f)", liveSpread, sym.bid, sym.ask);

    // Cap bars per chunk to keep JSON response size manageable (~150KB)
    const int MAX_BARS_PER_CHUNK = 1500;

    // Compute chunk size based on bars per chunk + weekend gap (52h) buffer
    // Clamped to max 7 days (cTrader API limit)
    long long minutesNeeded = (long long)nTickMinutes * (long long)MAX_BARS_PER_CHUNK;
    long long CHUNK_MS = minutesNeeded * 60000LL + 52LL * 3600000LL;
    const long long MAX_CHUNK_MS = 7LL * 24LL * 60LL * 60LL * 1000LL;
    if (CHUNK_MS > MAX_CHUNK_MS) CHUNK_MS = MAX_CHUNK_MS;
    if (CHUNK_MS < 3600000LL) CHUNK_MS = 3600000LL;  // minimum 1 hour

    // Request chunks from newest to oldest (Zorro wants newest first at index 0)
    long long chunkEnd = endMs;
    while (chunkEnd > startMs && totalBars < nTicks) {
        long long chunkStart = chunkEnd - CHUNK_MS;
        if (chunkStart < startMs) chunkStart = startMs;

        int barsNeeded = nTicks - totalBars;
        if (barsNeeded > MAX_BARS_PER_CHUNK) barsNeeded = MAX_BARS_PER_CHUNK;

        // Build request with count parameter to limit server response
        char payload[512];
        sprintf_s(payload, "\"ctidTraderAccountId\":%lld,\"symbolId\":%lld,\"period\":%d,\"fromTimestamp\":%lld,\"toTimestamp\":%lld,\"count\":%d",
                  G.accountId, sym.symbolId, period, chunkStart, chunkEnd, barsNeeded);

        const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                                 PayloadType::GetTrendbarsReq, payload);

        Log::Info("HIST", "Chunk: from=%lld to=%lld count=%d", chunkStart, chunkEnd, barsNeeded);

        // Reset shared buffer and set waiting flag (all inside lock for C6 fix)
        {
            CsLock lock(G.csHistory);
            G.historyResponseReady = false;
            G.historyResponsePt = 0;
            G.historyResponseBuf[0] = '\0';
            G.waitingForHistory = true;
        }

        if (!WebSocket::Send(msg)) {
            Log::Error("HIST", "Send failed!");
            G.waitingForHistory = false;
            break;
        }

        // Wait for NetworkThread to deliver the response
        bool gotResponse = WaitForHistoryResponse(15000);
        G.waitingForHistory = false;

        if (!gotResponse) {
            Log::Warn("HIST", "Chunk timeout (no response in 15s)");
            break;
        }

        // Process response from shared buffer
        CsLock lock(G.csHistory);
        int pt = G.historyResponsePt;

        if (pt == ToInt(PayloadType::ErrorRes)) {
            Log::Error("HIST", "Server error: %s",
                      Protocol::ExtractString(G.historyResponseBuf, "description"));
            break;
        }

        if (pt != ToInt(PayloadType::GetTrendbarsRes)) {
            Log::Warn("HIST", "Unexpected response type: %d", pt);
            break;
        }

        // Log raw response size
        int respLen = (int)strlen(G.historyResponseBuf);
        Log::Info("HIST", "Response: %d bytes", respLen);

        // Parse trendbar array
        const char* arr = Protocol::ExtractArray(G.historyResponseBuf, "trendbar");
        if (!arr || *arr == '\0' || (*arr == '[' && *(arr + 1) == ']')) {
            Log::Info("HIST", "Empty chunk, moving to earlier period");
            chunkEnd = chunkStart;
            continue;
        }

        int count = Protocol::CountArrayElements(arr);
        Log::Info("HIST", "Chunk has %d bars", count);

        // Parse all bars from chunk (server returns oldest first)
        std::vector<T6> chunkBars;
        chunkBars.reserve(count);

        for (int i = 0; i < count; i++) {
            const char* elem = Protocol::GetArrayElement(arr, i);
            if (!elem || !*elem) continue;

            // Delta decoding: low is absolute, others relative to low
            long long low = Protocol::ExtractInt64(elem, "low");
            long long deltaOpen = Protocol::ExtractInt64(elem, "deltaOpen");
            long long deltaHigh = Protocol::ExtractInt64(elem, "deltaHigh");
            long long deltaClose = Protocol::ExtractInt64(elem, "deltaClose");
            long long volume = Protocol::ExtractInt64(elem, "volume");

            // Timestamp: utcTimestampInMinutes
            long long tsMinutes = Protocol::ExtractInt64(elem, "utcTimestampInMinutes");
            if (tsMinutes <= 0) {
                long long tsMs = Protocol::ExtractInt64(elem, "timestamp");
                tsMinutes = tsMs / 60000;
            }

            T6 bar;
            memset(&bar, 0, sizeof(T6));
            bar.fLow   = (float)((double)low / PRICE_SCALE);
            bar.fHigh  = (float)((double)(low + deltaHigh) / PRICE_SCALE);
            bar.fOpen  = (float)((double)(low + deltaOpen) / PRICE_SCALE);
            bar.fClose = (float)((double)(low + deltaClose) / PRICE_SCALE);
            bar.fVol   = (float)volume;
            bar.fVal   = liveSpread;  // spread from live SpotEvent quotes
            bar.time   = Utils::MinutesToOle(tsMinutes);

            // Debug: log first 3 bars and any anomalies
            if (i < 3) {
                Log::Diag(1, "HIST Bar[%d] raw: low=%lld dO=%lld dH=%lld dC=%lld vol=%lld tsMin=%lld",
                          i, low, deltaOpen, deltaHigh, deltaClose, volume, tsMinutes);
                Log::Diag(1, "HIST Bar[%d] T6: O=%.5f H=%.5f L=%.5f C=%.5f V=%.0f time=%.6f",
                          i, bar.fOpen, bar.fHigh, bar.fLow, bar.fClose, bar.fVol, bar.time);
            }

            chunkBars.push_back(bar);
        }

        if (!chunkBars.empty()) {
            Log::Info("HIST", "Bars: oldest=%.6f newest=%.6f",
                      chunkBars.front().time, chunkBars.back().time);
        }

        // Insert chunk bars in REVERSE order (newest first for Zorro)
        for (int i = (int)chunkBars.size() - 1; i >= 0 && totalBars < nTicks; i--) {
            bars[totalBars++] = chunkBars[i];
        }

        Log::Info("HIST", "Chunk done: %d bars added (total=%d)", count, totalBars);

        // Move to earlier period: use oldest bar's timestamp as new chunkEnd
        long long prevChunkEnd = chunkEnd;
        if (!chunkBars.empty()) {
            // Convert oldest bar's OLE time back to Unix ms, minus 1 minute
            chunkEnd = Utils::OleToUnix(chunkBars.front().time) - 60000LL;
        } else {
            chunkEnd = chunkStart;
        }
        // M7 guard: if chunkEnd didn't advance, break to avoid infinite loop
        if (chunkEnd >= prevChunkEnd) {
            Log::Warn("HIST", "Chunk did not advance (end=%lld >= prev=%lld), breaking", chunkEnd, prevChunkEnd);
            break;
        }
    }

    Log::Info("HIST", "<<< BrokerHistory2 EXIT: %s returned %d bars", Asset, totalBars);

    // Write bars to History folder for local cache
    if (totalBars > 0) {
        WriteHistoryCache(Asset, bars, totalBars);
    }

    return totalBars;
}

// Legacy BrokerHistory wrapper - Zorro may call this instead of BrokerHistory2
DLLFUNC int BrokerHistory(char* Asset, DATE tStart, DATE tEnd,
                          int nTickMinutes, int nTicks, void* ticks) {
    Log::Info("HIST", ">>> BrokerHistory (OLD API) called - forwarding to BrokerHistory2");
    return BrokerHistory2(Asset, tStart, tEnd, nTickMinutes, nTicks, ticks);
}

DLLFUNC double BrokerCommand(int Command, DWORD dwParameter) {
    switch (Command) {
        case 2000: // cycled heartbeat - called on every bar cycle
            return 1;

        case SET_DIAGNOSTICS: // 138
            G.diagLevel = (int)dwParameter;
            Log::Info("CMD", "Diagnostics level set to %d", G.diagLevel);
            return G.diagLevel;

        case SET_SYMBOL: // 132
            if (dwParameter) {
                G.currentSymbol = (char*)dwParameter;
            }
            return 1;

        case GET_SERVERSTATE: // 68
            if (!WebSocket::IsConnected()) return 0;
            if (G.quoteCount == 0) return 1;
            return 2;

        case SET_ORDERTYPE: // 157
            G.orderType = (int)dwParameter;
            return 1;

        case SET_WAIT: // 170
            G.waitTime = (int)dwParameter;
            return 1;

        case SET_PRICETYPE: // 151
            return 1;  // supported

        case GET_MINLOT: { // 23
            SymbolInfo sym;
            if (G.currentSymbol.empty() || !Symbols::GetSymbol(G.currentSymbol.c_str(), sym)) return 0;
            if (sym.lotSize <= 0) return 0;
            // LotAmount = lotSize/10000 (micro-lot). MinLot = min_units / LotAmount.
            // min_units = minVolume / 100 (cTrader cents → base units)
            double lotAmount = (double)sym.lotSize / 10000.0;
            double minLot = ((double)sym.minVolume / 100.0) / lotAmount;
            Log::Diag(1, "CMD GET_MINLOT %s: minVol=%lld lotSize=%lld lotAmt=%.0f -> %.4f",
                      G.currentSymbol.c_str(), sym.minVolume, sym.lotSize, lotAmount, minLot);
            return minLot;
        }

        case GET_LOTSTEP: { // 24
            SymbolInfo sym;
            if (G.currentSymbol.empty() || !Symbols::GetSymbol(G.currentSymbol.c_str(), sym)) return 0;
            if (sym.lotSize <= 0) return 0;
            double lotAmount = (double)sym.lotSize / 10000.0;
            return ((double)sym.stepVolume / 100.0) / lotAmount;
        }

        case GET_MAXLOT: { // 25
            SymbolInfo sym;
            if (G.currentSymbol.empty() || !Symbols::GetSymbol(G.currentSymbol.c_str(), sym)) return 0;
            if (sym.lotSize <= 0) return 0;
            double lotAmount = (double)sym.lotSize / 10000.0;
            return ((double)sym.maxVolume / 100.0) / lotAmount;
        }

        case GET_DIGITS: { // 12 - number of digits after decimal point
            SymbolInfo sym;
            if (G.currentSymbol.empty() || !Symbols::GetSymbol(G.currentSymbol.c_str(), sym)) return 0;
            return (double)sym.digits;
        }

        case GET_STOPLEVEL: { // 14 - minimum stop distance in pips (0=none)
            return 0;  // cTrader doesn't provide a fixed stop level; server validates
        }

        case GET_BROKERZONE: // 40 - broker timezone offset (0=UTC)
            return 0;

        case GET_COMPLIANCE: // 51 - NFA flags (0=no restrictions)
            return 0;

        case GET_NTRADES: { // 52 - total number of trades (open + closed)
            CsLock lock(G.csTrades);
            return (double)G.trades.size();
        }

        case GET_POSITION: { // 53 - net open amount for given symbol (BrokerBuy2 units, signed)
            // dwParameter = (char*) symbol name per Zorro docs (e.g. "EUR/USD")
            const char* posSym = (dwParameter) ? (const char*)dwParameter : G.currentSymbol.c_str();
            if (!posSym || !*posSym) return 0;

            // Resolve Zorro asset name (e.g. "EUR/USD") to cTrader symbol (e.g. "EURUSD")
            // by looking up in symbol map. Fallback: use as-is.
            std::string resolvedSym = posSym;
            {
                SymbolInfo tmpSym;
                if (Symbols::GetSymbol(posSym, tmpSym)) {
                    resolvedSym = tmpSym.name;  // cTrader canonical name
                }
            }

            CsLock lock(G.csTrades);
            long long netAmount = 0;
            double totalEntry = 0.0;
            long long totalVol = 0;
            for (auto& kv : G.trades) {
                if (kv.second.open && !kv.second.reconciled && kv.second.symbol == resolvedSym) {
                    long long units = kv.second.volume / 100LL;
                    if (units == 0) units = 1;
                    if (kv.second.tradeSide == 2) units = -units; // SELL = negative
                    netAmount += units;
                    // Accumulate for weighted average entry price
                    totalEntry += kv.second.openPrice * (double)kv.second.volume;
                    totalVol += kv.second.volume;
                }
            }
            // Cache avg entry for GET_AVGENTRY
            G.lastPositionAvgEntry = (totalVol > 0) ? (totalEntry / (double)totalVol) : 0.0;
            Log::Info("CMD", "GET_POSITION(%s -> %s) = %lld (avgEntry=%.5f, reconciled excluded)",
                      posSym, resolvedSym.c_str(), netAmount, G.lastPositionAvgEntry);
            return (double)netAmount;
        }

        case GET_ACCOUNT: { // 54 - account ID as string in dwParameter
            if (dwParameter) {
                sprintf_s((char*)dwParameter, 256, "%lld", G.accountId);
            }
            return 1;
        }

        case GET_AVGENTRY: // 55 - average entry price from preceding GET_POSITION
            return G.lastPositionAvgEntry;

        case DO_CANCEL: { // 301 - cancel pending order
            int tradeId = (int)dwParameter;
            if (tradeId <= 0) return 0;
            return Trading::CancelOrder(tradeId) ? 1.0 : 0.0;
        }

        case SET_ORDERTEXT: // 131 - set order label/comment
            if (dwParameter) {
                G.orderLabel = (const char*)dwParameter;
            } else {
                G.orderLabel.clear();
            }
            return 1;

        case SET_LIMIT: // 135 - set limit price for next order
            if (dwParameter) {
                G.limitPrice = *(double*)dwParameter;
            } else {
                G.limitPrice = 0.0;
            }
            return 1;

        case GET_LOCK: // 46 - enter critical section (multi-thread safety)
            EnterCriticalSection(&G.csTrades);
            return 1;

        case SET_LOCK: // 171 - leave critical section
            LeaveCriticalSection(&G.csTrades);
            return 1;

        case GET_MAXTICKS: // 43 - max ticks for history
            return 8000;

        case SET_STOPLOSS: // 2001 - set pending SL price for AmendPositionSltp
            if (dwParameter) {
                G.pendingSL = *(double*)dwParameter;
            } else {
                G.pendingSL = 0.0;
            }
            return 1;

        case SET_TAKEPROFIT: // 2002 - set pending TP price for AmendPositionSltp
            if (dwParameter) {
                G.pendingTP = *(double*)dwParameter;
            } else {
                G.pendingTP = 0.0;
            }
            return 1;

        case DO_MODIFY_SLTP: { // 2003 - send AmendPositionSltpReq
            int tradeId = (int)dwParameter;
            if (tradeId <= 0) return 0;
            bool ok = Trading::AmendPositionSltp(tradeId, G.pendingSL, G.pendingTP);
            G.pendingSL = 0.0;
            G.pendingTP = 0.0;
            return ok ? 1.0 : 0.0;
        }

        default:
            Log::Diag(1, "CMD Unhandled command %d (param=%lu)", Command, dwParameter);
            return 0;
    }
}

DLLFUNC void BrokerLogout() {
    Log::Info("BROKER", "BrokerLogout");
    StopNetworkThread();
    WebSocket::Disconnect();
    G.loggedIn = false;
    G.loginCompleted = false;
}

DLLFUNC void BrokerClose() {
    Log::Info("BROKER", "BrokerClose");
    StopNetworkThread();
    WebSocket::Disconnect();
}
