#include "../include/trading.h"
#include "../include/globals.h"
#include "../include/utils.h"
#include "../include/network.h"
#include "../include/symbols.h"

namespace Trading {

int PlaceOrder(const char* symbol, int amount, double stopDist, double limit, double* pPrice, int* pFill) {
    if (G.CTraderAccountId == 0 || !symbol) return 0;

    SymbolInfo* info = Symbols::GetSymbolByIdOrName(symbol);
    if (!info) {
        char msg[128];
        sprintf_s(msg, "Symbol not found: %s (normalized: %s)",
                  symbol, Symbols::NormalizeSymbol(symbol).c_str());
        Utils::ShowMsg(msg);
        return 0;
    }

    long long symbolId = info->id;
    int tradeSide = (amount > 0) ? 1 : 2; // 1=BUY, 2=SELL
    long long volumeInCents = (long long)abs(amount) * 100000;

    std::string clientMsgId = Utils::GetMsgId();
    int zorroTradeId = G.nextTradeId++;

    // Store pending trade
    EnterCriticalSection(&G.cs_trades);
    G.pendingTrades[clientMsgId] = zorroTradeId;
    G.pendingTradeInfo[zorroTradeId] = std::make_pair(std::string(symbol),
                                                       (amount > 0 ? 1 : -1));
    LeaveCriticalSection(&G.cs_trades);

    // Build request with Stop Loss and Take Profit support
    char request[1024];
    std::string stopLossStr = "";
    std::string takeProfitStr = "";

    if (stopDist > 0) {
        double currentPrice = (double)info->bid;
        if (tradeSide == 2) currentPrice = (double)info->ask;

        double stopPrice = (tradeSide == 1) ?
            currentPrice - stopDist :
            currentPrice + stopDist;

        char stopBuf[64];
        sprintf_s(stopBuf, ",\"stopLoss\":%.5f", stopPrice);
        stopLossStr = stopBuf;
    }

    if (limit > 0) {
        char takeBuf[64];
        sprintf_s(takeBuf, ",\"takeProfit\":%.5f", limit);
        takeProfitStr = takeBuf;
    }

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
        "{\"ctidTraderAccountId\":%lld,"
        "\"symbolId\":%lld,\"orderType\":1,\"tradeSide\":%d,\"volume\":%lld%s%s}}",
        clientMsgId.c_str(), (int)PayloadType::PROTO_OA_NEW_ORDER_REQ, G.CTraderAccountId, symbolId,
        tradeSide, volumeInCents, stopLossStr.c_str(), takeProfitStr.c_str());

    if (!Network::Send(request)) {
        EnterCriticalSection(&G.cs_trades);
        G.pendingTrades.erase(clientMsgId);
        G.pendingTradeInfo.erase(zorroTradeId);
        LeaveCriticalSection(&G.cs_trades);
        Utils::ShowMsg("Order send failed");
        return 0;
    }

    if (pFill) *pFill = amount;
    if (pPrice) *pPrice = 0;

    return zorroTradeId;
}

int ClosePosition(int tradeId, int amount) {
    EnterCriticalSection(&G.cs_trades);

    auto it = G.openTrades.find(tradeId);
    if (it == G.openTrades.end()) {
        LeaveCriticalSection(&G.cs_trades);
        return 0;
    }

    Trade& t = it->second;
    long long ctid = t.ctid;

    LeaveCriticalSection(&G.cs_trades);

    long long volumeInCents = (long long)((amount != 0 ? abs(amount) : abs(t.amount)) * 100000ll);

    // Close position
    char request[512];
    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
        "{\"ctidTraderAccountId\":%lld,"
        "\"positionId\":%lld,\"volume\":%lld}}",
        Utils::GetMsgId(), (int)PayloadType::PROTO_OA_CLOSE_POSITION_REQ, G.CTraderAccountId, ctid, volumeInCents);

    if (!Network::Send(request)) {
        Utils::ShowMsg("Close position failed");
        return 0;
    }

    return 1;
}

int GetTradeInfo(int tradeId, double* pOpen, double* pClose, double* pRoll, double* pProfit) {
    EnterCriticalSection(&G.cs_trades);

    auto it = G.openTrades.find(tradeId);
    if (it == G.openTrades.end()) {
        LeaveCriticalSection(&G.cs_trades);
        return 0;
    }

    Trade t = it->second;
    LeaveCriticalSection(&G.cs_trades);

    if (pOpen) *pOpen = t.openPrice;
    if (pClose) *pClose = t.closed ? t.closePrice : 0;
    if (pRoll) *pRoll = t.swap;
    if (pProfit) *pProfit = t.profit;

    return 1;
}

} // namespace Trading