#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

#define DLLFUNC extern "C" __declspec(dllexport)

// Callbacks from Zorro
extern int(__cdecl* BrokerMessage)(const char* Text);
extern int(__cdecl* BrokerProgress)(intptr_t Progress);

// Environment
enum class Env { Demo, Live };

// RAII lock guard for CRITICAL_SECTION
class CsLock {
    CRITICAL_SECTION& cs_;
public:
    CsLock(CRITICAL_SECTION& cs) : cs_(cs) { EnterCriticalSection(&cs_); }
    ~CsLock() { LeaveCriticalSection(&cs_); }
    CsLock(const CsLock&) = delete;
    CsLock& operator=(const CsLock&) = delete;
};

// Symbol info from SymbolsListRes + SymbolByIdRes
struct SymbolInfo {
    long long symbolId = 0;
    std::string name;
    int digits = 5;
    int pipPosition = 4;
    long long lotSize = 100000;   // in cents (100000 = 1 lot)
    long long minVolume = 1000;   // in cents
    long long maxVolume = 10000000;
    long long stepVolume = 1000;
    double swapLong = 0.0;
    double swapShort = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double high = 0.0;
    double low = 0.0;
    long long baseAssetId = 0;
    long long quoteAssetId = 0;
    bool subscribed = false;
    long long lastQuoteTime = 0;
};

// Trade/position info
struct TradeInfo {
    int zorroId = 0;
    long long positionId = 0;
    long long orderId = 0;
    std::string symbol;
    long long volume = 0;         // in cents
    int tradeSide = 0;            // 1=Buy, 2=Sell
    double openPrice = 0.0;
    double stopLoss = 0.0;
    double takeProfit = 0.0;
    double profit = 0.0;
    double commission = 0.0;
    double swap = 0.0;
    long long openTime = 0;
    bool open = true;
    bool reconciled = false;      // true = loaded from server at login, NOT opened by Zorro this session
};

// Pending action tracking
struct PendingAction {
    std::string msgId;
    int zorroId = 0;
    long long positionId = 0;
    long long orderId = 0;
    ULONGLONG sentTimeMs = 0;
};

// CSV credentials
struct CsvCreds {
    std::string clientId;
    std::string clientSecret;
    std::string type;             // "demo" or "live"
    std::string accountId;
    std::string accessToken;
    std::string server;
    bool hasExplicitEnv = false;
    Env explicitEnv = Env::Demo;
};

// Central state - single source of truth
struct State {
    // Auth tokens
    char accessToken[2048] = {};
    char refreshToken[2048] = {};
    char clientId[256] = {};
    char clientSecret[256] = {};
    long long accountId = 0;

    // Environment - set ONCE at login, NEVER overwritten
    Env env = Env::Demo;
    bool envLocked = false;
    std::string hostOverride;
    std::string redirectUri;       // from CSV, e.g. "http://127.0.0.1:53123/callback"

    // Login state
    bool loggedIn = false;
    bool loginCompleted = false;

    // WebSocket handles
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hWebSocket = NULL;
    bool wsConnected = false;

    // Network thread
    HANDLE hThread = NULL;
    volatile bool running = false;

    // Critical sections
    CRITICAL_SECTION csSymbols;
    CRITICAL_SECTION csTrades;
    CRITICAL_SECTION csLog;
    CRITICAL_SECTION csWebSocket;

    // Symbols - SINGLE source!
    std::map<std::string, SymbolInfo> symbols;       // name -> info
    std::map<long long, std::string> symbolIdToName;  // reverse lookup

    // Trades
    std::map<int, TradeInfo> trades;                  // zorroId -> info
    std::map<long long, int> posIdToZorroId;          // positionId -> zorroId
    std::map<int, int> tradeIdAlias;                  // zorroTradeId -> actual zorroId (fallback cache)
    std::map<std::string, PendingAction> pendingActions; // msgId -> action
    int nextZorroId = 2;  // MUST start at 2! BrokerBuy2 return 0=rejected, 1=unfilled

