#include "../include/account.h"
#include "../include/globals.h"
#include "../include/utils.h"
#include "../include/network.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

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
static MarginSnapshot g_latestMarginSnapshot;
static std::vector<CashFlowEntry> g_cashFlowHistory;
constexpr size_t MAX_CASHFLOW_ENTRIES = 128;
static ULONGLONG g_lastAccountInfoRequestMs = 0;

namespace {

double parse_double(const std::string& json, const char* field, double defaultValue = 0.0) {
    if (!field) return defaultValue;
    std::string key = std::string("\"") + field + "\":";
    size_t pos = json.find(key);
    if (pos == std::string::npos) {
        key = std::string("\"") + field + "\" :";
        pos = json.find(key);
        if (pos == std::string::npos) return defaultValue;
    }
    pos += key.length();
    while (pos < json.length() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    size_t end = pos;
    while (end < json.length() && (std::isdigit(static_cast<unsigned char>(json[end])) ||
           json[end] == '-' || json[end] == '+' || json[end] == '.' || json[end] == 'e' || json[end] == 'E')) {
        end++;
    }
    if (end == pos) return defaultValue;
    try {
        return std::stod(json.substr(pos, end - pos));
    } catch (...) {
        return defaultValue;
    }
}

long long parse_int64(const std::string& json, const char* field, long long defaultValue = 0) {
    if (!field) return defaultValue;
    std::string key = std::string("\"") + field + "\":";
    size_t pos = json.find(key);
    if (pos == std::string::npos) {
        key = std::string("\"") + field + "\" :";
        pos = json.find(key);
        if (pos == std::string::npos) return defaultValue;
    }
    pos += key.length();
    while (pos < json.length() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    size_t end = pos;
    while (end < json.length() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-' || json[end] == '+')) {
        end++;
    }
    if (end == pos) return defaultValue;
    try {
        return std::stoll(json.substr(pos, end - pos));
    } catch (...) {
        return defaultValue;
    }
}

std::string parse_string(const std::string& json, const char* field, const std::string& defaultValue = "") {
    if (!field) return defaultValue;
    std::string key = std::string("\"") + field + "\":";
    size_t pos = json.find(key);
    if (pos == std::string::npos) {
        key = std::string("\"") + field + "\" :";
        pos = json.find(key);
        if (pos == std::string::npos) return defaultValue;
    }
    pos += key.length();
    while (pos < json.length() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    if (pos >= json.length() || json[pos] != '"') return defaultValue;
    pos++;
    std::ostringstream oss;
    while (pos < json.length()) {
        char c = json[pos];
        if (c == '\\' && pos + 1 < json.length()) {
            pos++;
            oss << json[pos];
        } else if (c == '"') {
            break;
        } else {
            oss << c;
        }
        pos++;
    }
    return oss.str();
}

std::string extract_object_json(const char* start, const char* end) {
    if (!start) return {};
    const char* objStart = strchr(start, '{');
    if (!objStart || (end && objStart >= end)) return {};
    int depth = 1;
    const char* cursor = objStart + 1;
    while (*cursor && depth > 0 && (!end || cursor < end)) {
        if (*cursor == '{') depth++;
        else if (*cursor == '}') depth--;
        cursor++;
    }
    if (depth != 0) return {};
    return std::string(objStart, cursor - objStart);
}

void append_cash_flow_entry(const CashFlowEntry& entry) {
    if (entry.cashFlowId == 0 && std::fabs(entry.amount) < 1e-9) return;
    if (entry.cashFlowId != 0) {
        auto it = std::find_if(g_cashFlowHistory.begin(), g_cashFlowHistory.end(),
            [&entry](const CashFlowEntry& existing) {
                return existing.cashFlowId == entry.cashFlowId;
            });
        if (it != g_cashFlowHistory.end()) {
            *it = entry;
            return;
        }
    }
    g_cashFlowHistory.push_back(entry);
    if (g_cashFlowHistory.size() > MAX_CASHFLOW_ENTRIES) {
        g_cashFlowHistory.erase(g_cashFlowHistory.begin(), g_cashFlowHistory.begin() + (g_cashFlowHistory.size() - MAX_CASHFLOW_ENTRIES));
    }
}

} // namespace

bool RequestAccountInfo() {
    if (G.CTraderAccountId == 0) return false;

    // Throttle: Don't send requests more than once every 30 seconds
    constexpr ULONGLONG THROTTLE_MS = 30000;
    ULONGLONG now = GetTickCount64();
    if (now - g_lastAccountInfoRequestMs < THROTTLE_MS) {
        // Too soon - skip request
        return false;
    }

    // Request account trader info (PROTO_OA_TRADER_REQ)
    char request[512];
    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
        "{\"ctidTraderAccountId\":%lld}}",
        Utils::GetMsgId(), ToInt(PayloadType::TraderReq), G.CTraderAccountId);

    if (!Network::Send(request)) {
        Utils::ShowMsg("Account info request failed");
        return false;
    }

    // Update last request time
    g_lastAccountInfoRequestMs = now;

    // Note: Response will be processed in NetworkThread
    return true;
}

bool RequestCashFlowHistory(int limit) {
    if (G.CTraderAccountId == 0) return false;
    if (limit <= 0) limit = 20;
    if (limit > 200) limit = 200;

    // Calculate Unix timestamps in milliseconds - API requires fromTimestamp and toTimestamp
    // Max range is 1 week (604800000 ms), so we request the last week of data
    ULONGLONG nowMs = (ULONGLONG)time(NULL) * 1000LL; // Unix time in milliseconds
    ULONGLONG oneWeekMs = 604800000LL; // 7 days in milliseconds
    ULONGLONG toTimestamp = nowMs;
    ULONGLONG fromTimestamp = nowMs - oneWeekMs;

    char request[512];
    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{"
        "\"ctidTraderAccountId\":%lld,\"fromTimestamp\":%llu,\"toTimestamp\":%llu}}",
        Utils::GetMsgId(), ToInt(PayloadType::CashFlowHistoryReq), G.CTraderAccountId, fromTimestamp, toTimestamp);

    if (!Network::Send(request)) {
        Utils::LogToFile("CASH_FLOW_REQ", "Failed to send cash flow history request (PROTO_OA_CASH_FLOW_HISTORY_LIST_REQ)");
        return false;
    }

    Utils::LogToFile("CASH_FLOW_REQ", "Cash flow history request sent with time range (PROTO_OA_CASH_FLOW_HISTORY_LIST_REQ)");
    G.cashFlowRequested = true;
    return true;
}

bool SendAccountAuth(long long accountId) {
    if (accountId <= 0) return false;
    if (!G.Token[0]) {
        Utils::LogToFile("ACCOUNT_AUTH", "No access token available for PROTO_OA_ACCOUNT_AUTH_REQ");
        return false;
    }

    char request[1024];
    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{"
        "\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld}}",
        Utils::GetMsgId(), ToInt(PayloadType::AccountAuthReq), G.Token, accountId);

    if (!Network::Send(request)) {
        Utils::LogToFile("ACCOUNT_AUTH", "Failed to send account auth request (PROTO_OA_ACCOUNT_AUTH_REQ)");
        return false;
    }

    Utils::LogToFile("ACCOUNT_AUTH", "Account auth request dispatched (PROTO_OA_ACCOUNT_AUTH_REQ)");
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

    g_latestMarginSnapshot.balance = balance;
    g_latestMarginSnapshot.equity = equity;
    g_latestMarginSnapshot.margin = margin;
    g_latestMarginSnapshot.freeMargin = g_accountInfo.freeMargin;
    g_latestMarginSnapshot.marginLevel = g_accountInfo.marginLevel;
    g_latestMarginSnapshot.timestamp = static_cast<long long>(g_accountInfo.updateTime);
    g_latestMarginSnapshot.source = "TRADER_RES";
    G.lastMarginUpdateMs = g_accountInfo.updateTime;

    char msg[256];
    sprintf_s(msg, "Account updated: Balance=%.2f, Equity=%.2f, Margin=%.2f, Free=%.2f",
              balance, equity, margin, g_accountInfo.freeMargin);
    Utils::LogToFile("ACCOUNT_UPDATE", msg);
}

void HandleMarginChangedEvent(const char* buffer) {
    if (!buffer) return;

    const char* payloadStart = strstr(buffer, "\"payload\":");
    if (!payloadStart) return;

    std::string payloadJson = extract_object_json(payloadStart, nullptr);
    if (payloadJson.empty()) return;

    double balance = parse_double(payloadJson, "balance", 0.0) / 100.0;
    double equity = parse_double(payloadJson, "equity", 0.0) / 100.0;
    double margin = parse_double(payloadJson, "margin", 0.0) / 100.0;
    if (std::fabs(margin) < 1e-9) {
        margin = parse_double(payloadJson, "usedMargin", g_accountInfo.margin);
    }
    double freeMargin = parse_double(payloadJson, "freeMargin", equity - margin);
    double marginLevel = parse_double(payloadJson, "marginLevel", (margin > 0) ? (equity / margin) * 100.0 : g_accountInfo.marginLevel);
    std::string currency = parse_string(payloadJson, "currency", g_accountInfo.currency);
    if (currency.empty()) {
        currency = parse_string(payloadJson, "depositCurrency", g_accountInfo.currency);
    }
    std::string reason = parse_string(payloadJson, "reason", "MARGIN_EVENT");
    if (reason.empty()) {
        reason = parse_string(payloadJson, "cause", "MARGIN_EVENT");
    }

    long long ts = parse_int64(payloadJson, "timestamp", 0);
    if (ts == 0) {
        ts = parse_int64(payloadJson, "eventTimestamp", static_cast<long long>(GetTickCount64()));
    }

    g_accountInfo.balance = balance;
    g_accountInfo.equity = equity;
    g_accountInfo.margin = margin;
    g_accountInfo.freeMargin = freeMargin;
    g_accountInfo.marginLevel = marginLevel;
    if (!currency.empty()) g_accountInfo.currency = currency;
    g_accountInfo.updateTime = GetTickCount64();

    g_latestMarginSnapshot.balance = balance;
    g_latestMarginSnapshot.equity = equity;
    g_latestMarginSnapshot.margin = margin;
    g_latestMarginSnapshot.freeMargin = freeMargin;
    g_latestMarginSnapshot.marginLevel = marginLevel;
    g_latestMarginSnapshot.timestamp = ts;
    g_latestMarginSnapshot.source = reason;

    G.lastMarginUpdateMs = g_accountInfo.updateTime;

    char msg[320];
    sprintf_s(msg, "Margin event: Balance=%.2f, Equity=%.2f, Margin=%.2f, Free=%.2f, Level=%.2f, Reason=%s",
              balance, equity, margin, freeMargin, marginLevel, reason.c_str());
    Utils::LogToFile("MARGIN_EVENT", msg);
}

void HandleCashFlowHistoryResponse(const char* buffer) {
    if (!buffer) return;

    const char* payloadStart = strstr(buffer, "\"payload\":");
    if (!payloadStart) return;

    const char* arrayAnchor = strstr(payloadStart, "\"cashFlow");
    if (!arrayAnchor) {
        arrayAnchor = strstr(payloadStart, "\"cashFlows");
    }
    if (!arrayAnchor) {
        Utils::LogToFile("CASH_FLOW_PARSE", "No cash flow array present");
        return;
    }

    const char* arrayStart = strchr(arrayAnchor, '[');
    if (!arrayStart) return;

    const char* cursor = arrayStart + 1;
    int parsed = 0;
    while (*cursor) {
        if (*cursor == '{') {
            const char* objectStart = cursor;
            int depth = 1;
            const char* walker = cursor + 1;
            while (*walker && depth > 0) {
                if (*walker == '{') depth++;
                else if (*walker == '}') depth--;
                walker++;
            }
            if (depth != 0) break;
            std::string objJson(objectStart, walker - objectStart);

            CashFlowEntry entry;
            entry.cashFlowId = parse_int64(objJson, "cashFlowId", parse_int64(objJson, "id", 0));
            entry.amount = parse_double(objJson, "amount", 0.0);
            entry.balance = parse_double(objJson, "balance", 0.0);
            entry.equity = parse_double(objJson, "equity", 0.0);
            entry.margin = parse_double(objJson, "margin", 0.0);
            entry.profit = parse_double(objJson, "profit", 0.0);
            entry.timestamp = parse_int64(objJson, "timestamp", parse_int64(objJson, "eventTimestamp", 0));
            entry.currency = parse_string(objJson, "currency", parse_string(objJson, "depositCurrency", g_accountInfo.currency));
            entry.description = parse_string(objJson, "description", parse_string(objJson, "comment", ""));
            entry.type = parse_string(objJson, "type", parse_string(objJson, "cashFlowType", ""));

            append_cash_flow_entry(entry);

            char logLine[384];
            sprintf_s(logLine, "CashFlow id=%lld amount=%.2f balance=%.2f type=%s",
                      entry.cashFlowId, entry.amount, entry.balance, entry.type.c_str());
            Utils::LogToFile("CASH_FLOW_ENTRY", logLine);

            parsed++;
            cursor = walker;
        } else if (*cursor == ']') {
            break;
        } else {
            cursor++;
        }
    }

    if (parsed == 0) {
        Utils::LogToFile("CASH_FLOW_PARSE", "Cash flow response contained zero entries");
    } else {
        Utils::LogToFile("CASH_FLOW_PARSE", "Cash flow history snapshot parsed");
        G.cashFlowHydrated = true;
    }
}

bool GetAccountData(double* pBalance, double* pEquity, double* pMargin, double* pFreeMargin, char* currency) {
    // If data is older than 30 seconds, request fresh data
    ULONGLONG now = GetTickCount64();
    if (now - g_accountInfo.updateTime > 30000) {
        RequestAccountInfo();
    }

    // Use server equity if we have it, otherwise use balance
    double equity = (g_accountInfo.equity > 0.0) ? g_accountInfo.equity : g_accountInfo.balance;

    if (pBalance) *pBalance = g_accountInfo.balance;
    if (pEquity) *pEquity = equity;
    if (pMargin) *pMargin = g_accountInfo.margin;
    if (pFreeMargin) *pFreeMargin = g_accountInfo.freeMargin;
    if (currency) strcpy_s(currency, 32, g_accountInfo.currency.c_str());

    return g_accountInfo.updateTime > 0; // Return true if we have data
}

MarginSnapshot GetLatestMarginSnapshot() {
    return g_latestMarginSnapshot;
}

const std::vector<CashFlowEntry>& GetCashFlowHistory() {
    return g_cashFlowHistory;
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

