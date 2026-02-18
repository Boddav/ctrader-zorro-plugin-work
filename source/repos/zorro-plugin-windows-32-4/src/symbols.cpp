#include "../include/state.h"
#include "../include/symbols.h"
#include "../include/protocol.h"
#include "../include/websocket.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace Symbols {

// Forward declarations
static std::string NormalizeSymbol(const char* name);
static std::string FindSymbolName(const char* name);

bool RequestSymbolList() {
    const int MAX_RETRIES = 3;

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        char payload[128];
        sprintf_s(payload, "\"ctidTraderAccountId\":%lld", G.accountId);

        const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                                 PayloadType::SymbolsListReq, payload);
        if (!WebSocket::Send(msg)) {
            Log::Error("SYM", "SymbolsListReq send failed (attempt %d/%d)", attempt, MAX_RETRIES);
            if (attempt < MAX_RETRIES) { Sleep(1000); continue; }
            return false;
        }

        // Wait for response
        char response[131072] = {0};
        ULONGLONG start = Utils::NowMs();
        while (Utils::NowMs() - start < (ULONGLONG)G.waitTime) {
            int n = WebSocket::Receive(response, sizeof(response));
            if (n > 0) {
                int pt = Protocol::ExtractPayloadType(response);
                if (pt == ToInt(PayloadType::SymbolsListRes)) {
                    HandleSymbolsListRes(response);
                    return true;
                }
                if (pt == ToInt(PayloadType::ErrorRes)) {
                    Log::Error("SYM", "SymbolsListReq error: %s",
                              Protocol::ExtractString(response, "description"));
                    return false;
                }
            }
            Sleep(10);
        }

        Log::Warn("SYM", "SymbolsListReq timeout (attempt %d/%d)", attempt, MAX_RETRIES);
        if (attempt < MAX_RETRIES) {
            Sleep(2000);  // wait before retry
        }
    }

    Log::Error("SYM", "SymbolsListReq failed after %d attempts", MAX_RETRIES);
    return false;
}

void HandleSymbolsListRes(const char* buffer) {
    CsLock lock(G.csSymbols);

    const char* arr = Protocol::ExtractArray(buffer, "symbol");
    int count = Protocol::CountArrayElements(arr);

    Log::Info("SYM", "Received %d symbols", count);

    for (int i = 0; i < count; i++) {
        const char* elem = Protocol::GetArrayElement(arr, i);
        if (!elem || !*elem) continue;

        long long symbolId = Protocol::ExtractInt64(elem, "symbolId");
        const char* name = Protocol::ExtractString(elem, "symbolName");
        bool enabled = Protocol::ExtractBool(elem, "enabled");

        if (symbolId > 0 && name && *name && enabled) {
            SymbolInfo& sym = G.symbols[name];
            sym.symbolId = symbolId;
            sym.name = name;
            sym.baseAssetId = Protocol::ExtractInt64(elem, "baseAssetId");
            sym.quoteAssetId = Protocol::ExtractInt64(elem, "quoteAssetId");

            G.symbolIdToName[symbolId] = name;
        }
    }

    Log::Info("SYM", "Stored %d enabled symbols", (int)G.symbols.size());
}

bool RequestSymbolDetails() {
    // Collect IDs under lock, then do network calls unlocked
    std::vector<long long> ids;
    {
        CsLock lock(G.csSymbols);
        if (G.symbols.empty()) return false;
        for (auto& kv : G.symbols) {
            ids.push_back(kv.second.symbolId);
        }
    }  // lock released here, exactly once

    // Send in batches of 50
    const int BATCH = 50;
    for (size_t offset = 0; offset < ids.size(); offset += BATCH) {
        size_t end = (offset + BATCH < ids.size()) ? offset + BATCH : ids.size();

        // Build symbolId array
        char idList[4096] = {0};
        int pos = 0;
        for (size_t j = offset; j < end; j++) {
            if (j > offset) pos += sprintf_s(idList + pos, sizeof(idList) - pos, ",");
            pos += sprintf_s(idList + pos, sizeof(idList) - pos, "%lld", ids[j]);
        }

        char payload[4200];
        sprintf_s(payload, "\"ctidTraderAccountId\":%lld,\"symbolId\":[%s]",
                  G.accountId, idList);

        const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                                 PayloadType::SymbolByIdReq, payload);
        if (!WebSocket::Send(msg)) return false;

        // Wait for response
        char response[131072] = {0};
        ULONGLONG start = Utils::NowMs();
        while (Utils::NowMs() - start < (ULONGLONG)G.waitTime) {
            int n = WebSocket::Receive(response, sizeof(response));
            if (n > 0) {
                int pt = Protocol::ExtractPayloadType(response);
                if (pt == ToInt(PayloadType::SymbolByIdRes)) {
                    HandleSymbolByIdRes(response);
                    break;
                }
                if (pt == ToInt(PayloadType::ErrorRes)) {
                    Log::Warn("SYM", "SymbolByIdReq batch error");
                    break;
                }
            }
            Sleep(10);
        }
    }

    return true;
}

