#include "../include/state.h"
#include "../include/trading.h"
#include "../include/protocol.h"
#include "../include/websocket.h"
#include "../include/symbols.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace Trading {

// ============================================================
// Volume conversion helpers
// ============================================================

// Zorro Amount -> cTrader volume in cents
// Zorro sends Amount = Lots * LotAmount (base currency units)
// cTrader volume = base_units * 100 (cents encoding)
// Example: Amount=1000 (1000 EUR) -> vol=100,000 -> 0.01 std lot
static long long ZorroToVolume(int amount) {
    return (long long)abs(amount) * 100LL;
}

// cTrader volume in cents -> Zorro signed Amount (base currency units)
// side: 1=Buy (positive), 2=Sell (negative)
static int VolumeToZorro(long long vol, int side) {
    int units = (int)(vol / 100LL);
    if (units == 0) units = 1;  // minimum 1
    return (side == 2) ? -units : units;
}

// ============================================================
// Wait for trading response from NetworkThread
// ============================================================

static bool WaitForTradingResponse(int timeoutMs) {
    ULONGLONG start = Utils::NowMs();
    while (Utils::NowMs() - start < (ULONGLONG)timeoutMs) {
        if (G.tradingResponseReady) return true;
        Sleep(10);
        if (BrokerProgress) BrokerProgress(1);
    }
    return false;
}

// Helper: reset shared trading buffer for next event (must hold csTrading)
static void ResetTradingBuffer() {
    G.tradingResponseReady = false;
    G.tradingResponsePt = 0;
    G.tradingResponseExecType = 0;
    G.tradingResponseBuf[0] = '\0';
}

// Helper: check if positionStatus indicates CLOSED
// Server sends integer (2=CLOSED) or string "POSITION_STATUS_CLOSED"
static bool IsPositionClosed(const char* buf) {
    const char* posStatus = Protocol::ExtractString(buf, "positionStatus");
    if (!posStatus || !*posStatus) return false;
    // Check integer value "2" (POSITION_STATUS_CLOSED)
    if (strcmp(posStatus, "2") == 0) return true;
    // Check string enum
    if (strcmp(posStatus, "POSITION_STATUS_CLOSED") == 0) return true;
    return false;
}

// ============================================================
// BuyOrder - open position or place pending order
// ============================================================

