#include "../include/state.h"
#include "../include/logger.h"

int(__cdecl* BrokerMessage)(const char* Text) = nullptr;
int(__cdecl* BrokerProgress)(intptr_t Progress) = nullptr;

State G;

namespace StateInit {

void Init() {
    InitializeCriticalSection(&G.csSymbols);
    InitializeCriticalSection(&G.csTrades);
    InitializeCriticalSection(&G.csLog);
    InitializeCriticalSection(&G.csHistory);
    InitializeCriticalSection(&G.csWebSocket);
    InitializeCriticalSection(&G.csTrading);
    G.historyResponseBuf = (char*)malloc(State::HIST_BUF_SIZE);
    if (G.historyResponseBuf) G.historyResponseBuf[0] = '\0';
    G.tradingResponseBuf = (char*)malloc(State::TRADE_BUF_SIZE);
    if (G.tradingResponseBuf) G.tradingResponseBuf[0] = '\0';
}

void Destroy() {
    if (G.historyResponseBuf) { free(G.historyResponseBuf); G.historyResponseBuf = nullptr; }
    if (G.tradingResponseBuf) { free(G.tradingResponseBuf); G.tradingResponseBuf = nullptr; }
    DeleteCriticalSection(&G.csSymbols);
    DeleteCriticalSection(&G.csTrades);
    DeleteCriticalSection(&G.csLog);
    DeleteCriticalSection(&G.csHistory);
    DeleteCriticalSection(&G.csWebSocket);
    DeleteCriticalSection(&G.csTrading);
}

void Reset() {
    // Symbols
    {
        CsLock lock(G.csSymbols);
        G.symbols.clear();
        G.symbolIdToName.clear();
    }
    // Trades
    {
        CsLock lock(G.csTrades);
        G.trades.clear();
        G.posIdToZorroId.clear();
        G.tradeIdAlias.clear();
        G.pendingActions.clear();
        G.nextZorroId = 2;  // MUST start at 2! BrokerBuy2 return 0=rejected, 1=unfilled per Zorro API
    }
    // Account
    G.balance = 0.0;
    G.equity = 0.0;
    G.margin = 0.0;
    G.freeMargin = 0.0;
    G.moneyDigits = 2;

    // Timing
    G.lastHeartbeatMs = 0;
    G.lastServerTimestamp = 0;
    G.quoteCount = 0;
    G.lastQuoteRecvMs = 0;
    G.msgIdCounter = 0;

    // History
    {
        CsLock lock(G.csHistory);
        G.waitingForHistory = false;
        G.historyResponseReady = false;
        G.historyResponsePt = 0;
        if (G.historyResponseBuf) G.historyResponseBuf[0] = '\0';
    }

    // Trading
    {
        CsLock lock(G.csTrading);
        G.waitingForTrading = false;
        G.tradingResponseReady = false;
        G.tradingResponsePt = 0;
        G.tradingResponseExecType = 0;
        if (G.tradingResponseBuf) G.tradingResponseBuf[0] = '\0';
    }

    // Current state
    G.currentSymbol.clear();
    G.orderType = 0;
    G.orderLabel.clear();
    G.limitPrice = 0.0;
    G.pendingSL = 0.0;
    G.pendingTP = 0.0;

    // Reconnect
    G.reconnectAttempts = 0;
    G.lastReconnectMs = 0;
    G.isReconnecting = false;

    // Env lock reset for new login (NOT reconnect)
    G.envLocked = false;

    Log::Info("STATE", "Session state reset");
}

void ResetConnection() {
    // Soft reset for reconnect: preserve trades, symbols, account
    // Only reset connection-specific state (buffers, timing, pending actions)

    // Clear pending actions (they're stale after disconnect)
    {
        CsLock lock(G.csTrades);
        G.pendingActions.clear();
    }

    // Reset subscription flags (need to resubscribe after reconnect)
    {
        CsLock lock(G.csSymbols);
        for (auto& kv : G.symbols) {
            kv.second.subscribed = false;
        }
    }

    // Timing
    G.lastHeartbeatMs = 0;
    G.quoteCount = 0;
    G.lastQuoteRecvMs = 0;

    // History buffer
    {
        CsLock lock(G.csHistory);
        G.waitingForHistory = false;
        G.historyResponseReady = false;
        G.historyResponsePt = 0;
        if (G.historyResponseBuf) G.historyResponseBuf[0] = '\0';
    }

    // Trading buffer
    {
        CsLock lock(G.csTrading);
        G.waitingForTrading = false;
        G.tradingResponseReady = false;
        G.tradingResponsePt = 0;
        G.tradingResponseExecType = 0;
        if (G.tradingResponseBuf) G.tradingResponseBuf[0] = '\0';
    }

    Log::Info("STATE", "Connection state reset (trades/symbols preserved)");
}

} // namespace StateInit