void HandleSymbolByIdRes(const char* buffer) {
    CsLock lock(G.csSymbols);

    const char* arr = Protocol::ExtractArray(buffer, "symbol");
    int count = Protocol::CountArrayElements(arr);

    for (int i = 0; i < count; i++) {
        const char* elem = Protocol::GetArrayElement(arr, i);
        if (!elem || !*elem) continue;

        long long symbolId = Protocol::ExtractInt64(elem, "symbolId");

        // Find by ID in reverse map
        auto it = G.symbolIdToName.find(symbolId);
        if (it == G.symbolIdToName.end()) continue;

        auto sit = G.symbols.find(it->second);
        if (sit == G.symbols.end()) continue;

        SymbolInfo& sym = sit->second;
        sym.digits = Protocol::ExtractInt(elem, "digits");
        sym.pipPosition = Protocol::ExtractInt(elem, "pipPosition");
        sym.lotSize = Protocol::ExtractInt64(elem, "lotSize");
        sym.minVolume = Protocol::ExtractInt64(elem, "minVolume");
        sym.maxVolume = Protocol::ExtractInt64(elem, "maxVolume");
        sym.stepVolume = Protocol::ExtractInt64(elem, "stepVolume");
        sym.swapLong = Protocol::ExtractDouble(elem, "swapLong");
        sym.swapShort = Protocol::ExtractDouble(elem, "swapShort");

        // Default lotSize if not set
        if (sym.lotSize <= 0) sym.lotSize = 100000;
        if (sym.minVolume <= 0) sym.minVolume = 1000;
        if (sym.stepVolume <= 0) sym.stepVolume = 1000;
    }

    Log::Info("SYM", "Updated details for %d symbols", count);
}

bool Subscribe(const char* symbolName) {
    if (!symbolName || !*symbolName) return false;

    long long symbolId = 0;
    {
        CsLock lock(G.csSymbols);
        std::string actual = FindSymbolName(symbolName);
        if (actual.empty()) {
            Log::Warn("SYM", "Symbol not found: %s", symbolName);
            return false;
        }

        auto& sym = G.symbols[actual];
        if (sym.subscribed) return true;  // Already subscribed
        symbolId = sym.symbolId;
        sym.subscribed = true;  // Mark optimistically
    }

    char payload[256];
    sprintf_s(payload, "\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld]",
              G.accountId, symbolId);

    const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                             PayloadType::SubscribeSpotsReq, payload);
    if (!WebSocket::Send(msg)) return false;

    Log::Info("SYM", "Subscribe sent for %s (id=%lld)", symbolName, symbolId);

    // Don't wait for response - NetworkThread will handle SubscribeSpotsRes
    // Give server a moment to start sending quotes
    Sleep(200);
    return true;
}

void BatchResubscribe() {
    std::vector<std::pair<std::string, long long>> toSub;
    {
        CsLock lock(G.csSymbols);
        for (auto& kv : G.symbols) {
            if (kv.second.subscribed) {
                toSub.push_back({kv.first, kv.second.symbolId});
                kv.second.subscribed = false;
            }
        }
    }  // lock released here, exactly once

    for (auto& s : toSub) {
        char payload[256];
        sprintf_s(payload, "\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld]",
                  G.accountId, s.second);

        const char* msg = Protocol::BuildMessage(Utils::NextMsgId(),
                                                 PayloadType::SubscribeSpotsReq, payload);
        WebSocket::Send(msg);
        Sleep(50);  // Small delay between subscriptions
    }
}

void HandleSpotEvent(const char* buffer) {
    long long symbolId = Protocol::ExtractInt64(buffer, "symbolId");
    if (symbolId <= 0) return;

    CsLock lock(G.csSymbols);

    auto it = G.symbolIdToName.find(symbolId);
    if (it == G.symbolIdToName.end()) return;

    auto sit = G.symbols.find(it->second);
    if (sit == G.symbols.end()) return;

    SymbolInfo& sym = sit->second;

    // Prices come as raw integers, divide by PRICE_SCALE
    long long rawBid = Protocol::ExtractInt64(buffer, "bid");
    long long rawAsk = Protocol::ExtractInt64(buffer, "ask");

    if (rawBid > 0) sym.bid = (double)rawBid / PRICE_SCALE;
    if (rawAsk > 0) sym.ask = (double)rawAsk / PRICE_SCALE;

    // Timestamp
    long long ts = Protocol::ExtractInt64(buffer, "timestamp");
    if (ts > 0) {
        sym.lastQuoteTime = ts;
        G.lastServerTimestamp = ts;
    }

    sym.subscribed = true;
    G.quoteCount++;
    G.lastQuoteRecvMs = GetTickCount64();
}

// Normalize symbol name: strip slashes, dots, spaces, convert to uppercase
// e.g. "EUR/USD" -> "EURUSD", "EUR/USD." -> "EURUSD"
static std::string NormalizeSymbol(const char* name) {
    std::string result;
    for (const char* p = name; *p; p++) {
        if (*p != '/' && *p != '.' && *p != ' ' && *p != '-') {
            result += (char)toupper((unsigned char)*p);
        }
    }
    return result;
}

// Find the actual cTrader symbol name from a Zorro name
// Zorro may send "EUR/USD" but cTrader stores "EURUSD" (or vice versa)
static std::string FindSymbolName(const char* name) {
    // First try exact match
    if (G.symbols.count(name)) return name;

    // Normalize input and try matching against all known symbols
    std::string normalized = NormalizeSymbol(name);
    for (auto& kv : G.symbols) {
        std::string symNorm = NormalizeSymbol(kv.first.c_str());
        if (symNorm == normalized) return kv.first;
    }

    return "";  // Not found
}

bool GetSymbol(const char* name, SymbolInfo& out) {
    if (!name || !*name) return false;
    CsLock lock(G.csSymbols);

    std::string actual = FindSymbolName(name);
    if (actual.empty()) return false;

    out = G.symbols[actual];
    return true;
}

const char* GetNameById(long long symbolId) {
    __declspec(thread) static char name[128];
    name[0] = '\0';
    CsLock lock(G.csSymbols);
    auto it = G.symbolIdToName.find(symbolId);
    if (it != G.symbolIdToName.end()) {
        strcpy_s(name, it->second.c_str());
    }
    return name;
}

} // namespace Symbols