int BuyOrder(const char* asset, int amount, double stopDist, double limit,
             double* pPrice, int* pFill) {

    if (!asset || amount == 0 || !G.loggedIn) {
        Log::Error("TRADE", "BuyOrder: invalid params (asset=%s amount=%d loggedIn=%d)",
                   asset ? asset : "NULL", amount, (int)G.loggedIn);
        return 0;
    }

    // Symbol lookup
    SymbolInfo sym;
    if (!Symbols::GetSymbol(asset, sym)) {
        Log::Error("TRADE", "BuyOrder: symbol %s not found", asset);
        return 0;
    }

    // Trade direction
    int tradeSide = (amount > 0) ? 1 : 2;  // 1=BUY, 2=SELL

    // Volume conversion: Amount is in base currency units (Zorro convention)
    long long vol = ZorroToVolume(amount);
    if (vol < sym.minVolume) {
        Log::Warn("TRADE", "BuyOrder: volume %lld below minimum %lld, clamping", vol, sym.minVolume);
        vol = sym.minVolume;
    }
    if (vol > sym.maxVolume) {
        Log::Warn("TRADE", "BuyOrder: volume %lld above maximum %lld, clamping", vol, sym.maxVolume);
        vol = sym.maxVolume;
    }

    // Reference price for SL calculation
    double refPrice = (tradeSide == 1) ? sym.ask : sym.bid;
    if (refPrice <= 0.0) {
        Log::Error("TRADE", "BuyOrder: no price for %s (ask=%.5f bid=%.5f)", asset, sym.ask, sym.bid);
        return 0;
    }

    // Determine order type from G.orderType (set by SET_ORDERTYPE)
    // Zorro: 0=Market(GTC), 2=Limit, 3=Stop
    // cTrader: 1=Market, 2=Limit, 3=Stop
    int cTraderOrderType = 1;  // default Market
    double orderPrice = 0.0;

    if (G.orderType == 2 && limit > 0.0) {
        cTraderOrderType = 2;
        orderPrice = limit;
    } else if (G.orderType == 3 && limit > 0.0) {
        cTraderOrderType = 3;
        orderPrice = limit;
    }

    // Allocate zorroId
    int zorroId;
    {
        CsLock lock(G.csTrades);
        zorroId = G.nextZorroId++;
    }

    // Build NewOrderReq payload
    char payload[1024];
    int off = sprintf_s(payload,
        "\"ctidTraderAccountId\":%lld,"
        "\"symbolId\":%lld,"
        "\"orderType\":%d,"
        "\"tradeSide\":%d,"
        "\"volume\":%lld,"
        "\"label\":\"z_%d\"",
        G.accountId, sym.symbolId, cTraderOrderType, tradeSide, vol, zorroId);

    // Add limit/stop price for pending orders
    if (cTraderOrderType == 2 && orderPrice > 0.0) {
        off += sprintf_s(payload + off, sizeof(payload) - off,
            ",\"limitPrice\":%.0f", orderPrice * PRICE_SCALE);
    } else if (cTraderOrderType == 3 && orderPrice > 0.0) {
        off += sprintf_s(payload + off, sizeof(payload) - off,
            ",\"stopPrice\":%.0f", orderPrice * PRICE_SCALE);
    }

    // SL for market orders: use relativeStopLoss (distance in points)
    // cTrader rejects absolute SL/TP on market orders
    if (cTraderOrderType == 1 && stopDist > 0.0) {
        long long slPoints = (long long)(stopDist * PRICE_SCALE);
        off += sprintf_s(payload + off, sizeof(payload) - off,
            ",\"relativeStopLoss\":%lld", slPoints);
    }

    // SL for limit/stop orders: use absolute stopLoss price
    if (cTraderOrderType != 1 && stopDist > 0.0) {
        double slPrice = (tradeSide == 1) ? (orderPrice - stopDist) : (orderPrice + stopDist);
        if (slPrice > 0.0) {
            off += sprintf_s(payload + off, sizeof(payload) - off,
                ",\"stopLoss\":%.0f", slPrice * PRICE_SCALE);
        }
    }

    const char* msgId = Utils::NextMsgId();

    // Register pending action
    {
        CsLock lock(G.csTrades);
        PendingAction pa;
        pa.msgId = msgId;
        pa.zorroId = zorroId;
        pa.sentTimeMs = Utils::NowMs();
        G.pendingActions[msgId] = pa;
    }

    const char* msg = Protocol::BuildMessage(msgId, PayloadType::NewOrderReq, payload);

    Log::Info("TRADE", "NewOrder: %s %s amount=%d vol=%lld type=%d zorroId=%d stopDist=%.5f limit=%.5f orderPrice=%.5f",
              (tradeSide == 1) ? "BUY" : "SELL", asset, amount, vol, cTraderOrderType, zorroId,
              stopDist, limit, orderPrice);

    // Set waiting flag and send
    {
        CsLock lock(G.csTrading);
        ResetTradingBuffer();
        G.waitingForTrading = true;
    }

    if (!WebSocket::Send(msg)) {
        Log::Error("TRADE", "NewOrder send failed");
        G.waitingForTrading = false;
        CsLock lock(G.csTrades);
        G.pendingActions.erase(msgId);
        return 0;
    }

    // Wait for response — market orders may get multiple ACCEPTED events
    // before FILLED (order accepted + SL modification etc.)
    // Loop until we get FILLED, ERROR, or timeout.
    for (int eventCount = 0; eventCount < 10; eventCount++) {
        bool gotResponse = WaitForTradingResponse(G.waitTime);

        if (!gotResponse) {
            G.waitingForTrading = false;
            Log::Error("TRADE", "NewOrder timeout (%dms) after %d events", G.waitTime, eventCount);
            CsLock lock(G.csTrades);
            G.pendingActions.erase(msgId);
            return 0;
        }

        // Process response (hold lock while reading buffer)
        CsLock tlock(G.csTrading);
        int pt = G.tradingResponsePt;

        // ErrorRes from server
        if (pt == ToInt(PayloadType::ErrorRes)) {
            G.waitingForTrading = false;
            const char* desc = Protocol::ExtractString(G.tradingResponseBuf, "description");
            Log::Error("TRADE", "NewOrder rejected by server: %s", desc);
            CsLock lock(G.csTrades);
            G.pendingActions.erase(msgId);
            return 0;
        }

        // OrderErrorEvent
        if (pt == ToInt(PayloadType::OrderErrorEvent)) {
            G.waitingForTrading = false;
            const char* desc = Protocol::ExtractString(G.tradingResponseBuf, "description");
            Log::Error("TRADE", "Order error: %s", desc);
            CsLock lock(G.csTrades);
            G.pendingActions.erase(msgId);
            return 0;
        }

        // ExecutionEvent
        if (pt == ToInt(PayloadType::ExecutionEvent)) {
            int execType = G.tradingResponseExecType;
            const char* buf = G.tradingResponseBuf;

            if (execType == 3 || execType == 11) {
                // FILLED or PARTIAL_FILL — this is what we want
                G.waitingForTrading = false;

                long long posId = Protocol::ExtractInt64(buf, "positionId");
                long long ordId = Protocol::ExtractInt64(buf, "orderId");
                // executionPrice is a JSON double (e.g. 1.18676), NOT scaled integer
                double execPrice = Protocol::ExtractDouble(buf, "executionPrice");
                long long filledVol = Protocol::ExtractInt64(buf, "filledVolume");
                if (filledVol <= 0) filledVol = vol;

                double scale = pow(10.0, (double)G.moneyDigits);
                double commission = (double)Protocol::ExtractInt64(buf, "commission") / scale;
                double swap = (double)Protocol::ExtractInt64(buf, "swap") / scale;

                // Register trade
                {
                    CsLock lock(G.csTrades);
                    TradeInfo ti;
                    ti.zorroId = zorroId;
                    ti.positionId = posId;
                    ti.orderId = ordId;
                    ti.symbol = asset;
                    ti.volume = filledVol;
                    ti.tradeSide = tradeSide;
                    ti.openPrice = execPrice;
                    ti.stopLoss = 0.0;
                    ti.takeProfit = 0.0;
                    ti.commission = commission;
                    ti.swap = swap;
                    ti.openTime = Utils::NowMs();
                    ti.open = true;
                    G.trades[zorroId] = ti;
                    G.posIdToZorroId[posId] = zorroId;
                    G.pendingActions.erase(msgId);
                }

                if (pPrice) *pPrice = execPrice;
                // pFill must ALWAYS be positive in BrokerBuy2 (filled amount).
                // Negative pFill = "partial fill, N remaining" in Zorro API.
                // Trade direction is already encoded in the Amount parameter.
                if (pFill) {
                    int filled = (int)(filledVol / 100LL);
                    if (filled == 0) filled = 1;
                    *pFill = filled;  // always positive!
                }

                Log::Info("TRADE", "Order filled: zorroId=%d posId=%lld price=%.5f vol=%lld",
                          zorroId, posId, execPrice, filledVol);
                return zorroId;
            }
            else if (execType == 2) {
                // ACCEPTED
                if (cTraderOrderType == 1) {
                    // Market order: server sends ACCEPTED before FILLED, skip and wait
                    Log::Info("TRADE", "Market order accepted (event %d), waiting for fill... zorroId=%d",
                              eventCount, zorroId);
                    ResetTradingBuffer();
                    // waitingForTrading stays true — NetworkThread keeps forwarding
                    continue;
                }

                // Limit/Stop order: ACCEPTED = pending, return -zorroId
                G.waitingForTrading = false;
                long long ordId = Protocol::ExtractInt64(buf, "orderId");

                {
                    CsLock lock(G.csTrades);
                    TradeInfo ti;
                    ti.zorroId = zorroId;
                    ti.orderId = ordId;
                    ti.symbol = asset;
                    ti.volume = vol;
                    ti.tradeSide = tradeSide;
                    ti.openPrice = orderPrice;
                    ti.stopLoss = 0.0;
                    ti.takeProfit = 0.0;
                    ti.open = true;
                    G.trades[zorroId] = ti;
                    G.pendingActions.erase(msgId);
                }

                Log::Info("TRADE", "Pending order accepted: zorroId=%d orderId=%lld price=%.5f",
                          zorroId, ordId, orderPrice);
                return -zorroId;  // negative = pending
            }
            else if (execType == 7) {
                // REJECTED
                G.waitingForTrading = false;
                const char* reason = Protocol::ExtractString(buf, "reasonCode");
                Log::Error("TRADE", "Order rejected: execType=%d reason=%s", execType, reason);
                CsLock lock(G.csTrades);
                G.pendingActions.erase(msgId);
                return 0;
            }
            else {
                // Other execution types (4=ORDER_REPLACED, 5=ORDER_CANCELLED, etc.)
                // For market orders: skip and keep waiting for FILLED
                if (cTraderOrderType == 1) {
                    Log::Info("TRADE", "Market order event execType=%d (event %d), skipping...", execType, eventCount);
                    ResetTradingBuffer();
                    continue;
                }
                G.waitingForTrading = false;
                Log::Warn("TRADE", "Unexpected execType=%d for NewOrder", execType);
                CsLock lock(G.csTrades);
                G.pendingActions.erase(msgId);
                return 0;
            }
        }

        // Unknown payloadType — for market orders, skip and keep waiting
        if (cTraderOrderType == 1) {
            Log::Info("TRADE", "Market order: skipping unexpected pt=%d (event %d)", pt, eventCount);
            ResetTradingBuffer();
            continue;
        }

        G.waitingForTrading = false;
        Log::Warn("TRADE", "Unexpected response pt=%d for NewOrder", pt);
        {
            CsLock lock(G.csTrades);
            G.pendingActions.erase(msgId);
        }
        return 0;
    }

    // Exhausted event loop iterations
    G.waitingForTrading = false;
    Log::Error("TRADE", "Market order: too many events without FILLED, giving up");
    {
        CsLock lock(G.csTrades);
        G.pendingActions.erase(msgId);
    }
    return 0;
}

