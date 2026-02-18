#include "../include/state.h"
#include "../include/account.h"
#include "../include/protocol.h"
#include "../include/websocket.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include <cstdio>
#include <cmath>

namespace Account {

bool RequestTraderInfo() {
    char payload[128];
    sprintf_s(payload, "\"ctidTraderAccountId\":%lld", G.accountId);

    const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                             PayloadType::TraderReq, payload);
    if (!WebSocket::Send(msg)) return false;

    char response[16384] = {0};
    ULONGLONG start = Utils::NowMs();
    while (Utils::NowMs() - start < (ULONGLONG)G.waitTime) {
        int n = WebSocket::Receive(response, sizeof(response));
        if (n > 0) {
            int pt = Protocol::ExtractPayloadType(response);
            if (pt == ToInt(PayloadType::TraderRes)) {
                HandleTraderRes(response);
                return true;
            }
            if (pt == ToInt(PayloadType::ErrorRes)) {
                Log::Error("ACC", "TraderReq error: %s",
                          Protocol::ExtractString(response, "description"));
                return false;
            }
        }
        Sleep(10);
    }

    Log::Error("ACC", "TraderReq timeout");
    return false;
}

void HandleTraderRes(const char* buffer) {
    const char* trader = strstr(buffer, "\"trader\"");
    if (!trader) trader = buffer;

    int md = Protocol::ExtractInt(trader, "moneyDigits");
    if (md > 0) {
        G.moneyDigits = md;
    }

    double scale = pow(10.0, (double)G.moneyDigits);

    // Bug #18: use HasField to check presence, not value
    if (Protocol::HasField(trader, "balance")) {
        G.balance = (double)Protocol::ExtractInt64(trader, "balance") / scale;
    }

    // Account leverage (e.g. 50000 = 500:1)
    long long lev = Protocol::ExtractInt64(trader, "leverageInCents");
    if (lev > 0) {
        G.leverageInCents = lev;
    }

    // Bug #10: initialize equity from balance if not yet set
    if (G.equity <= 0.0 && G.balance > 0.0) {
        G.equity = G.balance;
        G.freeMargin = G.balance;
        Log::Info("ACC", "Equity initialized from balance: %.2f", G.equity);
    }

    Log::Info("ACC", "Balance=%.2f moneyDigits=%d leverage=%lld (%.0f:1)",
              G.balance, G.moneyDigits, G.leverageInCents, (double)G.leverageInCents / 100.0);
}

void HandleMarginChangedEvent(const char* buffer) {
    double scale = pow(10.0, (double)G.moneyDigits);

    // Bug #18: use HasField to check presence (value 0 is valid)
    if (Protocol::HasField(buffer, "equity"))
        G.equity = (double)Protocol::ExtractInt64(buffer, "equity") / scale;
    if (Protocol::HasField(buffer, "usedMargin"))
        G.margin = (double)Protocol::ExtractInt64(buffer, "usedMargin") / scale;
    if (Protocol::HasField(buffer, "freeMargin"))
        G.freeMargin = (double)Protocol::ExtractInt64(buffer, "freeMargin") / scale;
    if (Protocol::HasField(buffer, "balance"))
        G.balance = (double)Protocol::ExtractInt64(buffer, "balance") / scale;

    Log::Diag(1, "Margin update: bal=%.2f eq=%.2f margin=%.2f free=%.2f",
              G.balance, G.equity, G.margin, G.freeMargin);
}

void HandleTraderUpdateEvent(const char* buffer) {
    double scale = pow(10.0, (double)G.moneyDigits);

    if (Protocol::HasField(buffer, "balance")) {
        G.balance = (double)Protocol::ExtractInt64(buffer, "balance") / scale;
    }

    int md = Protocol::ExtractInt(buffer, "moneyDigits");
    if (md > 0) {
        G.moneyDigits = md;
    }
}

} // namespace Account
