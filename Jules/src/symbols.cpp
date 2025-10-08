// ============================================================================
// symbols.cpp - TELJES IMPLEMENTÁCIÓ
// ============================================================================

#include "../include/symbols.h"
#include "../include/globals.h"
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <ctime>

// External functions (provided by main.cpp)
extern void showMsg(const char* Text, const char* Detail);
extern void log_to_wesocket(const char* line1, const char* line2);
extern const char* get_msg_id();
extern bool tcp_send(const char* data);
extern bool body_indicates_error(const char* buffer);

namespace {
    std::map<std::string, SymbolInfo> g_symbols;
    std::map<long long, std::string> g_symbolById;
    std::map<std::string, std::string> g_pendingSubscriptions;
    CRITICAL_SECTION g_cs_symbols;

    constexpr int MAX_RETRY_COUNT = 3;
    constexpr int64_t RETRY_DELAYS[] = {5000, 15000, 30000};

    std::vector<std::string> split_csv_line(const std::string& line) {
        std::vector<std::string> result;
        std::string current;
        bool in_quotes = false;

        for (char c : line) {
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ',' && !in_quotes) {
                result.push_back(current);
                current.clear();
            } else {
                current.push_back(c);
            }
        }
        result.push_back(current);
        return result;
    }

    void send_subscription_request(long long accountId, const std::string& symbolName) {
        auto it = g_symbols.find(symbolName);
        if (it == g_symbols.end()) return;

        long long symbolId = it->second.id;
        std::string clientMsgId = get_msg_id();

        g_pendingSubscriptions[clientMsgId] = symbolName;

        char request[512];
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":2121,\"payload\":"
            "{\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld]}}",
            clientMsgId.c_str(), accountId, symbolId);

        if (!tcp_send(request)) {
            g_pendingSubscriptions.erase(clientMsgId);
        }
    }
}

namespace Symbols {

void Initialize() {
    InitializeCriticalSection(&g_cs_symbols);
    g_symbols.clear();
    g_symbolById.clear();
    g_pendingSubscriptions.clear();
}

void Cleanup() {
    Lock();
    g_symbols.clear();
    g_symbolById.clear();
    g_pendingSubscriptions.clear();
    Unlock();
    DeleteCriticalSection(&g_cs_symbols);
}

void Lock() {
    EnterCriticalSection(&g_cs_symbols);
}

void Unlock() {
    LeaveCriticalSection(&g_cs_symbols);
}

void AddSymbol(const std::string& name, long long id, int digits) {
    Lock();

    SymbolInfo info = {};
    info.id = id;
    info.digits = digits;
    info.subscribed = false;
    info.bid = 0;
    info.ask = 0;
    info.retry_count = 0;
    info.last_retry_time = 0;

    g_symbols[name] = info;
    g_symbolById[id] = name;

    Unlock();
}

SymbolInfo* GetSymbol(const std::string& symbolName) {
    Lock();
    auto it = g_symbols.find(symbolName);
    if (it != g_symbols.end()) {
        Unlock();
        return &it->second;
    }
    Unlock();
    return nullptr;
}

bool EnsureSubscribed(long long ctidTraderAccountId, const std::string& symbolName) {
    Lock();

    auto it = g_symbols.find(symbolName);
    if (it == g_symbols.end()) {
        Unlock();
        return false;
    }

    if (it->second.subscribed) {
        Unlock();
        return true;
    }

    if (it->second.retry_count >= MAX_RETRY_COUNT) {
        Unlock();
        return false;
    }

    int64_t now = (int64_t)time(nullptr) * 1000;
    int64_t delay = (it->second.retry_count > 0) ?
                    RETRY_DELAYS[it->second.retry_count - 1] : 0;

    if (now - it->second.last_retry_time < delay) {
        Unlock();
        return false;
    }

    it->second.last_retry_time = now;
    it->second.retry_count++;

    Unlock();

    send_subscription_request(ctidTraderAccountId, symbolName);
    return false;
}

void UpdateQuote(long long symbolId, long long bid, long long ask) {
    Lock();

    auto idIt = g_symbolById.find(symbolId);
    if (idIt == g_symbolById.end()) {
        Unlock();
        return;
    }

    std::string name = idIt->second;
    auto it = g_symbols.find(name);
    if (it != g_symbols.end()) {
        it->second.bid = bid;
        it->second.ask = ask;

        if (!it->second.subscribed) {
            it->second.subscribed = true;
            it->second.retry_count = 0;
        }
    }

    Unlock();
}

void BatchResubscribe(long long ctidTraderAccountId) {
    Lock();

    std::vector<long long> symbolIds;
    for (const auto& kv : g_symbols) {
        if (kv.second.subscribed) {
            symbolIds.push_back(kv.second.id);
        }
    }

    Unlock();

    if (symbolIds.empty()) return;

    const size_t BATCH_SIZE = 50;
    for (size_t i = 0; i < symbolIds.size(); i += BATCH_SIZE) {
        size_t end = (i + BATCH_SIZE < symbolIds.size()) ?
                     i + BATCH_SIZE : symbolIds.size();

        std::string ids;
        for (size_t j = i; j < end; ++j) {
            if (j > i) ids += ",";
            char buf[32];
            sprintf_s(buf, "%lld", symbolIds[j]);
            ids += buf;
        }

        char request[2048];
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":2121,\"payload\":"
            "{\"ctidTraderAccountId\":%lld,\"symbolId\":[%s]}}",
            get_msg_id(), ctidTraderAccountId, ids.c_str());

        tcp_send(request);
    }
}