// ============================================================
// SellOrder - close or reduce position
// ============================================================

int SellOrder(int tradeId, int amount, double limit,
              double* pClose, double* pCost, double* pProfit, int* pFill) {

    if (!G.loggedIn) return 0;

    // Zorro may pass negative tradeId (from pending order return value)
    int lookupId = abs(tradeId);

    // Lookup trade — exact match, then alias cache, then symbol fallback
    TradeInfo ti;
    {
        CsLock lock(G.csTrades);
        auto it = G.trades.find(lookupId);

        // Check alias cache
        if (it == G.trades.end()) {
            auto aliasIt = G.tradeIdAlias.find(lookupId);
            if (aliasIt != G.tradeIdAlias.end()) {
                it = G.trades.find(aliasIt->second);
                if (it != G.trades.end()) {
                    lookupId = aliasIt->second;
                    Log::Info("TRADE", "SellOrder: tradeId=%d resolved via alias to zorroId=%d",
                              tradeId, lookupId);
                }
            }
        }

        // Fallback: search by symbol (only if single open position)
        if (it == G.trades.end() && !G.currentSymbol.empty()) {
            int matchId = 0;
            int matchCount = 0;
            for (auto& kv : G.trades) {
                if (kv.second.open && kv.second.positionId > 0 &&
                    kv.second.symbol == G.currentSymbol) {
                    matchId = kv.first;
                    matchCount++;
                }
            }
            if (matchCount == 1) {
                it = G.trades.find(matchId);
                lookupId = matchId;
                G.tradeIdAlias[abs(tradeId)] = matchId;
                Log::Info("TRADE", "SellOrder: tradeId=%d not found, fallback to zorroId=%d (%s)",
                          tradeId, lookupId, G.currentSymbol.c_str());
            } else if (matchCount > 1) {
                Log::Warn("TRADE", "SellOrder: tradeId=%d not found, %d open trades for %s — ambiguous",
                           tradeId, matchCount, G.currentSymbol.c_str());
            }
        }

        if (it == G.trades.end()) {
            Log::Error("TRADE", "SellOrder: tradeId=%d (lookup=%d) not found", tradeId, lookupId);
            return 0;
        }
        ti = it->second;
    }

    if (!ti.open) {
        Log::Warn("TRADE", "SellOrder: tradeId=%d already closed", tradeId);
        return 0;
    }

    if (ti.positionId <= 0) {
        // This is a pending order, not a filled position - cancel it
        Log::Info("TRADE", "SellOrder: tradeId=%d is pending order, cancelling", tradeId);
        return CancelOrder(lookupId) ? 0 : lookupId;
    }

    // Get symbol info for volume conversion
    SymbolInfo sym;
    if (!Symbols::GetSymbol(ti.symbol.c_str(), sym)) {
        Log::Error("TRADE", "SellOrder: symbol %s not found", ti.symbol.c_str());
        return lookupId;
    }

    // Volume to close
    long long closeVol = ti.volume;  // default: close all
    if (amount != 0) {
        closeVol = ZorroToVolume(amount);
        if (closeVol > ti.volume) closeVol = ti.volume;
    }

    // Build ClosePositionReq payload
    char payload[512];
    sprintf_s(payload,
        "\"ctidTraderAccountId\":%lld,"
        "\"positionId\":%lld,"
        "\"volume\":%lld",
        G.accountId, ti.positionId, closeVol);

    const char* msgId = Utils::NextMsgId();
    const char* msg = Protocol::BuildMessage(msgId, PayloadType::ClosePositionReq, payload);

    Log::Info("TRADE", "ClosePosition: tradeId=%d posId=%lld vol=%lld/%lld",
              lookupId, ti.positionId, closeVol, ti.volume);

    // Set waiting flag and send
    {
        CsLock lock(G.csTrading);
        ResetTradingBuffer();
        G.waitingForTrading = true;
    }

    if (!WebSocket::Send(msg)) {
        Log::Error("TRADE", "ClosePosition send failed");
        G.waitingForTrading = false;
        return lookupId;
    }

    // Wait for response — ClosePosition also gets ACCEPTED before FILLED
    for (int eventCount = 0; eventCount < 10; eventCount++) {
        bool gotResponse = WaitForTradingResponse(G.waitTime);

        if (!gotResponse) {
            G.waitingForTrading = false;
            Log::Error("TRADE", "ClosePosition timeout (%dms) after %d events", G.waitTime, eventCount);
            return lookupId;
        }

        CsLock tlock(G.csTrading);
        int pt = G.tradingResponsePt;

        if (pt == ToInt(PayloadType::ErrorRes)) {
            G.waitingForTrading = false;
            const char* desc = Protocol::ExtractString(G.tradingResponseBuf, "description");
            Log::Error("TRADE", "ClosePosition error: %s", desc);
            return lookupId;
        }

        if (pt == ToInt(PayloadType::OrderErrorEvent)) {
            G.waitingForTrading = false;
            const char* desc = Protocol::ExtractString(G.tradingResponseBuf, "description");
            Log::Error("TRADE", "ClosePosition order error: %s", desc);
            return lookupId;
        }

        if (pt == ToInt(PayloadType::ExecutionEvent)) {
            int execType = G.tradingResponseExecType;
            const char* buf = G.tradingResponseBuf;

            if (execType == 3 || execType == 11) {
                // FILLED or PARTIAL_FILL
                G.waitingForTrading = false;

                // executionPrice is a JSON double
                double closePrice = Protocol::ExtractDouble(buf, "executionPrice");
                long long filledVol = Protocol::ExtractInt64(buf, "filledVolume");
                if (filledVol <= 0) filledVol = closeVol;

                double scale = pow(10.0, (double)G.moneyDigits);
                double commission = (double)Protocol::ExtractInt64(buf, "commission") / scale;
                double swap = (double)Protocol::ExtractInt64(buf, "swap") / scale;

                bool fullyClosed = IsPositionClosed(buf);
                if (!fullyClosed && filledVol >= ti.volume) {
                    fullyClosed = true;
                }

                // Calculate profit
                double lotAmount = (double)sym.lotSize / 100.0;
                double lots = (double)filledVol / (double)sym.lotSize;
                double priceDiff = (ti.tradeSide == 1)
                    ? (closePrice - ti.openPrice)
                    : (ti.openPrice - closePrice);
                double profit = priceDiff * lots * lotAmount;

                if (pClose) *pClose = closePrice;
                if (pCost) *pCost = ti.swap + ti.commission + commission;
                if (pProfit) *pProfit = profit;
                if (pFill) *pFill = VolumeToZorro(filledVol, ti.tradeSide);

                // Update trade state
                {
                    CsLock lock(G.csTrades);
                    auto it = G.trades.find(lookupId);
                    if (it != G.trades.end()) {
                        if (fullyClosed) {
                            it->second.open = false;
                            it->second.profit = profit;
                            it->second.commission += commission;
                            it->second.swap += swap;
                        } else {
                            it->second.volume -= filledVol;
                            it->second.commission += commission;
                            it->second.swap += swap;
                        }
                    }
                }

                Log::Info("TRADE", "Position %s: tradeId=%d price=%.5f profit=%.2f %s",
                          fullyClosed ? "closed" : "partially closed",
                          lookupId, closePrice, profit,
                          fullyClosed ? "" : "(still open)");

                return fullyClosed ? 0 : lookupId;
            }
            else if (execType == 2 || execType == 4 || execType == 5) {
                // ACCEPTED / ORDER_REPLACED / ORDER_CANCELLED
                // Close operation gets ACCEPTED before FILLED, skip and wait
                Log::Info("TRADE", "ClosePosition event execType=%d (event %d), waiting for fill...", execType, eventCount);
                ResetTradingBuffer();
                continue;
            }
            else {
                // Other execution types — skip and keep waiting
                Log::Info("TRADE", "ClosePosition event execType=%d (event %d), skipping...", execType, eventCount);
                ResetTradingBuffer();
                continue;
            }
        }

        // Unknown payloadType — skip and keep waiting
        Log::Info("TRADE", "ClosePosition: skipping unexpected pt=%d (event %d)", pt, eventCount);
        ResetTradingBuffer();
        continue;
    }

    G.waitingForTrading = false;
    Log::Error("TRADE", "ClosePosition: too many events without FILLED");
    return lookupId;
}

