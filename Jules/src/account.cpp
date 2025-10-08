#include "../include/account.h"
#include "../include/globals.h"
#include "../include/utils.h"
#include "../include/network.h"

namespace Account {

struct AccountInfo {
    double balance;
    double equity;
    double margin;
    double marginLevel;
    double freeMargin;
    std::string currency;
    long long updateTime;
};

static AccountInfo g_accountInfo = {0};

bool RequestAccountInfo() {
    if (G.CTraderAccountId == 0) return false;

    // Request account trader info (payloadType 2121)
    char request[512];
    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":2121,\"payload\":"
        "{\"ctidTraderAccountId\":%lld}}",
        Utils::GetMsgId(), G.CTraderAccountId);

    if (!Network::Send(request)) {
        Utils::ShowMsg("Account info request failed");
        return false;
    }

    // Note: Response will be processed in NetworkThread
    return true;
}

void UpdateAccountInfo(double balance, double equity, double margin, const char* currency) {
    g_accountInfo.balance = balance;
    g_accountInfo.equity = equity;
    g_accountInfo.margin = margin;
    g_accountInfo.marginLevel = (margin > 0) ? (equity / margin) * 100.0 : 0.0;
    g_accountInfo.freeMargin = equity - margin;
    if (currency) g_accountInfo.currency = currency;
    g_accountInfo.updateTime = GetTickCount64();

    char msg[256];
    sprintf_s(msg, "Account updated: Balance=%.2f, Equity=%.2f, Margin=%.2f, Free=%.2f",
              balance, equity, margin, g_accountInfo.freeMargin);
    Utils::LogToFile("ACCOUNT_UPDATE", msg);
}

bool GetAccountData(double* pBalance, double* pEquity, double* pMargin, double* pFreeMargin, char* currency) {
    // If data is older than 30 seconds, request fresh data
    ULONGLONG now = GetTickCount64();
    if (now - g_accountInfo.updateTime > 30000) {
        RequestAccountInfo();
    }

    if (pBalance) *pBalance = g_accountInfo.balance;
    if (pEquity) *pEquity = g_accountInfo.equity;
    if (pMargin) *pMargin = g_accountInfo.margin;
    if (pFreeMargin) *pFreeMargin = g_accountInfo.freeMargin;
    if (currency) strcpy_s(currency, 32, g_accountInfo.currency.c_str());

    return g_accountInfo.updateTime > 0; // Return true if we have data
}

double GetBalance() {
    return g_accountInfo.balance;
}

double GetEquity() {
    return g_accountInfo.equity;
}

double GetMargin() {
    return g_accountInfo.margin;
}

double GetFreeMargin() {
    return g_accountInfo.freeMargin;
}

} // namespace Account