void HandleSubscriptionResponse(const std::string& clientMsgId, bool success,
                               const std::string& error_details) {
    Lock();

    auto pit = g_pendingSubscriptions.find(clientMsgId);
    if (pit == g_pendingSubscriptions.end()) {
        Unlock();
        return;
    }

    std::string symbolName = pit->second;
    g_pendingSubscriptions.erase(pit);

    auto it = g_symbols.find(symbolName);
    if (it == g_symbols.end()) {
        Unlock();
        return;
    }

    if (success) {
        it->second.subscribed = true;
        it->second.retry_count = 0;
    } else {
        char msg[256];
        sprintf_s(msg, "Subscription failed: %s - %s",
                 symbolName.c_str(), error_details.c_str());
        log_to_wesocket("SUBSCRIPTION_ERROR", msg);
    }

    Unlock();
}

void ProcessRetries(long long ctidTraderAccountId) {
    Lock();

    std::vector<std::string> toRetry;
    int64_t now = (int64_t)time(nullptr) * 1000;

    for (auto& kv : g_symbols) {
        if (!kv.second.subscribed &&
            kv.second.retry_count > 0 &&
            kv.second.retry_count < MAX_RETRY_COUNT) {

            int64_t delay = RETRY_DELAYS[kv.second.retry_count - 1];
            if (now - kv.second.last_retry_time >= delay) {
                toRetry.push_back(kv.first);
            }
        }
    }

    Unlock();

    for (const auto& name : toRetry) {
        EnsureSubscribed(ctidTraderAccountId, name);
    }
}

void GenerateBrokerAssetsFile(const char* dllPath) {
    char assetFixPath[MAX_PATH];
    sprintf_s(assetFixPath, "%sAssetFix.csv", dllPath);

    std::ifstream inFile(assetFixPath);
    if (!inFile.is_open()) {
        // Try History folder if not found in Plugin folder
        char historyPath[MAX_PATH];
        sprintf_s(historyPath, "%sHistory\\AssetFix.csv", dllPath);
        inFile.open(historyPath);

        if (!inFile.is_open()) {
            log_to_wesocket("ASSET_GEN", "AssetFix.csv not found in Plugin or History folder, skipping");
            return;
        } else {
            log_to_wesocket("ASSET_GEN", "Using AssetFix.csv from History folder");
        }
    } else {
        log_to_wesocket("ASSET_GEN", "Using AssetFix.csv from Plugin folder");
    }

    std::map<std::string, std::string> mapping;
    std::string line;

    while (std::getline(inFile, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto parts = split_csv_line(line);
        if (parts.size() >= 2) {
            std::string key = parts[0];
            std::string value = parts[1];

            key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
            key.erase(std::remove(key.begin(), key.end(), '\t'), key.end());
            key.erase(std::remove(key.begin(), key.end(), '\r'), key.end());

            value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
            value.erase(std::remove(value.begin(), value.end(), '\t'), value.end());
            value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());

            if (!key.empty() && !value.empty()) {
                mapping[key] = value;
            }
        }
    }
    inFile.close();

    char assetListPath[MAX_PATH];
    sprintf_s(assetListPath, "%sAssetList.txt", dllPath);

    std::ofstream outFile(assetListPath);
    if (!outFile.is_open()) {
        log_to_wesocket("ASSET_GEN", "Cannot create AssetList.txt");
        return;
    }

    Lock();
    for (const auto& kv : g_symbols) {
        std::string originalName = kv.first;
        std::string outputName = originalName;

        auto mit = mapping.find(originalName);
        if (mit != mapping.end()) {
            outputName = mit->second;
        }

        outFile << outputName << "\n";
    }
    Unlock();

    outFile.close();

    char msg[256];
    sprintf_s(msg, "AssetList.txt generated (%zu symbols)", g_symbols.size());
    log_to_wesocket("ASSET_GEN", msg);
}