// ============================================================
// GetTradeStatus - no network call, uses local state
// ============================================================

int GetTradeStatus(int tradeId, double* pOpen, double* pClose,
                   double* pCost, double* pProfit) {
    int lookupId = abs(tradeId);
    TradeInfo ti;
    {
        CsLock lock(G.csTrades);
        auto it = G.trades.find(lookupId);

        // Check alias cache first
        if (it == G.trades.end()) {
            auto aliasIt = G.tradeIdAlias.find(lookupId);
            if (aliasIt != G.tradeIdAlias.end()) {
                it = G.trades.find(aliasIt->second);
                if (it != G.trades.end()) lookupId = aliasIt->second;
            }
        }

        // Fallback: search by current symbol (only if single open position for that symbol)
        if (it == G.trades.end() && !G.currentSymbol.empty()) {
            int matchId = 0;
            int matchCount = 0;
            for (auto& kv : G.trades) {
                if (kv.second.open && kv.second.positionId > 0 &&
                    kv.second.symbol == G.currentSymbol) {
                    matchId = kv.first;
                    matchCount++;
                }
            }
            // Only use fallback if exactly ONE open position for this symbol
            // to avoid ambiguity between LONG and SHORT
            if (matchCount == 1) {
                it = G.trades.find(matchId);
                lookupId = matchId;
                G.tradeIdAlias[abs(tradeId)] = matchId;  // cache the alias
                Log::Info("TRADE", "GetTradeStatus: tradeId=%d not found, fallback to zorroId=%d (%s)",
                          tradeId, lookupId, G.currentSymbol.c_str());
            }
        }

        if (it == G.trades.end()) {
            Log::Warn("TRADE", "GetTradeStatus: tradeId=%d not found -> return -1 (closed)", tradeId);
            return -1;  // trade not found = closed (Zorro will book P&L)
        }
        ti = it->second;
    }

    if (!ti.open) {
        Log::Info("TRADE", "GetTradeStatus: tradeId=%d is closed -> return -1", tradeId);
        return -1;  // trade closed = Zorro books P&L
    }

    // Get current price for unrealized PnL
    SymbolInfo sym;
    double closePrice = 0.0;
    if (Symbols::GetSymbol(ti.symbol.c_str(), sym)) {
        closePrice = (ti.tradeSide == 1) ? sym.bid : sym.ask;

        double lotAmount = (double)sym.lotSize / 100.0;
        double lots = (double)ti.volume / (double)sym.lotSize;
        double priceDiff = (ti.tradeSide == 1)
            ? (closePrice - ti.openPrice)
            : (ti.openPrice - closePrice);
        double profit = priceDiff * lots * lotAmount;

        if (pOpen) *pOpen = ti.openPrice;
        if (pClose) *pClose = closePrice;
        if (pCost) *pCost = ti.swap + ti.commission;
        if (pProfit) *pProfit = profit;
    } else {
        if (pOpen) *pOpen = ti.openPrice;
        if (pClose) *pClose = 0.0;
        if (pCost) *pCost = ti.swap + ti.commission;
        if (pProfit) *pProfit = 0.0;
    }

    // BrokerTrade return value = nLotAmount, ALWAYS POSITIVE (Zorro convention)
    // Direction is not encoded in the return value.
    int lotAmount = (int)(ti.volume / 100LL);
    if (lotAmount == 0) lotAmount = 1;
    return lotAmount;
}