    // Account
    double balance = 0.0;
    double equity = 0.0;
    double margin = 0.0;
    double freeMargin = 0.0;
    int moneyDigits = 2;
    long long leverageInCents = 0;  // from TraderRes: 50000 = 500:1

    // Diagnostics
    int diagLevel = 0;
    char dllDir[MAX_PATH] = {};
    char logPath[MAX_PATH] = {};

    // Timing
    ULONGLONG lastHeartbeatMs = 0;
    long long lastServerTimestamp = 0;

    // Message ID counter
    int msgIdCounter = 0;

    // Current state for BrokerCommand
    std::string currentSymbol;
    int orderType = 0;
    int waitTime = 30000;  // default 30s timeout
    double lastPositionAvgEntry = 0.0;  // cached for GET_AVGENTRY (set by GET_POSITION)

    // Reconnect state
    int reconnectAttempts = 0;
    ULONGLONG lastReconnectMs = 0;
    volatile bool isReconnecting = false;

    // BrokerCommand state (F2)
    std::string orderLabel;
    double limitPrice = 0.0;

    // AmendPositionSltp state (M5)
    double pendingSL = 0.0;
    double pendingTP = 0.0;

    // Subscription tracking
    int quoteCount = 0;
    ULONGLONG subscriptionStartMs = 0;
    volatile ULONGLONG lastQuoteRecvMs = 0;  // GetTickCount64() of last SpotEvent

    // History response mechanism (NetworkThread forwards to BrokerHistory2)
    CRITICAL_SECTION csHistory;
    volatile bool waitingForHistory = false;
    volatile bool historyResponseReady = false;
    volatile int historyResponsePt = 0;
    static constexpr int HIST_BUF_SIZE = 2 * 1024 * 1024;  // 2MB for M1 data
    char* historyResponseBuf = nullptr;  // heap-allocated in Init()

    // Trading response mechanism (NetworkThread forwards to BrokerBuy2/Sell2)
    CRITICAL_SECTION csTrading;
    volatile bool waitingForTrading = false;
    volatile bool tradingResponseReady = false;
    volatile int tradingResponsePt = 0;
    volatile int tradingResponseExecType = 0;
    static constexpr int TRADE_BUF_SIZE = 64 * 1024;  // 64KB for execution events
    char* tradingResponseBuf = nullptr;  // heap-allocated in Init()
};

extern State G;

// Constants
constexpr int PLUGIN_TYPE = 2;
constexpr const char* PLUGIN_NAME = "cTrader";
constexpr const char* PLUGIN_VERSION = "4.2.0";
constexpr const char* CTRADER_HOST_DEMO = "demo.ctraderapi.com";
constexpr const char* CTRADER_HOST_LIVE = "live.ctraderapi.com";
constexpr int CTRADER_WS_PORT = 5036;
constexpr ULONGLONG PING_INTERVAL_MS = 10000;  // API: 10s heartbeat, 30s disconnect
constexpr double PRICE_SCALE = 100000.0;

typedef double DATE;

// T6 tick/bar struct (Zorro official format from include/trading.h)
// CRITICAL: fields are FLOAT not double, and order is High,Low,Open,Close!
#pragma pack(push, 4)
typedef struct T6 {
    DATE  time;           // GMT timestamp of fClose (8 bytes, double)
    float fHigh, fLow;    // (f1,f2) - 4+4 bytes
    float fOpen, fClose;   // (f3,f4) - 4+4 bytes
    float fVal, fVol;      // additional data: spread and volume (f5,f6) - 4+4 bytes
} T6;  // = 32 bytes total
#pragma pack(pop)

namespace StateInit {
    void Init();
    void Destroy();
    void Reset();              // Full session reset (clear symbols, trades, account, timing)
    void ResetConnection();    // Soft reset for reconnect (preserve trades, symbols, account)
}