std::string NormalizeSymbol(const char* symbol) {
    if (!symbol) return "";

    std::string normalized = symbol;

    // Remove slashes (EUR/USD -> EURUSD)
    size_t pos = 0;
    while ((pos = normalized.find('/', pos)) != std::string::npos) {
        normalized.erase(pos, 1);
    }

    // Remove dots (GER30.f -> GER30f)
    pos = 0;
    while ((pos = normalized.find('.', pos)) != std::string::npos) {
        normalized.erase(pos, 1);
    }

    // Remove spaces
    pos = 0;
    while ((pos = normalized.find(' ', pos)) != std::string::npos) {
        normalized.erase(pos, 1);
    }

    // Convert to uppercase
    for (char& c : normalized) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }

    return normalized;
}

std::string AddSlashToSymbol(const char* symbol) {
    if (!symbol) return "";

    std::string input = symbol;

    // Convert to uppercase first
    for (char& c : input) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }

    // If already has slash, return as-is
    if (input.find('/') != std::string::npos) {
        return input;
    }

    // Check if it's a 6-character forex pair (EURUSD -> EUR/USD)
    if (input.length() == 6) {
        std::string first = input.substr(0, 3);
        std::string second = input.substr(3, 3);

        // Basic currency code validation
        const char* currencies[] = {"EUR", "USD", "GBP", "JPY", "AUD", "CAD", "CHF", "NZD",
                                   "SEK", "NOK", "DKK", "PLN", "CZK", "HUF", "TRY", "ZAR",
                                   "MXN", "SGD", "HKD", "CNH", "RUB", nullptr};

        bool firstValid = false, secondValid = false;
        for (int i = 0; currencies[i]; i++) {
            if (first == currencies[i]) firstValid = true;
            if (second == currencies[i]) secondValid = true;
        }

        if (firstValid && secondValid) {
            return first + "/" + second;
        }
    }

    return input; // Return unchanged if not a recognized forex pair
}

SymbolInfo* GetSymbolByIdOrName(const char* symbol) {
    if (!symbol) {
        log_to_wesocket("SYMBOL_LOOKUP", "Symbol is NULL");
        return nullptr;
    }

    char msg[256];
    sprintf_s(msg, "Looking for symbol: '%s'", symbol);
    log_to_wesocket("SYMBOL_LOOKUP", msg);

    Lock();

    // Try exact match first
    std::string exactSymbol = symbol;
    auto it = g_symbols.find(exactSymbol);
    if (it != g_symbols.end()) {
        sprintf_s(msg, "FOUND exact match: '%s' -> id=%lld", symbol, it->second.id);
        log_to_wesocket("SYMBOL_LOOKUP", msg);
        Unlock();
        return &it->second;
    }

    // Try normalized format (EUR/USD -> EURUSD)
    std::string normalized = NormalizeSymbol(symbol);
    sprintf_s(msg, "Trying normalized: '%s' -> '%s'", symbol, normalized.c_str());
    log_to_wesocket("SYMBOL_LOOKUP", msg);

    it = g_symbols.find(normalized);
    if (it != g_symbols.end()) {
        sprintf_s(msg, "FOUND normalized match: '%s' -> id=%lld", normalized.c_str(), it->second.id);
        log_to_wesocket("SYMBOL_LOOKUP", msg);
        Unlock();
        return &it->second;
    }

    // Try adding slash format (EURUSD -> EUR/USD)
    std::string withSlash = AddSlashToSymbol(symbol);
    if (withSlash != exactSymbol) {  // Only try if it's different
        sprintf_s(msg, "Trying with slash: '%s' -> '%s'", symbol, withSlash.c_str());
        log_to_wesocket("SYMBOL_LOOKUP", msg);

        it = g_symbols.find(withSlash);
        if (it != g_symbols.end()) {
            sprintf_s(msg, "FOUND slash match: '%s' -> id=%lld", withSlash.c_str(), it->second.id);
            log_to_wesocket("SYMBOL_LOOKUP", msg);
            Unlock();
            return &it->second;
        }
    }

    // Try partial matches (for things like EURUSD vs EURUSDt)
    sprintf_s(msg, "Trying partial matches for: '%s'", normalized.c_str());
    log_to_wesocket("SYMBOL_LOOKUP", msg);

    for (auto& kv : g_symbols) {
        if (kv.first.find(normalized) == 0 || normalized.find(kv.first) == 0) {
            sprintf_s(msg, "FOUND partial match: '%s' matches '%s' -> id=%lld",
                     normalized.c_str(), kv.first.c_str(), kv.second.id);
            log_to_wesocket("SYMBOL_LOOKUP", msg);
            Unlock();
            return &kv.second;
        }
    }

    sprintf_s(msg, "SYMBOL NOT FOUND: '%s' (total symbols: %zu)", symbol, g_symbols.size());
    log_to_wesocket("SYMBOL_LOOKUP", msg);

    Unlock();
    return nullptr;
}

} // namespace Symbols