// ============================================================
// HandleExecutionEvent - called from NetworkThread
// ============================================================

void HandleExecutionEvent(const char* buffer, int bufLen) {
    int execType = Protocol::ExtractInt(buffer, "executionType");

    if (G.waitingForTrading) {
        // Wait for main thread to consume previous event before overwriting.
        // Without this, fast consecutive events (ACCEPTED → FILLED → SL_ACCEPTED)
        // can cause FILLED to be overwritten by SL_ACCEPTED before main thread reads it.
        for (int i = 0; i < 500 && G.tradingResponseReady; i++) {
            Sleep(1);
        }

        // Forward to waiting BuyOrder/SellOrder via shared buffer
        CsLock lock(G.csTrading);
        int copyLen = (bufLen < State::TRADE_BUF_SIZE - 1) ? bufLen : State::TRADE_BUF_SIZE - 1;
        memcpy(G.tradingResponseBuf, buffer, copyLen);
        G.tradingResponseBuf[copyLen] = '\0';
        G.tradingResponsePt = ToInt(PayloadType::ExecutionEvent);
        G.tradingResponseExecType = execType;
        G.tradingResponseReady = true;
        return;
    }

    // Async event: SL/TP trigger, swap, etc.
    long long posId = Protocol::ExtractInt64(buffer, "positionId");
    int posStatusInt = Protocol::ExtractInt(buffer, "positionStatus");

    Log::Info("TRADE", "Async ExecutionEvent: execType=%d posId=%lld status=%d",
              execType, posId, posStatusInt);

    if (posId > 0) {
        CsLock lock(G.csTrades);
        auto posIt = G.posIdToZorroId.find(posId);
        if (posIt != G.posIdToZorroId.end()) {
            int zid = posIt->second;
            auto tradeIt = G.trades.find(zid);
            if (tradeIt != G.trades.end()) {
                TradeInfo& ti = tradeIt->second;

                // Update swap from execution event
                double scale = pow(10.0, (double)G.moneyDigits);
                if (execType == 9) {  // Swap
                    long long swapRaw = Protocol::ExtractInt64(buffer, "swap");
                    ti.swap = (double)swapRaw / scale;
                }

                // Position closed (SL/TP hit, liquidation, etc.)
                // positionStatus: 1=OPEN, 2=CLOSED
                if (posStatusInt == 2) {
                    ti.open = false;
                    // executionPrice is a JSON double
                    double closePrice = Protocol::ExtractDouble(buffer, "executionPrice");
                    Log::Info("TRADE", "Position auto-closed: zorroId=%d posId=%lld at %.5f",
                              zid, posId, closePrice);
                }
            }
        }
    }
}

// ============================================================
// HandleOrderErrorEvent - called from NetworkThread
// ============================================================

void HandleOrderErrorEvent(const char* buffer, int bufLen) {
    if (G.waitingForTrading) {
        // Wait for main thread to consume previous event
        for (int i = 0; i < 500 && G.tradingResponseReady; i++) {
            Sleep(1);
        }
        // Forward to waiting BuyOrder/SellOrder
        CsLock lock(G.csTrading);
        int copyLen = (bufLen < State::TRADE_BUF_SIZE - 1) ? bufLen : State::TRADE_BUF_SIZE - 1;
        memcpy(G.tradingResponseBuf, buffer, copyLen);
        G.tradingResponseBuf[copyLen] = '\0';
        G.tradingResponsePt = ToInt(PayloadType::OrderErrorEvent);
        G.tradingResponseExecType = 0;
        G.tradingResponseReady = true;
        return;
    }

    // Async error
    const char* desc = Protocol::ExtractString(buffer, "description");
    Log::Error("TRADE", "Async OrderError: %s", desc);
}

// ============================================================
// RequestReconcile / HandleReconcileRes
// ============================================================

bool RequestReconcile() {
    char payload[128];
    sprintf_s(payload, "\"ctidTraderAccountId\":%lld", G.accountId);

    const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                             PayloadType::ReconcileReq, payload);

    Log::Info("TRADE", "Requesting position reconciliation");

    if (!WebSocket::Send(msg)) {
        Log::Error("TRADE", "ReconcileReq send failed");
        return false;
    }

    // Wait for response synchronously (called during login before NetworkThread)
    char response[65536] = {0};
    ULONGLONG start = Utils::NowMs();
    while (Utils::NowMs() - start < (ULONGLONG)G.waitTime) {
        int n = WebSocket::Receive(response, sizeof(response));
        if (n > 0) {
            int pt = Protocol::ExtractPayloadType(response);
            if (pt == ToInt(PayloadType::ReconcileRes)) {
                HandleReconcileRes(response);
                return true;
            }
            if (pt == ToInt(PayloadType::ErrorRes)) {
                Log::Error("TRADE", "ReconcileReq error: %s",
                          Protocol::ExtractString(response, "description"));
                return false;
            }
            // Other messages during reconcile: SpotEvent, MarginChanged, etc. - ignore
        }
        Sleep(10);
    }

    Log::Warn("TRADE", "ReconcileReq timeout");
    return false;
}

void HandleReconcileRes(const char* buffer) {
    // Parse position array from reconcile response
    const char* arr = Protocol::ExtractArray(buffer, "position");
    if (!arr || *arr == '\0' || (*arr == '[' && *(arr + 1) == ']')) {
        Log::Info("TRADE", "Reconcile: no open positions");
        return;
    }

    int count = Protocol::CountArrayElements(arr);
    Log::Info("TRADE", "Reconcile: %d open positions", count);

    CsLock lock(G.csTrades);

    for (int i = 0; i < count; i++) {
        const char* elem = Protocol::GetArrayElement(arr, i);
        if (!elem || !*elem) continue;

        long long posId = Protocol::ExtractInt64(elem, "positionId");
        long long symId = Protocol::ExtractInt64(elem, "symbolId");
        int side = Protocol::ExtractInt(elem, "tradeSide");
        long long vol = Protocol::ExtractInt64(elem, "volume");
        // price is a JSON double (actual price, not scaled)
        double price = Protocol::ExtractDouble(elem, "price");

        double scale = pow(10.0, (double)G.moneyDigits);
        double commission = (double)Protocol::ExtractInt64(elem, "commission") / scale;
        double swap = (double)Protocol::ExtractInt64(elem, "swap") / scale;

        // Recover zorroId from label "z_N" (set by BuyOrder)
        // This allows Zorro to find the same trade after plugin restart
        int zid = 0;
        const char* label = Protocol::ExtractString(elem, "label");
        if (label && label[0] == 'z' && label[1] == '_') {
            zid = atoi(label + 2);
        }
        if (zid <= 0) {
            // No label or invalid — check posIdToZorroId, then allocate new
            auto posIt = G.posIdToZorroId.find(posId);
            if (posIt != G.posIdToZorroId.end()) {
                zid = posIt->second;
            } else {
                zid = G.nextZorroId++;
            }
        }

        // Get symbol name
        const char* symName = Symbols::GetNameById(symId);
        std::string symStr = symName ? symName : "";

        // Check if this position was opened by Zorro (has "z_N" label)
        bool hasZorroLabel = (label && label[0] == 'z' && label[1] == '_' && zid > 0);

        TradeInfo ti;
        ti.zorroId = zid;
        ti.positionId = posId;
        ti.symbol = symStr;
        ti.volume = vol;
        ti.tradeSide = side;
        ti.openPrice = price;
        ti.commission = commission;
        ti.swap = swap;
        ti.open = true;
        ti.reconciled = !hasZorroLabel;  // NOT reconciled if Zorro opened it (has z_N label)

        // SL/TP if present (JSON doubles)
        if (Protocol::HasField(elem, "stopLoss")) {
            ti.stopLoss = Protocol::ExtractDouble(elem, "stopLoss");
        }
        if (Protocol::HasField(elem, "takeProfit")) {
            ti.takeProfit = Protocol::ExtractDouble(elem, "takeProfit");
        }

        G.trades[zid] = ti;
        G.posIdToZorroId[posId] = zid;

        Log::Info("TRADE", "Reconciled: zorroId=%d posId=%lld %s %s vol=%lld price=%.5f label=%s reconciled=%d",
                  zid, posId, (side == 1) ? "BUY" : "SELL", symStr.c_str(), vol, price,
                  label ? label : "(none)", (int)ti.reconciled);

        // Ensure nextZorroId is above all recovered IDs
        if (zid >= G.nextZorroId) G.nextZorroId = zid + 1;
    }

    // Also parse pending orders from "order" array
    const char* orderArr = Protocol::ExtractArray(buffer, "order");
    if (orderArr && *orderArr != '\0' && !(*orderArr == '[' && *(orderArr + 1) == ']')) {
        int orderCount = Protocol::CountArrayElements(orderArr);
        Log::Info("TRADE", "Reconcile: %d pending orders", orderCount);

        for (int i = 0; i < orderCount; i++) {
            const char* elem = Protocol::GetArrayElement(orderArr, i);
            if (!elem || !*elem) continue;

            long long ordId = Protocol::ExtractInt64(elem, "orderId");
            long long symId = Protocol::ExtractInt64(elem, "symbolId");
            int side = Protocol::ExtractInt(elem, "tradeSide");
            long long vol = Protocol::ExtractInt64(elem, "volume");
            int ordType = Protocol::ExtractInt(elem, "orderType");

            // Prices are JSON doubles
            double limitPrice = 0.0;
            double stopPrice = 0.0;
            if (Protocol::HasField(elem, "limitPrice"))
                limitPrice = Protocol::ExtractDouble(elem, "limitPrice");
            if (Protocol::HasField(elem, "stopPrice"))
                stopPrice = Protocol::ExtractDouble(elem, "stopPrice");

            // Recover zorroId from label "z_N"
            int zid = 0;
            const char* ordLabel = Protocol::ExtractString(elem, "label");
            if (ordLabel && ordLabel[0] == 'z' && ordLabel[1] == '_') {
                zid = atoi(ordLabel + 2);
            }
            if (zid <= 0) {
                zid = G.nextZorroId++;
            }

            const char* symName = Symbols::GetNameById(symId);
            bool hasZorroLabel = (ordLabel && ordLabel[0] == 'z' && ordLabel[1] == '_' && zid > 0);

            TradeInfo ti;
            ti.zorroId = zid;
            ti.orderId = ordId;
            ti.symbol = symName ? symName : "";
            ti.volume = vol;
            ti.tradeSide = side;
            ti.openPrice = (ordType == 2) ? limitPrice : stopPrice;
            ti.open = true;
            ti.reconciled = !hasZorroLabel;

            G.trades[zid] = ti;

            // Ensure nextZorroId is above all recovered IDs
            if (zid >= G.nextZorroId) G.nextZorroId = zid + 1;

            Log::Info("TRADE", "Reconciled pending: zorroId=%d orderId=%lld %s vol=%lld type=%d label=%s reconciled=%d",
                      zid, ordId, (side == 1) ? "BUY" : "SELL", vol, ordType,
                      ordLabel ? ordLabel : "(none)", (int)ti.reconciled);
        }
    }
}

// ============================================================
// CancelOrder - cancel a pending order
// ============================================================

bool CancelOrder(int tradeId) {
    TradeInfo ti;
    {
        CsLock lock(G.csTrades);
        auto it = G.trades.find(tradeId);
        if (it == G.trades.end()) return false;
        ti = it->second;
    }

    if (ti.orderId <= 0) {
        Log::Error("TRADE", "CancelOrder: tradeId=%d has no orderId", tradeId);
        return false;
    }

    char payload[256];
    sprintf_s(payload,
        "\"ctidTraderAccountId\":%lld,"
        "\"orderId\":%lld",
        G.accountId, ti.orderId);

    const char* msgId = Utils::NextMsgId();
    const char* msg = Protocol::BuildMessage(msgId, PayloadType::CancelOrderReq, payload);

    Log::Info("TRADE", "CancelOrder: tradeId=%d orderId=%lld", tradeId, ti.orderId);

    // Set waiting flag and send
    {
        CsLock lock(G.csTrading);
        ResetTradingBuffer();
        G.waitingForTrading = true;
    }

    if (!WebSocket::Send(msg)) {
        Log::Error("TRADE", "CancelOrder send failed");
        G.waitingForTrading = false;
        return false;
    }

    bool gotResponse = WaitForTradingResponse(G.waitTime);
    G.waitingForTrading = false;

    if (!gotResponse) {
        Log::Error("TRADE", "CancelOrder timeout");
        return false;
    }

    CsLock tlock(G.csTrading);
    int pt = G.tradingResponsePt;

    if (pt == ToInt(PayloadType::ExecutionEvent)) {
        int execType = G.tradingResponseExecType;
        if (execType == 5) {  // OrderCancelled
            CsLock lock(G.csTrades);
            auto it = G.trades.find(tradeId);
            if (it != G.trades.end()) {
                it->second.open = false;
            }
            Log::Info("TRADE", "Order cancelled: tradeId=%d", tradeId);
            return true;
        }
    }

    if (pt == ToInt(PayloadType::ErrorRes) || pt == ToInt(PayloadType::OrderErrorEvent)) {
        const char* desc = Protocol::ExtractString(G.tradingResponseBuf, "description");
        Log::Error("TRADE", "CancelOrder error: %s", desc);
    }

    return false;
}

} // namespace Trading
