// ============================================================================
// symbols.cpp - TELJES IMPLEMENT�CI�
// ============================================================================

#include "../include/symbols.h"
#include "../include/globals.h"
#include "../include/utils.h"
#include "../include/network.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <cctype>
#include <cstdlib>
#include <cmath>

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
    std::map<std::string, std::pair<std::string, int>> g_pendingDepthSubscriptions;
    std::map<std::string, std::string> g_pendingDepthUnsubscribes;
    CRITICAL_SECTION g_cs_symbols;

    struct SubStats {
        uint64_t totalSent = 0;
        uint64_t totalAcked = 0;
        uint64_t totalFirstQuote = 0;
        uint64_t totalRetries = 0;
        uint64_t totalFailures = 0;
        uint64_t forcedResubscribe = 0;
        uint64_t quietResubscribe = 0;
        ULONGLONG lastLogMs = 0;
    };

    SubStats g_subStats;

    constexpr size_t SUB_BATCH_SIZE = 10;  // Send subscriptions in batches of 10
    constexpr ULONGLONG SUB_PENDING_TIMEOUT_MS = 8000;      // wait for ACK before retrying
    constexpr ULONGLONG SUB_FIRST_QUOTE_TIMEOUT_MS = 15000; // wait for first quote after ACK
    constexpr ULONGLONG SUB_QUIET_TIMEOUT_MS = 60000;       // resubscribe quiet symbols
    constexpr ULONGLONG SUB_STATS_INTERVAL_MS = 5000;
    constexpr int CTRADER_SPOT_SUBSCRIBE_REQ = ToInt(PayloadType::SpotSubscribeReq);

    bool g_spotPayloadWarningEmitted = false;

    struct AssetCatalogEntry {
        std::string displayName;
        long long assetId = 0;
        long long symbolId = 0;
        int digits = 0;
        int pipPosition = 0;
        double pipSize = 0.0;
        int64_t lotSize = 0;        // Volume parameter for AssetList.txt
        int64_t minVolume = 0;      // Min volume for AssetList.txt
        int64_t maxVolume = 0;      // Max volume for AssetList.txt
        int64_t stepVolume = 0;     // Volume step size
        int64_t maxExposure = 0;    // Max exposure limit
        int64_t commission = 0;     // Commission in micro-units
        int commissionType = 0;     // Commission type
        double swapLong = 0.0;      // Swap for long positions
        double swapShort = 0.0;     // Swap for short positions
        int swapRollover3Days = 0;  // Rollover day
        bool enableShortSelling = true;
        bool guaranteedStopLoss = false;
        bool fromAssetList = false;
    };

    std::map<std::string, AssetCatalogEntry> g_assetCatalog;
    std::map<long long, std::string> g_assetIdToKey;
    std::map<long long, std::string> g_assetClassNames;

    constexpr int MAX_RETRY_COUNT = 3;
    constexpr int64_t RETRY_DELAYS[] = {5000, 15000, 30000};
    constexpr int DEFAULT_DEPTH_LEVELS = 10;

    void update_asset_catalog_entry_locked(const std::string& key,
                                           const std::string& displayName,
                                           long long assetId,
                                           long long symbolId,
                                           int digits,
                                           int pipPosition,
                                           double pipSize,
                                           bool fromAssetList);

    void sync_asset_catalog_from_symbols_locked(bool createMissingEntries);

    void write_asset_list_file(const char* dllPath, const char* originLabel);

    bool parse_asset_list_payload(const char* response, std::vector<AssetCatalogEntry>& outEntries);

    void log_spot_payload_override_once() {
        if (!g_spotPayloadWarningEmitted) {
            char msg[160];
            sprintf_s(msg, "Using SpotSubscribe payloadType %d", CTRADER_SPOT_SUBSCRIBE_REQ);
            Utils::LogToFile("SPOT_PAYLOAD_INFO", msg);
            g_spotPayloadWarningEmitted = true;
        }
    }

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

    void update_asset_catalog_entry_locked(const std::string& key,
                                           const std::string& displayName,
                                           long long assetId,
                                           long long symbolId,
                                           int digits,
                                           int pipPosition,
                                           double pipSize,
                                           bool fromAssetList) {
        if (key.empty()) return;

        AssetCatalogEntry& entry = g_assetCatalog[key];

        if (fromAssetList) {
            if (!displayName.empty()) {
                entry.displayName = displayName;
            }
            if (assetId > 0) {
                entry.assetId = assetId;
                g_assetIdToKey[assetId] = key;
            }
            if (digits > 0) {
                entry.digits = digits;
            }
            if (pipPosition > 0) {
                entry.pipPosition = pipPosition;
            }
            if (pipSize > 0.0) {
                entry.pipSize = pipSize;
            }
            entry.fromAssetList = true;
        } else {
            if (entry.displayName.empty() && !displayName.empty()) {
                entry.displayName = displayName;
            }
            if (assetId > 0 && entry.assetId == 0) {
                entry.assetId = assetId;
                g_assetIdToKey[assetId] = key;
            }
            if (symbolId > 0) {
                entry.symbolId = symbolId;
            }
            if (digits > 0) {
                entry.digits = digits;
            }
        }
    }

    void sync_asset_catalog_from_symbols_locked(bool createMissingEntries) {
        for (const auto& kv : g_symbols) {
            const std::string& symbolName = kv.first;
            const SymbolInfo& info = kv.second;

            std::string key = Symbols::NormalizeSymbol(symbolName.c_str());
            if (key.empty()) {
                key = symbolName;
            }

            if (!createMissingEntries && g_assetCatalog.find(key) == g_assetCatalog.end()) {
                continue;
            }

            update_asset_catalog_entry_locked(key, symbolName, 0, info.id, info.digits,
                                              0, 0.0, false);
        }
    }

    bool extract_long(const char* start, const char* end, const char* token, long long& out) {
        if (!start || !token) return false;
        const char* pos = strstr(start, token);
        if (!pos || (end && pos >= end)) return false;
        pos += strlen(token);

        while ((!end || pos < end) && (*pos == ' ' || *pos == '\t')) pos++;

        char buffer[64] = {0};
        size_t idx = 0;
        while ((!end || pos < end) && idx < sizeof(buffer) - 1) {
            char c = *pos;
            if (!(c == '-' || c == '+' || (c >= '0' && c <= '9'))) break;
            buffer[idx++] = c;
            pos++;
        }
        if (idx == 0) return false;
        buffer[idx] = '\0';
        out = _strtoui64(buffer, nullptr, 10);
        return true;
    }

    bool extract_int(const char* start, const char* end, const char* token, int& out) {
        long long value = 0;
        if (!extract_long(start, end, token, value)) return false;
        out = static_cast<int>(value);
        return true;
    }

    bool extract_double(const char* start, const char* end, const char* token, double& out) {
        if (!start || !token) return false;
        const char* pos = strstr(start, token);
        if (!pos || (end && pos >= end)) return false;
        pos += strlen(token);

        while ((!end || pos < end) && (*pos == ' ' || *pos == '\t')) pos++;

        char buffer[64] = {0};
        size_t idx = 0;
        while ((!end || pos < end) && idx < sizeof(buffer) - 1) {
            char c = *pos;
            if (!(c == '-' || c == '+' || (c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E')) break;
            buffer[idx++] = c;
            pos++;
        }
        if (idx == 0) return false;
        buffer[idx] = '\0';
        out = atof(buffer);
        return true;
    }

    bool extract_string(const char* start, const char* end, const char* token, std::string& out) {
        if (!start || !token) return false;
        const char* pos = strstr(start, token);
        if (!pos || (end && pos >= end)) return false;
        pos += strlen(token);

        const char* cursor = pos;
        std::string result;
        while ((!end || cursor < end) && *cursor && *cursor != '"') {
            if (*cursor == '\\' && (!end || cursor + 1 < end)) {
                cursor++;
                if (*cursor) result.push_back(*cursor);
            } else {
                result.push_back(*cursor);
            }
            cursor++;
        }

        if (*cursor != '"') return false;
        out = result;
        return true;
    }

    bool parse_asset_list_payload(const char* response, std::vector<AssetCatalogEntry>& outEntries) {
        if (!response) return false;

        const char* arrayStart = strstr(response, "\"asset\":[");
        if (!arrayStart) arrayStart = strstr(response, "\"assets\":[");
        if (!arrayStart) return false;

        const char* arrayOpen = strchr(arrayStart, '[');
        if (!arrayOpen) return false;

        const char* cursor = arrayOpen + 1;
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
                const char* objectEnd = walker - 1;

                AssetCatalogEntry entry;
                entry.displayName.clear();
                entry.assetId = 0;
                entry.symbolId = 0;
                entry.digits = 0;
                entry.pipPosition = 0;
                entry.pipSize = 0.0;
                entry.fromAssetList = true;

                extract_string(objectStart, objectEnd, "\"symbolName\":\"", entry.displayName);
                if (entry.displayName.empty()) {
                    extract_string(objectStart, objectEnd, "\"name\":\"", entry.displayName);
                }
                if (entry.displayName.empty()) {
                    extract_string(objectStart, objectEnd, "\"assetName\":\"", entry.displayName);
                }

                extract_long(objectStart, objectEnd, "\"assetId\":", entry.assetId);
                long long parsedSymbolId = 0;
                if (extract_long(objectStart, objectEnd, "\"symbolId\":", parsedSymbolId)) {
                    entry.symbolId = parsedSymbolId;
                }

                int parsedDigits = 0;
                if (extract_int(objectStart, objectEnd, "\"digits\":", parsedDigits)) {
                    entry.digits = parsedDigits;
                }

                int pipPos = 0;
                if (extract_int(objectStart, objectEnd, "\"pipPosition\":", pipPos)) {
                    entry.pipPosition = pipPos;
                }

                double pipSz = 0.0;
                if (extract_double(objectStart, objectEnd, "\"pipSize\":", pipSz)) {
                    entry.pipSize = pipSz;
                }

                if (!entry.displayName.empty() || entry.assetId > 0) {
                    outEntries.push_back(entry);
                }

                cursor = walker;
            } else if (*cursor == ']') {
                break;
            } else {
                cursor++;
            }
        }

        return !outEntries.empty();
    }

    void write_asset_list_file(const char* dllPath, const char* originLabel) {
        if (!dllPath) return;

        std::ifstream inFile;
        char foundPath[MAX_PATH] = "";

        // First try: Assets*.csv in ../History folder (relative to Plugin)
        char searchPattern[MAX_PATH];
        sprintf_s(searchPattern, "%s..\\History\\Assets*.csv", dllPath);

        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPattern, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            sprintf_s(foundPath, "%s..\\History\\%s", dllPath, findData.cFileName);
            FindClose(hFind);
            inFile.open(foundPath);
            if (inFile.is_open()) {
                char msg[256];
                sprintf_s(msg, "Using %s from ../History folder", findData.cFileName);
                log_to_wesocket("ASSET_GEN", msg);
            }
        }

        // Second try: AssetsFix.csv in Plugin folder
        if (!inFile.is_open()) {
            char pluginPath[MAX_PATH];
            sprintf_s(pluginPath, "%sAssetsFix.csv", dllPath);
            inFile.open(pluginPath);
            if (inFile.is_open()) {
                log_to_wesocket("ASSET_GEN", "Using AssetsFix.csv from Plugin folder");
                strcpy_s(foundPath, pluginPath);
            }
        }

        if (!inFile.is_open()) {
            log_to_wesocket("ASSET_GEN", "Assets*.csv not found in ../History or Plugin folder, skipping");
            return;
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

        std::vector<AssetCatalogEntry> snapshot;
        {
            Symbols::Lock();
            if (!g_assetCatalog.empty()) {
                snapshot.reserve(g_assetCatalog.size());
                for (const auto& kv : g_assetCatalog) {
                    snapshot.push_back(kv.second);
                }
            } else {
                snapshot.reserve(g_symbols.size());
                for (const auto& kv : g_symbols) {
                    AssetCatalogEntry entry;
                    entry.displayName = kv.first;
                    entry.symbolId = kv.second.id;
                    entry.digits = kv.second.digits;
                    entry.assetId = 0;
                    entry.pipPosition = kv.second.pipPosition;
                    entry.pipSize = kv.second.pipSize;
                    // FIX: Copy ALL volume and trading parameters from g_symbols
                    entry.lotSize = kv.second.lotSize;
                    entry.minVolume = kv.second.minVolume;
                    entry.maxVolume = kv.second.maxVolume;
                    entry.stepVolume = kv.second.stepVolume;
                    entry.maxExposure = kv.second.maxExposure;
                    entry.commission = kv.second.commission;
                    entry.commissionType = kv.second.commissionType;
                    entry.swapLong = kv.second.swapLong;
                    entry.swapShort = kv.second.swapShort;
                    entry.swapRollover3Days = kv.second.swapRollover3Days;
                    entry.enableShortSelling = kv.second.enableShortSelling;
                    entry.guaranteedStopLoss = kv.second.guaranteedStopLoss;
                    entry.fromAssetList = false;
                    snapshot.push_back(entry);
                }
            }
            Symbols::Unlock();
        }

        bool snapshotHasIds = false;
        for (const auto& entry : snapshot) {
            if (entry.symbolId > 0) {
                snapshotHasIds = true;
                break;
            }
        }

        if (!snapshotHasIds) {
            snapshot.clear();
            Symbols::Lock();
            snapshot.reserve(g_symbols.size());
            for (const auto& kv : g_symbols) {
                if (kv.second.id <= 0) continue;
                AssetCatalogEntry entry;
                entry.displayName = kv.first;
                entry.symbolId = kv.second.id;
                entry.digits = kv.second.digits;
                entry.assetId = kv.second.assetId;
                entry.pipPosition = kv.second.pipPosition;
                entry.pipSize = kv.second.pipSize;
                // FIX: Copy ALL volume and trading parameters from g_symbols (fallback branch)
                entry.lotSize = kv.second.lotSize;
                entry.minVolume = kv.second.minVolume;
                entry.maxVolume = kv.second.maxVolume;
                entry.stepVolume = kv.second.stepVolume;
                entry.maxExposure = kv.second.maxExposure;
                entry.commission = kv.second.commission;
                entry.commissionType = kv.second.commissionType;
                entry.swapLong = kv.second.swapLong;
                entry.swapShort = kv.second.swapShort;
                entry.swapRollover3Days = kv.second.swapRollover3Days;
                entry.enableShortSelling = kv.second.enableShortSelling;
                entry.guaranteedStopLoss = kv.second.guaranteedStopLoss;
                entry.fromAssetList = false;
                snapshot.push_back(entry);
            }
            Symbols::Unlock();

            if (!snapshot.empty()) {
                Utils::LogToFile("ASSET_GEN_FALLBACK", "Asset catalog missing symbol IDs; rebuilt snapshot from current symbol map");
            }
        }

        char assetListPath[MAX_PATH];
        sprintf_s(assetListPath, "%sAssetList.txt", dllPath);

        std::ofstream outFile(assetListPath);
        if (!outFile.is_open()) {
            log_to_wesocket("ASSET_GEN", "Cannot create AssetList.txt");
            return;
        }

        size_t written = 0;
        size_t skippedWithoutId = 0;
        for (auto& entry : snapshot) {
            if (entry.displayName.empty()) continue;

            std::string outputName = entry.displayName;
            auto mit = mapping.find(entry.displayName);
            if (mit != mapping.end()) {
                outputName = mit->second;
            }

            if (outputName.empty()) continue;

            if (entry.symbolId <= 0) {
                skippedWithoutId++;
                continue;
            }

            int digits = entry.digits;
            if (digits <= 0 && entry.pipPosition > 0) {
                digits = entry.pipPosition;
            }
            if (digits <= 0) digits = 5;

            long long idToWrite = entry.symbolId;
            const char* sourceLabel = "SYMBOL";

            // Extended AssetList.txt format with all trading parameters
            outFile << outputName << ";"
                    << idToWrite << ";"
                    << digits << ";"
                    << sourceLabel << ";"
                    << entry.lotSize << ";"
                    << entry.minVolume << ";"
                    << entry.maxVolume << ";"
                    << entry.stepVolume << ";"
                    << entry.maxExposure << ";"
                    << entry.commission << ";"
                    << entry.commissionType << ";"
                    << entry.swapLong << ";"
                    << entry.swapShort << ";"
                    << entry.swapRollover3Days << ";"
                    << (entry.enableShortSelling ? 1 : 0) << ";"
                    << (entry.guaranteedStopLoss ? 1 : 0) << ";"
                    << entry.pipPosition << "\n";
            written++;
        }

        outFile.close();

        char msg[256];
        if (originLabel && originLabel[0] != '\0') {
            sprintf_s(msg, "AssetList.txt generated (%zu entries, origin=%s)", written, originLabel);
        } else {
            sprintf_s(msg, "AssetList.txt generated (%zu entries)", written);
        }
        log_to_wesocket("ASSET_GEN", msg);

        if (skippedWithoutId > 0) {
            char skipMsg[256];
            sprintf_s(skipMsg, "AssetList generation skipped %zu entries without symbolId", skippedWithoutId);
            log_to_wesocket("ASSET_GEN", skipMsg);
        }
    }

    void send_subscription_request(long long accountId, const std::string& symbolName) {
        std::string clientMsgId;
        long long symbolId = 0;
        int64_t attemptNumber = 0;

        Symbols::Lock();
        auto it = g_symbols.find(symbolName);
        if (it == g_symbols.end()) {
            Symbols::Unlock();
            return;
        }

        symbolId = it->second.id;
        if (symbolId <= 0) {
            it->second.retry_count = MAX_RETRY_COUNT;
            if (!it->second.failureLogged) {
                g_subStats.totalFailures++;
                it->second.failureLogged = true;
            }
            Symbols::Unlock();

            char skipMsg[256];
            sprintf_s(skipMsg, "Skipping spot subscription for %s (no symbolId available)", symbolName.c_str());
            Utils::LogToFile("SPOT_SUB_SKIP", skipMsg);
            return;
        }

        attemptNumber = it->second.retry_count;
        clientMsgId = get_msg_id();
        g_pendingSubscriptions[clientMsgId] = symbolName;
        Symbols::Unlock();

        std::string request = Symbols::BuildSubscribeSpotsReq(accountId, symbolId, symbolName, clientMsgId);

        if (!tcp_send(request.c_str())) {
            char failMsg[256];
            sprintf_s(failMsg, "Spot subscribe send failed for %s (ID: %lld)", symbolName.c_str(), symbolId);
            Utils::LogToFile("SPOT_SUB_REQ_FAIL", failMsg);

            Symbols::Lock();
            g_pendingSubscriptions.erase(clientMsgId);
            auto retryIt = g_symbols.find(symbolName);
            if (retryIt != g_symbols.end()) {
                if (retryIt->second.retry_count > 0) {
                    retryIt->second.retry_count--;
                }
                retryIt->second.subscriptionPending = false;
                retryIt->second.failureLogged = false;
            }
            Symbols::Unlock();
            return;
        }

        ULONGLONG sendMs = GetTickCount64();

        Symbols::Lock();
        auto retryIt = g_symbols.find(symbolName);
        if (retryIt != g_symbols.end()) {
            SymbolInfo& sym = retryIt->second;
            sym.subscriptionPending = true;
            sym.subscriptionAcked = false;
            if (!sym.hasFirstQuote) {
                sym.subscribed = false;
            }
            sym.lastSubscribeSentMs = sendMs;
            sym.failureLogged = false;
        }

        g_subStats.totalSent++;
        if (attemptNumber > 1) {
            g_subStats.totalRetries++;
        }
        Symbols::Unlock();
    }

    void send_depth_subscription_request(long long accountId, const std::string& symbolName, int depthLevels) {
        auto it = g_symbols.find(symbolName);
        if (it == g_symbols.end()) return;

        long long symbolId = it->second.id;
        std::string clientMsgId = get_msg_id();

        g_pendingDepthSubscriptions[clientMsgId] = std::make_pair(symbolName, depthLevels);

        std::string request = Symbols::BuildSubscribeDepthReq(accountId, symbolId, symbolName, depthLevels);

        if (!tcp_send(request.c_str())) {
            char failMsg[256];
            sprintf_s(failMsg, "Depth subscribe send failed for %s (ID: %lld)", symbolName.c_str(), symbolId);
            Utils::LogToFile("DEPTH_SUB_REQ_FAIL", failMsg);
            g_pendingDepthSubscriptions.erase(clientMsgId);
        }
    }

    constexpr double PRICE_EPSILON = 1e-9;

    void apply_depth_delta(std::vector<DepthLevel>& book, const std::vector<DepthLevel>& delta, bool isBid, int maxLevels) {
        for (const auto& level : delta) {
            auto it = std::find_if(book.begin(), book.end(), [&](const DepthLevel& existing) {
                return fabs(existing.price - level.price) < PRICE_EPSILON;
            });

            if (level.volume <= 0.0) {
                if (it != book.end()) {
                    book.erase(it);
                }
                continue;
            }

            if (it != book.end()) {
                it->price = level.price;
                it->volume = level.volume;
                it->orders = level.orders;
            } else {
                book.push_back(level);
            }
        }

        auto comparator = isBid
            ? [](const DepthLevel& a, const DepthLevel& b) { return a.price > b.price; }
            : [](const DepthLevel& a, const DepthLevel& b) { return a.price < b.price; };
        std::sort(book.begin(), book.end(), comparator);

        if (maxLevels > 0 && static_cast<int>(book.size()) > maxLevels) {
            book.resize(static_cast<size_t>(maxLevels));
        }
    }
}

namespace Symbols {

void Initialize() {
    InitializeCriticalSection(&g_cs_symbols);
    g_symbols.clear();
    g_symbolById.clear();
    g_pendingSubscriptions.clear();
    g_pendingDepthSubscriptions.clear();
    g_pendingDepthUnsubscribes.clear();
    g_assetCatalog.clear();
    g_assetIdToKey.clear();
    g_assetClassNames.clear();
    g_subStats = {};
}

void Cleanup() {
    std::vector<std::string> depthSubscriptions;

    {
        Lock();
        for (const auto& kv : g_symbols) {
            if (kv.second.depthSubscribed && kv.second.id > 0) {
                depthSubscriptions.push_back(kv.first);
            }
        }
        Unlock();
    }

    if (G.CTraderAccountId != 0) {
        for (const auto& name : depthSubscriptions) {
            UnsubscribeDepth(G.CTraderAccountId, name);
            Sleep(2);
        }
    }

    Lock();
    g_symbols.clear();
    g_symbolById.clear();
    g_pendingSubscriptions.clear();
    g_pendingDepthSubscriptions.clear();
    g_pendingDepthUnsubscribes.clear();
    g_assetCatalog.clear();
    g_assetIdToKey.clear();
    g_assetClassNames.clear();
    g_subStats = {};
    Unlock();
    DeleteCriticalSection(&g_cs_symbols);
}

void Lock() {
    EnterCriticalSection(&g_cs_symbols);
}

void Unlock() {
    LeaveCriticalSection(&g_cs_symbols);
}

void AddSymbol(const std::string& name, long long id, int digits, long long assetId,
               int pipPosition, double pipSize, int tradingModeInt,
               int64_t minVolume, int64_t maxVolume, int64_t lotSize,
               double swapLong, double swapShort, int swapRollover3Days) {
    Utils::LogToFile("ADD_SYMBOL_DEBUG", ("Adding symbol: " + name).c_str());
    Lock();

    SymbolInfo info = {};
    info.id = id;
    info.assetId = assetId;
    info.digits = digits;

    // ADD THESE NEW FIELD ASSIGNMENTS:
    info.pipPosition = pipPosition;
    info.pipSize = pipSize;
    info.tradingMode = static_cast<TradingMode>(tradingModeInt);
    info.minVolume = minVolume;
    info.maxVolume = maxVolume;
    info.lotSize = lotSize;
    info.swapLong = swapLong;
    info.swapShort = swapShort;
    info.swapRollover3Days = swapRollover3Days;

    info.requestedDepthLevels = DEFAULT_DEPTH_LEVELS;

    g_symbols[name] = info;
    if (id > 0) {
        g_symbolById[id] = name;
    }

    std::string key;
    if (assetId > 0) {
        auto itKey = g_assetIdToKey.find(assetId);
        if (itKey != g_assetIdToKey.end()) {
            key = itKey->second;
        }
    }

    if (key.empty()) {
        key = NormalizeSymbol(name.c_str());
        if (key.empty()) {
            key = name;
        }
    }

    update_asset_catalog_entry_locked(key, name, assetId, id, digits, 0, 0.0, false);

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
    ULONGLONG nowTick = GetTickCount64();
    int64_t nowMs = static_cast<int64_t>(time(nullptr)) * 1000;

    Lock();

    auto it = g_symbols.find(symbolName);
    if (it == g_symbols.end()) {
        Unlock();
        return false;
    }

    SymbolInfo& info = it->second;

    if (info.id <= 0) {
        info.retry_count = MAX_RETRY_COUNT;
        Unlock();

        char skipMsg[256];
        sprintf_s(skipMsg, "EnsureSubscribed skip for %s (no symbolId available)", symbolName.c_str());
        Utils::LogToFile("SPOT_SUB_SKIP", skipMsg);
        return false;
    }

    if (info.subscribed && info.hasFirstQuote) {
        Unlock();
        return true;
    }

    if (info.subscriptionPending) {
        ULONGLONG elapsed = nowTick - info.lastSubscribeSentMs;
        if (elapsed < SUB_PENDING_TIMEOUT_MS) {
            Unlock();
            return false;
        }

        info.subscriptionPending = false;
        info.subscriptionAcked = false;
        info.subscribed = false;
        info.hasFirstQuote = false;
        info.last_retry_time = 0;
        info.failureLogged = false;
        g_subStats.forcedResubscribe++;

        char logMsg[256];
        sprintf_s(logMsg, "Spot subscribe pending timeout for %s after %.1f sec", symbolName.c_str(), elapsed / 1000.0);
        Utils::LogToFile("SPOT_SUB_TIMEOUT", logMsg);
    }

    if (info.subscriptionAcked && !info.hasFirstQuote) {
        ULONGLONG elapsed = nowTick - info.lastAckMs;
        if (elapsed < SUB_FIRST_QUOTE_TIMEOUT_MS) {
            Unlock();
            return false;
        }

        info.subscriptionAcked = false;
        info.subscribed = false;
        info.hasFirstQuote = false;
        info.last_retry_time = 0;
        info.failureLogged = false;
        g_subStats.forcedResubscribe++;

        char logMsg[256];
        sprintf_s(logMsg, "No spot quote after ACK for %s in %.1f sec, resubscribing", symbolName.c_str(), elapsed / 1000.0);
        Utils::LogToFile("SPOT_SUB_ACK_TIMEOUT", logMsg);
    }

    if (info.retry_count >= MAX_RETRY_COUNT) {
        if (!info.failureLogged) {
            g_subStats.totalFailures++;
            info.failureLogged = true;
        }
        Unlock();
        return false;
    }

    int64_t delay = (info.retry_count > 0) ? RETRY_DELAYS[info.retry_count - 1] : 0;
    if (info.retry_count > 0 && (nowMs - info.last_retry_time) < delay) {
        Unlock();
        return false;
    }

    info.last_retry_time = nowMs;
    info.retry_count++;
    info.subscriptionPending = false;
    info.subscriptionAcked = false;
    info.subscribed = false;
    info.failureLogged = false;
    Unlock();

    send_subscription_request(ctidTraderAccountId, symbolName);
    return false;
}

bool EnsureDepthSubscribed(long long ctidTraderAccountId, const std::string& symbolName, int depthLevels) {
    Lock();

    auto it = g_symbols.find(symbolName);
    if (it == g_symbols.end()) {
        Unlock();
        return false;
    }

    if (it->second.id <= 0) {
        it->second.depthRetryCount = MAX_RETRY_COUNT;
        Unlock();

        char skipMsg[256];
        sprintf_s(skipMsg, "EnsureDepthSubscribed skip for %s (no symbolId available)", symbolName.c_str());
        Utils::LogToFile("DEPTH_SUB_SKIP", skipMsg);
        return false;
    }

    if (it->second.depthSubscribed) {
        if (it->second.requestedDepthLevels != depthLevels && depthLevels > 0) {
            it->second.requestedDepthLevels = depthLevels;
        }
        Unlock();
        return true;
    }

    if (it->second.depthRetryCount >= MAX_RETRY_COUNT) {
        Unlock();
        return false;
    }

    int64_t now = (int64_t)time(nullptr) * 1000;
    int64_t delay = (it->second.depthRetryCount > 0) ?
                    RETRY_DELAYS[it->second.depthRetryCount - 1] : 0;

    if (now - it->second.lastDepthRetryTime < delay) {
        Unlock();
        return false;
    }

    it->second.lastDepthRetryTime = now;
    it->second.depthRetryCount++;
    if (depthLevels > 0) {
        it->second.requestedDepthLevels = depthLevels;
    }
    Unlock();

    send_depth_subscription_request(ctidTraderAccountId, symbolName, depthLevels > 0 ? depthLevels : DEFAULT_DEPTH_LEVELS);
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
        it->second.lastQuoteMs = GetTickCount64();
        if (!it->second.hasFirstQuote) {
            it->second.hasFirstQuote = true;
            g_subStats.totalFirstQuote++;

            // cTrader API: bid/ask are ALWAYS in 1/100000 of unit (fixed scale)
            const double CTRADER_PRICE_SCALE = 100000.0;
            double bidPrice = static_cast<double>(bid) / CTRADER_PRICE_SCALE;
            double askPrice = static_cast<double>(ask) / CTRADER_PRICE_SCALE;

            char logMsg[256];
            sprintf_s(logMsg,
                      "%s first quote raw bid=%lld ask=%lld digits=%d => bid=%.6f ask=%.6f",
                      name.c_str(), bid, ask, it->second.digits, bidPrice, askPrice);
            Utils::LogToFile("QUOTE_FIRST_DETAIL", logMsg);
        }
        it->second.subscriptionPending = false;
        it->second.subscriptionAcked = true;
        it->second.subscribed = true;
        it->second.retry_count = 0;
        it->second.failureLogged = false;
    }

    Unlock();
}

void BatchResubscribe(long long ctidTraderAccountId) {
    Lock();

    std::vector<long long> symbolIds;
    for (const auto& kv : g_symbols) {
        if (kv.second.subscribed && kv.second.id > 0) {
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
        log_spot_payload_override_once();
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
            "{\"ctidTraderAccountId\":%lld,\"symbolId\":[%s]}}",
            get_msg_id(), CTRADER_SPOT_SUBSCRIBE_REQ, ctidTraderAccountId, ids.c_str());

        std::string logMsg = "Batch subscribe via " + std::to_string(CTRADER_SPOT_SUBSCRIBE_REQ) +
                              " with " + std::to_string(end - i) + " symbols";
        Utils::LogToFile("SPOT_BATCH_REQ", logMsg.c_str());
        tcp_send(request);
    }
}

void BatchResubscribeDepth(long long ctidTraderAccountId, int depthLevels) {
    struct DepthRequest {
        std::string name;
        long long id;
        int levels;
    };

    std::vector<DepthRequest> depthSymbols;

    Lock();
    for (const auto& kv : g_symbols) {
        if (kv.second.depthSubscribed && kv.second.id > 0) {
            DepthRequest req;
            req.name = kv.first;
            req.id = kv.second.id;
            req.levels = (kv.second.requestedDepthLevels > 0) ? kv.second.requestedDepthLevels : depthLevels;
            depthSymbols.push_back(req);
        }
    }
    Unlock();

    if (depthSymbols.empty()) {
        return;
    }

    for (const auto& item : depthSymbols) {
        send_depth_subscription_request(ctidTraderAccountId, item.name, item.levels);
        Sleep(5); // slight throttle to avoid bursts
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
        SymbolInfo& info = it->second;
        info.subscriptionPending = false;
        info.subscriptionAcked = true;
        info.lastAckMs = GetTickCount64();
        info.failureLogged = false;
        g_subStats.totalAcked++;
    } else {
        char msg[256];
        sprintf_s(msg, "Subscription failed: %s - %s",
                 symbolName.c_str(), error_details.c_str());
        log_to_wesocket("SUBSCRIPTION_ERROR", msg);

        std::string detailsUpper = error_details;
        std::transform(detailsUpper.begin(), detailsUpper.end(), detailsUpper.begin(), ::toupper);
        if (detailsUpper.find("SYMBOL_NOT_FOUND") != std::string::npos ||
            detailsUpper.find("INVALID_SYMBOL_ID") != std::string::npos ||
            detailsUpper.find("NOT_FOUND") != std::string::npos) {
            it->second.retry_count = MAX_RETRY_COUNT;
            g_subStats.totalFailures++;
            it->second.failureLogged = true;
        }
        it->second.subscriptionPending = false;
        it->second.subscriptionAcked = false;
        it->second.subscribed = false;
        it->second.hasFirstQuote = false;
        it->second.last_retry_time = 0;
    }

    Unlock();
}

void HandleDepthSubscriptionResponse(const std::string& clientMsgId, bool success,
                                     const std::string& error_details) {
    Lock();

    auto pit = g_pendingDepthSubscriptions.find(clientMsgId);
    if (pit == g_pendingDepthSubscriptions.end()) {
        Unlock();
        return;
    }

    std::string symbolName = pit->second.first;
    int requestedLevels = pit->second.second > 0 ? pit->second.second : DEFAULT_DEPTH_LEVELS;
    g_pendingDepthSubscriptions.erase(pit);

    auto it = g_symbols.find(symbolName);
    if (it == g_symbols.end()) {
        Unlock();
        return;
    }

    if (success) {
        it->second.depthSubscribed = true;
        it->second.depthRetryCount = 0;
        it->second.requestedDepthLevels = requestedLevels;
    } else {
        char msg[256];
        sprintf_s(msg, "Depth subscription failed: %s - %s",
                 symbolName.c_str(), error_details.c_str());
        log_to_wesocket("DEPTH_SUB_ERROR", msg);

        std::string detailsUpper = error_details;
        std::transform(detailsUpper.begin(), detailsUpper.end(), detailsUpper.begin(), ::toupper);
        if (detailsUpper.find("SYMBOL_NOT_FOUND") != std::string::npos ||
            detailsUpper.find("INVALID_SYMBOL_ID") != std::string::npos ||
            detailsUpper.find("NOT_FOUND") != std::string::npos) {
            it->second.depthRetryCount = MAX_RETRY_COUNT;
        }
    }

    Unlock();
}

void HandleDepthUnsubscribeResponse(const std::string& clientMsgId, bool success,
                                     const std::string& error_details) {
    std::string symbolName;

    Lock();
    auto pit = g_pendingDepthUnsubscribes.find(clientMsgId);
    if (pit != g_pendingDepthUnsubscribes.end()) {
        symbolName = pit->second;
        g_pendingDepthUnsubscribes.erase(pit);
    }

    if (!symbolName.empty()) {
        auto it = g_symbols.find(symbolName);
        if (it != g_symbols.end()) {
            if (success) {
                it->second.depthSubscribed = false;
                it->second.bidDepth.clear();
                it->second.askDepth.clear();
            } else {
                it->second.depthSubscribed = true;
            }
        }
    }
    Unlock();

    if (!symbolName.empty()) {
        if (success) {
            char msg[192];
            sprintf_s(msg, "Depth unsubscribe confirmed for %s", symbolName.c_str());
            Utils::LogToFile("DEPTH_UNSUB_ACK", msg);
        } else {
            char msg[256];
            sprintf_s(msg, "Depth unsubscribe failed for %s: %s", symbolName.c_str(), error_details.c_str());
            Utils::LogToFile("DEPTH_UNSUB_ERROR", msg);
        }
    } else {
        Utils::LogToFile("DEPTH_UNSUB_ACK", "Unmatched depth unsubscribe response received");
    }
}

void ProcessRetries(long long ctidTraderAccountId) {
    Lock();

    std::vector<std::string> toRetry;
    std::vector<std::pair<std::string, int>> depthRetry;
    int64_t now = (int64_t)time(nullptr) * 1000;

    for (auto& kv : g_symbols) {
        if (kv.second.id <= 0) {
            kv.second.retry_count = MAX_RETRY_COUNT;
            kv.second.depthRetryCount = MAX_RETRY_COUNT;
            continue;
        }

        SymbolInfo& info = kv.second;

        bool waitingForAck = info.subscriptionPending;
        bool waitingForQuote = info.subscriptionAcked && !info.hasFirstQuote;
        bool active = info.subscribed && info.hasFirstQuote;

        if (!waitingForAck && !waitingForQuote && !active &&
            info.retry_count > 0 && info.retry_count < MAX_RETRY_COUNT) {
            int64_t delay = RETRY_DELAYS[info.retry_count - 1];
            if (now - info.last_retry_time >= delay) {
                toRetry.push_back(kv.first);
            }
        }

        if (!info.depthSubscribed &&
            info.depthRetryCount > 0 &&
            info.depthRetryCount < MAX_RETRY_COUNT) {

            int64_t delay = RETRY_DELAYS[info.depthRetryCount - 1];
            if (now - info.lastDepthRetryTime >= delay) {
                depthRetry.emplace_back(kv.first, info.requestedDepthLevels);
            }
        }
    }

    Unlock();

    for (const auto& name : toRetry) {
        EnsureSubscribed(ctidTraderAccountId, name);
    }

    for (const auto& item : depthRetry) {
        EnsureDepthSubscribed(ctidTraderAccountId, item.first, item.second > 0 ? item.second : DEFAULT_DEPTH_LEVELS);
    }

    LogSubscriptionStats();
}

enum class ResubReason {
    PendingTimeout,
    AckNoQuote,
    Quiet
};

void CheckStalledSubscriptions(long long ctidTraderAccountId) {
    ULONGLONG now = GetTickCount64();
    std::vector<std::pair<std::string, ResubReason>> resubscribe;

    Lock();
    for (auto& kv : g_symbols) {
        SymbolInfo& info = kv.second;
        if (info.id <= 0) {
            continue;
        }

        if (info.subscriptionPending) {
            ULONGLONG elapsed = now - info.lastSubscribeSentMs;
            if (elapsed >= SUB_PENDING_TIMEOUT_MS) {
                if (info.retry_count >= MAX_RETRY_COUNT) {
                    g_subStats.totalFailures++;
                    info.failureLogged = true;
                } else {
                    resubscribe.emplace_back(kv.first, ResubReason::PendingTimeout);
                    g_subStats.forcedResubscribe++;
                }
                info.subscriptionPending = false;
                info.subscriptionAcked = false;
                info.subscribed = false;
                info.hasFirstQuote = false;
                info.last_retry_time = 0;
                if (info.retry_count < MAX_RETRY_COUNT) {
                    info.failureLogged = false;
                }
            }
        } else if (info.subscriptionAcked && !info.hasFirstQuote) {
            ULONGLONG elapsed = now - info.lastAckMs;
            if (elapsed >= SUB_FIRST_QUOTE_TIMEOUT_MS) {
                if (info.retry_count >= MAX_RETRY_COUNT) {
                    g_subStats.totalFailures++;
                    info.failureLogged = true;
                } else {
                    resubscribe.emplace_back(kv.first, ResubReason::AckNoQuote);
                    g_subStats.forcedResubscribe++;
                }
                info.subscriptionAcked = false;
                info.subscribed = false;
                info.hasFirstQuote = false;
                info.last_retry_time = 0;
                if (info.retry_count < MAX_RETRY_COUNT) {
                    info.failureLogged = false;
                }
            }
        } else if (info.subscribed && info.hasFirstQuote) {
            ULONGLONG elapsed = now - info.lastQuoteMs;
            if (elapsed >= SUB_QUIET_TIMEOUT_MS) {
                if (info.retry_count >= MAX_RETRY_COUNT) {
                    g_subStats.totalFailures++;
                    info.failureLogged = true;
                } else {
                    resubscribe.emplace_back(kv.first, ResubReason::Quiet);
                    g_subStats.quietResubscribe++;
                }
                info.subscriptionPending = false;
                info.subscriptionAcked = false;
                info.subscribed = false;
                info.hasFirstQuote = false;
                info.last_retry_time = 0;
                if (info.retry_count < MAX_RETRY_COUNT) {
                    info.failureLogged = false;
                }
            }
        }
    }
    Unlock();

    for (const auto& item : resubscribe) {
        char msg[256];
        switch (item.second) {
            case ResubReason::PendingTimeout:
                sprintf_s(msg, "Resubscribing %s after pending timeout", item.first.c_str());
                Utils::LogToFile("SPOT_SUB_TIMEOUT", msg);
                break;
            case ResubReason::AckNoQuote:
                sprintf_s(msg, "Resubscribing %s after ACK without quote", item.first.c_str());
                Utils::LogToFile("SPOT_SUB_ACK_TIMEOUT", msg);
                break;
            case ResubReason::Quiet:
                sprintf_s(msg, "Resubscribing quiet symbol %s", item.first.c_str());
                Utils::LogToFile("SPOT_SUB_QUIET", msg);
                break;
        }

        EnsureSubscribed(ctidTraderAccountId, item.first);
        Sleep(5);
    }
}

void LogSubscriptionStats(bool force) {
    ULONGLONG now = GetTickCount64();

    Lock();
    if (!force && now - g_subStats.lastLogMs < SUB_STATS_INTERVAL_MS) {
        Unlock();
        return;
    }

    size_t totalSymbols = 0;
    size_t activeSymbols = 0;
    size_t pendingSymbols = 0;
    size_t ackNoQuote = 0;

    for (const auto& kv : g_symbols) {
        const SymbolInfo& info = kv.second;
        if (info.id <= 0) {
            continue;
        }

        totalSymbols++;

        if (info.subscriptionPending) {
            pendingSymbols++;
        } else if (info.subscriptionAcked && !info.hasFirstQuote) {
            ackNoQuote++;
        } else if (info.subscribed && info.hasFirstQuote) {
            activeSymbols++;
        }
    }

    g_subStats.lastLogMs = now;
    SubStats snapshot = g_subStats;
    Unlock();

    char msg[512];
    sprintf_s(msg,
              "sent=%llu ack=%llu first=%llu retry=%llu fail=%llu forced=%llu quiet=%llu active=%zu pending=%zu ackNoQuote=%zu total=%zu",
              static_cast<unsigned long long>(snapshot.totalSent),
              static_cast<unsigned long long>(snapshot.totalAcked),
              static_cast<unsigned long long>(snapshot.totalFirstQuote),
              static_cast<unsigned long long>(snapshot.totalRetries),
              static_cast<unsigned long long>(snapshot.totalFailures),
              static_cast<unsigned long long>(snapshot.forcedResubscribe),
              static_cast<unsigned long long>(snapshot.quietResubscribe),
              activeSymbols,
              pendingSymbols,
              ackNoQuote,
              totalSymbols);
    Utils::LogToFile("SUB_STATS", msg);
}

std::string BuildSubscribeSpotsReq(long long ctidTraderAccountId, long long symbolId, const std::string& symName, const std::string& clientMsgId) {
    log_spot_payload_override_once();
    char buf[1024];
    sprintf_s(buf, "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld]}}",
              clientMsgId.c_str(), CTRADER_SPOT_SUBSCRIBE_REQ, ctidTraderAccountId, symbolId);

    std::string logMsg = "Subscribing to spot prices for " + symName + " (ID: " + std::to_string(symbolId) +
                         ") using payloadType " + std::to_string(CTRADER_SPOT_SUBSCRIBE_REQ);
    Utils::LogToFile("SPOT_SUBSCRIBE", logMsg.c_str());
    return std::string(buf);
}

std::string BuildUnsubscribeSpotsReq(long long ctidTraderAccountId, long long symbolId, const std::string& symName) {
    char buf[512];
    sprintf_s(buf, "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld]}}",
              get_msg_id(), ToInt(PayloadType::SpotUnsubscribeReq), ctidTraderAccountId, symbolId);

    std::string logMsg = "Unsubscribing from spot prices for " + symName + " (ID: " + std::to_string(symbolId) +
                         ") using payloadType " + std::to_string(ToInt(PayloadType::SpotUnsubscribeReq));
    Utils::LogToFile("SPOT_UNSUBSCRIBE", logMsg.c_str());
    return std::string(buf);
}

std::string BuildSubscribeDepthReq(long long ctidTraderAccountId, long long symbolId, const std::string& symName, int depthLevels) {
    if (depthLevels <= 0) {
        depthLevels = DEFAULT_DEPTH_LEVELS;
    }

    char buf[1024];
    sprintf_s(buf, "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld],\"depth\":%d}}",
              get_msg_id(), ToInt(PayloadType::SubscribeDepthQuotesReq), ctidTraderAccountId, symbolId, depthLevels);

    std::string logMsg = "Subscribing to depth quotes for " + symName + " (ID: " + std::to_string(symbolId) + ", depth=" + std::to_string(depthLevels) + ") using payloadType " + std::to_string(ToInt(PayloadType::SubscribeDepthQuotesReq));
    Utils::LogToFile("DEPTH_SUBSCRIBE", logMsg.c_str());
    return std::string(buf);
}

std::string BuildUnsubscribeDepthReq(long long ctidTraderAccountId, long long symbolId, const std::string& symName, const std::string& clientMsgId) {
    char buf[512];
    sprintf_s(buf, "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld]}}",
              clientMsgId.c_str(), ToInt(PayloadType::UnsubscribeDepthQuotesReq), ctidTraderAccountId, symbolId);

    std::string logMsg = "Unsubscribing from depth quotes for " + symName + " (ID: " + std::to_string(symbolId) + ") using payloadType " + std::to_string(ToInt(PayloadType::UnsubscribeDepthQuotesReq));
    Utils::LogToFile("DEPTH_UNSUBSCRIBE", logMsg.c_str());
    return std::string(buf);
}

bool UnsubscribeDepth(long long ctidTraderAccountId, const std::string& symbolName) {
    if (ctidTraderAccountId == 0) {
        return false;
    }

    long long symbolId = 0;

    {
        Lock();
        auto it = g_symbols.find(symbolName);
        if (it == g_symbols.end() || !it->second.depthSubscribed || it->second.id <= 0) {
            Unlock();
            return false;
        }
        symbolId = it->second.id;
        Unlock();
    }

    std::string clientMsgId = Utils::GetMsgId();
    std::string request = BuildUnsubscribeDepthReq(ctidTraderAccountId, symbolId, symbolName, clientMsgId);

    Lock();
    g_pendingDepthUnsubscribes[clientMsgId] = symbolName;
    Unlock();

    if (!tcp_send(request.c_str())) {
        Lock();
        g_pendingDepthUnsubscribes.erase(clientMsgId);
        Unlock();

        char failMsg[256];
        sprintf_s(failMsg, "Depth unsubscribe send failed for %s (ID: %lld)", symbolName.c_str(), symbolId);
        Utils::LogToFile("DEPTH_UNSUB_REQ_FAIL", failMsg);
        return false;
    }

    return true;
}

void SubscribeToSpotPrices(long long ctidTraderAccountId) {
    log_spot_payload_override_once();
    std::vector<std::string> symbolsToSubscribe;
    int skippedWithoutId = 0;

    // Priority symbols (major forex pairs + metals + indices + commodities)
    // These are typically tradeable 24/5 and generate enough activity to keep connection alive
    std::set<std::string> prioritySymbols = {
        // Major Forex Pairs (28 symbols - most liquid)
        "EURUSD", "GBPUSD", "USDJPY", "USDCHF", "AUDUSD", "USDCAD", "NZDUSD",
        "EURGBP", "EURJPY", "GBPJPY", "AUDJPY", "EURAUD", "EURCHF", "GBPAUD",
        "GBPCAD", "GBPCHF", "CADJPY", "AUDCAD", "AUDCHF", "AUDNZD", "NZDJPY",
        "NZDCAD", "NZDCHF", "EURCAD", "EURNZD", "CADCHF",
        // Metals (4 symbols)
        "XAUUSD", "XAGUSD", "XPTUSD", "XPDUSD",
        // Major Indices (8 symbols)
        "US500", "USTEC", "US30", "DE40", "UK100", "JP225", "HK50", "AUS200",
        // Commodities (4 symbols)
        "WTI", "BRENT", "XNGUSD"
    };

    Lock();
    for (auto& kv : g_symbols) {
        SymbolInfo& sym = kv.second;
        if (sym.id <= 0) {
            skippedWithoutId++;
            continue;
        }

        sym.subscriptionPending = false;
        sym.subscriptionAcked = false;
        sym.subscribed = false;
        sym.hasFirstQuote = false;
        sym.retry_count = 0;
        sym.last_retry_time = 0;
        sym.lastSubscribeSentMs = 0;
        sym.lastAckMs = 0;
        sym.lastQuoteMs = 0;
        sym.failureLogged = false;

        // Only subscribe to priority symbols (reduces data traffic by ~70%)
        if (prioritySymbols.find(kv.first) != prioritySymbols.end()) {
            symbolsToSubscribe.push_back(kv.first);
        }
    }
    Unlock();

    size_t subscriptionCount = 0;

    for (const auto& name : symbolsToSubscribe) {
        EnsureSubscribed(ctidTraderAccountId, name);
        subscriptionCount++;

        if (subscriptionCount % SUB_BATCH_SIZE == 0) {
            Utils::LogToFile("THROTTLE", "Pausing 50ms after subscription batch to prevent overload");
            Sleep(50);  // Increased delay to avoid spam
        }
    }

    char summaryMsg[160];
    sprintf_s(summaryMsg, "Queued %zu spot price subscriptions (payloadType %d)",
              subscriptionCount, CTRADER_SPOT_SUBSCRIBE_REQ);
    Utils::LogToFile("SPOT_SUB_SUMMARY", summaryMsg);

    if (skippedWithoutId > 0) {
        char skipSummary[160];
        sprintf_s(skipSummary, "Skipped %d symbols without valid IDs during spot subscription", skippedWithoutId);
        Utils::LogToFile("SPOT_SUB_SKIP", skipSummary);
    }
}

bool UnsubscribeFromSpotPrice(long long ctidTraderAccountId, long long symbolId) {
    std::string symbolName;

    Lock();
    auto it = g_symbolById.find(symbolId);
    if (it != g_symbolById.end()) {
        symbolName = it->second;
        auto sit = g_symbols.find(symbolName);
        if (sit != g_symbols.end()) {
            SymbolInfo& info = sit->second;
            info.subscribed = false;
            info.subscriptionPending = false;
            info.subscriptionAcked = false;
            info.hasFirstQuote = false;
        }
    }
    Unlock();

    if (symbolName.empty()) {
        return false;
    }

    std::string request = BuildUnsubscribeSpotsReq(ctidTraderAccountId, symbolId, symbolName);
    if (!Network::Send(request.c_str())) {
        Utils::LogToFile("SPOT_UNSUBSCRIBE_FAIL", symbolName.c_str());
        return false;
    }

    return true;
}

void UpdateDepth(long long symbolId, const std::vector<DepthLevel>& bids, const std::vector<DepthLevel>& asks, uint64_t receivedMs, bool isDelta) {
    Lock();

    auto idIt = g_symbolById.find(symbolId);
    if (idIt == g_symbolById.end()) {
        Unlock();
        return;
    }

    auto it = g_symbols.find(idIt->second);
    if (it == g_symbols.end()) {
        Unlock();
        return;
    }

    SymbolInfo& info = it->second;
    int maxLevels = info.requestedDepthLevels > 0 ? info.requestedDepthLevels : DEFAULT_DEPTH_LEVELS;

    if (isDelta) {
        if (!bids.empty()) {
            apply_depth_delta(info.bidDepth, bids, true, maxLevels);
        }
        if (!asks.empty()) {
            apply_depth_delta(info.askDepth, asks, false, maxLevels);
        }
    } else {
        if (!bids.empty()) {
            info.bidDepth = bids;
            std::sort(info.bidDepth.begin(), info.bidDepth.end(), [](const DepthLevel& a, const DepthLevel& b) {
                return a.price > b.price;
            });
        }
        if (!asks.empty()) {
            info.askDepth = asks;
            std::sort(info.askDepth.begin(), info.askDepth.end(), [](const DepthLevel& a, const DepthLevel& b) {
                return a.price < b.price;
            });
        }

        if (maxLevels > 0) {
            if (static_cast<int>(info.bidDepth.size()) > maxLevels) {
                info.bidDepth.resize(static_cast<size_t>(maxLevels));
            }
            if (static_cast<int>(info.askDepth.size()) > maxLevels) {
                info.askDepth.resize(static_cast<size_t>(maxLevels));
            }
        }
    }

    info.depthSubscribed = true;
    info.depthRetryCount = 0;
    info.lastDepthUpdateMs = receivedMs;

    Unlock();
}

bool RequestAssetClassList(long long ctidTraderAccountId) {
    char request[256];

    if (ctidTraderAccountId > 0) {
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":2153,\"payload\":{\"ctidTraderAccountId\":%lld}}",
            Utils::GetMsgId(), ctidTraderAccountId);
    } else {
        sprintf_s(request, "{\"clientMsgId\":\"%s\",\"payloadType\":2153,\"payload\":{}}", Utils::GetMsgId());
        Utils::LogToFile("ASSET_CLASS_REQ_WARN", "ctidTraderAccountId missing, sending minimal payload");
    }

    if (!Network::Send(request)) {
        Utils::LogToFile("ASSET_CLASS_REQ", "Failed to send 2153 request");
        return false;
    }

    Utils::LogToFile("ASSET_CLASS_REQ", "2153 asset class request dispatched");
    return true;
}

bool RequestSymbolCategories(long long ctidTraderAccountId) {
    char request[512];

    if (ctidTraderAccountId > 0) {
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":2160,\"payload\":{\"ctidTraderAccountId\":%lld}}",
            Utils::GetMsgId(), ctidTraderAccountId);
    } else {
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":2160,\"payload\":{}}",
            Utils::GetMsgId());
    }

    if (!Network::Send(request)) {
        Utils::LogToFile("SYMBOL_CATEGORY_REQ", "Failed to send 2160 request");
        return false;
    }

    Utils::LogToFile("SYMBOL_CATEGORY_REQ", "2160 symbol category request dispatched");
    return true;
}

void HandleAssetClassListResponse(const char* buffer) {
    if (!buffer) return;

    const char* anchor = strstr(buffer, "\"assetClass\"");
    if (!anchor) {
        Utils::LogToFile("ASSET_CLASS_PARSE", "No asset class array found");
        return;
    }

    const char* open = strchr(anchor, '[');
    if (!open) return;

    const char* cursor = open + 1;
    std::vector<std::pair<long long, std::string>> parsed;

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

            long long assetClassId = 0;
            std::string name;
            extract_long(objectStart, walker, "\"assetClassId\":", assetClassId);
            if (assetClassId == 0) {
                extract_long(objectStart, walker, "\"id\":", assetClassId);
            }
            extract_string(objectStart, walker, "\"name\":\"", name);
            if (name.empty()) {
                extract_string(objectStart, walker, "\"assetClassName\":\"", name);
            }

            if (assetClassId > 0 && !name.empty()) {
                parsed.emplace_back(assetClassId, name);
            }

            cursor = walker;
        } else if (*cursor == ']') {
            break;
        } else {
            cursor++;
        }
    }

    if (parsed.empty()) {
        Utils::LogToFile("ASSET_CLASS_PARSE", "No asset class entries parsed");
        return;
    }

    Lock();
    for (const auto& kv : parsed) {
        g_assetClassNames[kv.first] = kv.second;
    }
    for (auto& sym : g_symbols) {
        if (sym.second.assetClassId > 0) {
            auto itClass = g_assetClassNames.find(sym.second.assetClassId);
            if (itClass != g_assetClassNames.end()) {
                sym.second.assetClassName = itClass->second;
            }
        }
    }
    Unlock();

    char logMsg[128];
    sprintf_s(logMsg, "Parsed %zu asset classes", parsed.size());
    Utils::LogToFile("ASSET_CLASS_PARSE", logMsg);
}

void HandleSymbolCategoryResponse(const char* buffer) {
    if (!buffer) return;

    const char* anchor = strstr(buffer, "\"symbolCategory\"");
    if (!anchor) {
        Utils::LogToFile("SYMBOL_CATEGORY_PARSE", "No symbol category array found");
        return;
    }

    const char* open = strchr(anchor, '[');
    if (!open) return;

    const char* cursor = open + 1;

    struct CategoryUpdate {
        long long symbolId;
        long long assetClassId;
        long long categoryId;
        std::string categoryName;
    };

    std::vector<CategoryUpdate> updates;

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

            CategoryUpdate update{0, 0, 0, {}};
            extract_long(objectStart, walker, "\"symbolId\":", update.symbolId);
            extract_long(objectStart, walker, "\"assetClassId\":", update.assetClassId);
            extract_long(objectStart, walker, "\"categoryId\":", update.categoryId);
            if (update.categoryId == 0) {
                extract_long(objectStart, walker, "\"symbolCategoryId\":", update.categoryId);
            }
            extract_string(objectStart, walker, "\"categoryName\":\"", update.categoryName);
            if (update.categoryName.empty()) {
                extract_string(objectStart, walker, "\"name\":\"", update.categoryName);
            }

            if (update.symbolId > 0) {
                updates.push_back(update);
            }

            cursor = walker;
        } else if (*cursor == ']') {
            break;
        } else {
            cursor++;
        }
    }

    if (updates.empty()) {
        Utils::LogToFile("SYMBOL_CATEGORY_PARSE", "No symbol category entries parsed");
        return;
    }

    Lock();
    for (const auto& upd : updates) {
        auto idIt = g_symbolById.find(upd.symbolId);
        if (idIt == g_symbolById.end()) continue;

        auto symIt = g_symbols.find(idIt->second);
        if (symIt == g_symbols.end()) continue;

        symIt->second.symbolCategoryId = upd.categoryId;
        symIt->second.categoryName = upd.categoryName;
        if (upd.assetClassId > 0) {
            symIt->second.assetClassId = upd.assetClassId;
            auto classIt = g_assetClassNames.find(upd.assetClassId);
            if (classIt != g_assetClassNames.end()) {
                symIt->second.assetClassName = classIt->second;
            }
        }
    }
    Unlock();

    char logMsg[160];
    sprintf_s(logMsg, "Parsed %zu symbol categories", updates.size());
    Utils::LogToFile("SYMBOL_CATEGORY_PARSE", logMsg);
}

void GenerateBrokerAssetsFile(const char* dllPath) {
    Lock();
    sync_asset_catalog_from_symbols_locked(true);
    Unlock();

    write_asset_list_file(dllPath, "SYMBOL_REFRESH");
}

bool FetchAssetList(long long ctidTraderAccountId, const char* dllPath) {
    if (ctidTraderAccountId <= 0) {
        Utils::LogToFile("ASSET_LIST", "FetchAssetList skipped - invalid accountId");
        return false;
    }

    char request[512] = {0};
    char response[262144] = {0};

    sprintf_s(request,
              "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"ctidTraderAccountId\":%lld}}",
              Utils::GetMsgId(),
              ToInt(PayloadType::AssetListReq),
              ctidTraderAccountId);

    if (!Network::Send(request)) {
        Utils::LogToFile("ASSET_LIST", "Asset list request send failed (PROTO_OA_ASSET_LIST_REQ)");
        return false;
    }

    int received = Network::Receive(response, sizeof(response));
    if (received <= 0) {
        Utils::LogToFile("ASSET_LIST", "Asset list response receive failed (PROTO_OA_ASSET_LIST_RES)");
        return false;
    }

    Utils::LogToFile("ASSET_LIST_RESPONSE", response);

    if (!Utils::ContainsPayloadType(response, PayloadType::AssetListRes)) {
        Utils::LogToFile("ASSET_LIST", "Unexpected payload type (expected PROTO_OA_ASSET_LIST_RES)");
        return false;
    }

    std::vector<AssetCatalogEntry> parsed;
    if (!parse_asset_list_payload(response, parsed)) {
        Utils::LogToFile("ASSET_LIST", "Failed to parse asset list payload");
        return false;
    }

    Lock();
    g_assetCatalog.clear();
    g_assetIdToKey.clear();

    for (const auto& entry : parsed) {
        std::string key;
        if (!entry.displayName.empty()) {
            key = NormalizeSymbol(entry.displayName.c_str());
        }
        if (key.empty() && entry.assetId > 0) {
            key = "ASSET_" + std::to_string(entry.assetId);
        }
        if (key.empty()) {
            continue;
        }

        update_asset_catalog_entry_locked(key,
                                          entry.displayName,
                                          entry.assetId,
                                          entry.symbolId,
                                          entry.digits,
                                          entry.pipPosition,
                                          entry.pipSize,
                                          true);
    }

    for (const auto& entry : parsed) {
        if (entry.assetId <= 0) {
            continue;
        }
        for (auto& symPair : g_symbols) {
            SymbolInfo& symInfo = symPair.second;
            if (symInfo.assetId == entry.assetId) {
                if (entry.digits > 0 && symInfo.digits <= 0) {
                    symInfo.digits = entry.digits;
                }
                if (entry.symbolId > 0 && symInfo.id <= 0) {
                    symInfo.id = entry.symbolId;
                    g_symbolById[entry.symbolId] = symPair.first;
                }
            }
        }
    }

    // Merge existing symbol metadata back into the catalog so spot subscriptions use live IDs
    sync_asset_catalog_from_symbols_locked(true);
    Unlock();

    char msg[128];

    sprintf_s(msg, "Fetched asset list (%zu entries)", parsed.size());
    Utils::LogToFile("ASSET_LIST", msg);

    if (dllPath) {
        write_asset_list_file(dllPath, "ASSET_FETCH");
    }

    return true;
}

bool RequestSymbolDetails(long long ctidTraderAccountId, const std::vector<long long>& symbolIds) {
    if (symbolIds.empty()) {
        Utils::LogToFile("SYMBOL_DETAILS_REQ", "No symbols to request details for");
        return false;
    }

    // Batch symbols to avoid JSON size limit (max 4096 bytes)
    constexpr size_t MAX_BATCH_SIZE = 50;

    for (size_t i = 0; i < symbolIds.size(); i += MAX_BATCH_SIZE) {
        size_t end = std::min(i + MAX_BATCH_SIZE, symbolIds.size());

        // Build symbolId array: [id1,id2,id3,...]
        std::string idArray;
        for (size_t j = i; j < end; ++j) {
            if (j > i) idArray += ",";
            char buf[32];
            sprintf_s(buf, "%lld", symbolIds[j]);
            idArray += buf;
        }

        char request[2048];
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":2116,\"payload\":{"
            "\"ctidTraderAccountId\":%lld,\"symbolId\":[%s]}}",
            Utils::GetMsgId(), ctidTraderAccountId, idArray.c_str());

        if (!Network::Send(request)) {
            char msg[256];
            sprintf_s(msg, "Failed to send PROTO_OA_SYMBOL_BY_ID_REQ for batch %zu-%zu", i, end);
            Utils::LogToFile("SYMBOL_DETAILS_REQ", msg);
            return false;
        }

        char msg[256];
        sprintf_s(msg, "Sent PROTO_OA_SYMBOL_BY_ID_REQ (2116) for %zu symbols", end - i);
        Utils::LogToFile("SYMBOL_DETAILS_REQ", msg);
    }

    return true;
}

void HandleSymbolByIdResponse(const char* buffer) {
    if (!buffer) {
        Utils::LogToFile("SYMBOL_BY_ID_RES", "ERROR: buffer is NULL");
        return;
    }

    const char* payloadStart = strstr(buffer, "\"payload\":");
    if (!payloadStart) {
        Utils::LogToFile("SYMBOL_BY_ID_RES", "No payload found");
        return;
    }

    const char* symbolArray = strstr(payloadStart, "\"symbol\":[");
    if (!symbolArray) {
        Utils::LogToFile("SYMBOL_BY_ID_RES", "No symbol array found");
        return;
    }

    const char* cursor = strchr(symbolArray, '[');
    if (!cursor) return;
    cursor++;

    int updateCount = 0;

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

            // Parse all symbol parameters from PROTO_OA_SYMBOL_BY_ID_RES (2117)
            long long symbolId = 0;
            int64_t lotSize = 0, minVolume = 0, maxVolume = 0, stepVolume = 0, maxExposure = 0;
            int64_t commission = 0;
            int commissionType = 0, digits = 0, pipPosition = 0, swapRollover3Days = 0;
            double swapLong = 0.0, swapShort = 0.0;
            bool enableShortSelling = true, guaranteedStopLoss = false;

            const char* pSymId = strstr(objectStart, "\"symbolId\":");
            if (pSymId && pSymId < walker) {
                sscanf_s(pSymId, "\"symbolId\":%lld", &symbolId);
            }

            const char* pDigits = strstr(objectStart, "\"digits\":");
            if (pDigits && pDigits < walker) {
                sscanf_s(pDigits, "\"digits\":%d", &digits);
            }

            const char* pPipPos = strstr(objectStart, "\"pipPosition\":");
            if (pPipPos && pPipPos < walker) {
                sscanf_s(pPipPos, "\"pipPosition\":%d", &pipPosition);
            }

            const char* pLotSize = strstr(objectStart, "\"lotSize\":");
            if (pLotSize && pLotSize < walker) {
                sscanf_s(pLotSize, "\"lotSize\":%lld", &lotSize);
            }

            const char* pMinVol = strstr(objectStart, "\"minVolume\":");
            if (pMinVol && pMinVol < walker) {
                sscanf_s(pMinVol, "\"minVolume\":%lld", &minVolume);
            }

            const char* pMaxVol = strstr(objectStart, "\"maxVolume\":");
            if (pMaxVol && pMaxVol < walker) {
                sscanf_s(pMaxVol, "\"maxVolume\":%lld", &maxVolume);
            }

            const char* pStepVol = strstr(objectStart, "\"stepVolume\":");
            if (pStepVol && pStepVol < walker) {
                sscanf_s(pStepVol, "\"stepVolume\":%lld", &stepVolume);
            }

            const char* pMaxExp = strstr(objectStart, "\"maxExposure\":");
            if (pMaxExp && pMaxExp < walker) {
                sscanf_s(pMaxExp, "\"maxExposure\":%lld", &maxExposure);
            }

            const char* pComm = strstr(objectStart, "\"commission\":");
            if (pComm && pComm < walker) {
                sscanf_s(pComm, "\"commission\":%lld", &commission);
            }

            const char* pCommType = strstr(objectStart, "\"commissionType\":");
            if (pCommType && pCommType < walker) {
                sscanf_s(pCommType, "\"commissionType\":%d", &commissionType);
            }

            const char* pSwapLong = strstr(objectStart, "\"swapLong\":");
            if (pSwapLong && pSwapLong < walker) {
                sscanf_s(pSwapLong, "\"swapLong\":%lf", &swapLong);
            }

            const char* pSwapShort = strstr(objectStart, "\"swapShort\":");
            if (pSwapShort && pSwapShort < walker) {
                sscanf_s(pSwapShort, "\"swapShort\":%lf", &swapShort);
            }

            const char* pSwapRoll = strstr(objectStart, "\"swapRollover3Days\":");
            if (pSwapRoll && pSwapRoll < walker) {
                sscanf_s(pSwapRoll, "\"swapRollover3Days\":%d", &swapRollover3Days);
            }

            const char* pShortSell = strstr(objectStart, "\"enableShortSelling\":");
            if (pShortSell && pShortSell < walker) {
                enableShortSelling = (strstr(pShortSell, "true") != nullptr);
            }

            const char* pGSL = strstr(objectStart, "\"guaranteedStopLoss\":");
            if (pGSL && pGSL < walker) {
                guaranteedStopLoss = (strstr(pGSL, "true") != nullptr);
            }

            // Update SymbolInfo
            if (symbolId > 0) {
                Lock();
                bool found = false;
                for (auto& kv : g_symbols) {
                    if (kv.second.id == symbolId) {
                        found = true;
                        if (digits > 0) kv.second.digits = digits;
                        if (pipPosition > 0) kv.second.pipPosition = pipPosition;
                        kv.second.lotSize = lotSize;
                        kv.second.minVolume = minVolume;
                        kv.second.maxVolume = maxVolume;
                        kv.second.stepVolume = stepVolume;
                        kv.second.maxExposure = maxExposure;
                        kv.second.commission = commission;
                        kv.second.commissionType = commissionType;
                        kv.second.swapLong = swapLong;
                        kv.second.swapShort = swapShort;
                        kv.second.swapRollover3Days = swapRollover3Days;
                        kv.second.enableShortSelling = enableShortSelling;
                        kv.second.guaranteedStopLoss = guaranteedStopLoss;

                        char msg[512];
                        sprintf_s(msg, "Updated %s: lot=%lld, min=%lld, max=%lld, step=%lld, comm=%lld, swap=[%.2f,%.2f]",
                                  kv.first.c_str(), lotSize, minVolume, maxVolume, stepVolume, commission, swapLong, swapShort);
                        Utils::LogToFile("VOLUME_LIMITS", msg);
                        updateCount++;
                        break;
                    }
                }
                Unlock();

                if (!found) {
                    char msg[256];
                    sprintf_s(msg, "WARNING: Received volume data for unknown symbolId=%lld", symbolId);
                    Utils::LogToFile("SYMBOL_BY_ID_RES", msg);
                }
            }

            cursor = walker;
        } else if (*cursor == ']') {
            break;
        } else {
            cursor++;
        }
    }

    char summary[256];
    sprintf_s(summary, "PROTO_OA_SYMBOL_BY_ID_RES (2117) processed: updated %d symbols", updateCount);
    Utils::LogToFile("SYMBOL_BY_ID_RES", summary);
}

bool Symbols::LoadAssetCache(const char* dllPath) {
    char assetListPath[MAX_PATH];
    sprintf_s(assetListPath, "%sAssetList.txt", dllPath);

    std::ifstream inFile(assetListPath);
    if (!inFile.is_open()) {
        log_to_wesocket("ASSET_CACHE", "AssetList.txt not found - cache miss");
        return false;
    }

    std::string line;
    int loadedWithIds = 0;
    int skippedWithoutId = 0;
    std::map<std::string, SymbolInfo> temp;

    while (std::getline(inFile, line)) {
        if (line.empty()) continue;

        // Parse extended format: Name;SymbolId;Digits;Source;LotSize;MinVolume;MaxVolume;StepVolume;MaxExposure;Commission;CommissionType;SwapLong;SwapShort;SwapRollover3Days;EnableShortSelling;GuaranteedStopLoss;PipPosition
        // Backwards compatible with old format: Name;Id;Digits;Source;LotSize;MinVolume;MaxVolume

        std::vector<std::string> fields;
        size_t pos = 0, next = 0;
        while ((next = line.find(';', pos)) != std::string::npos) {
            fields.push_back(Utils::Trim(line.substr(pos, next - pos)));
            pos = next + 1;
        }
        fields.push_back(Utils::Trim(line.substr(pos)));  // last field

        if (fields.size() < 3) {
            skippedWithoutId++;
            continue;
        }

        std::string name = fields[0];
        long long parsedId = fields.size() > 1 ? _strtoui64(fields[1].c_str(), nullptr, 10) : 0;
        int digits = fields.size() > 2 ? atoi(fields[2].c_str()) : 0;
        std::string sourceStr = fields.size() > 3 ? fields[3] : "";

        // Extended fields (backward compatible)
        int64_t lotSize = fields.size() > 4 ? _strtoui64(fields[4].c_str(), nullptr, 10) : 0;
        int64_t minVolume = fields.size() > 5 ? _strtoui64(fields[5].c_str(), nullptr, 10) : 0;
        int64_t maxVolume = fields.size() > 6 ? _strtoui64(fields[6].c_str(), nullptr, 10) : 0;
        int64_t stepVolume = fields.size() > 7 ? _strtoui64(fields[7].c_str(), nullptr, 10) : 0;
        int64_t maxExposure = fields.size() > 8 ? _strtoui64(fields[8].c_str(), nullptr, 10) : 0;
        int64_t commission = fields.size() > 9 ? _strtoui64(fields[9].c_str(), nullptr, 10) : 0;
        int commissionType = fields.size() > 10 ? atoi(fields[10].c_str()) : 0;
        double swapLong = fields.size() > 11 ? atof(fields[11].c_str()) : 0.0;
        double swapShort = fields.size() > 12 ? atof(fields[12].c_str()) : 0.0;
        int swapRollover3Days = fields.size() > 13 ? atoi(fields[13].c_str()) : 0;
        bool enableShortSelling = fields.size() > 14 ? (atoi(fields[14].c_str()) != 0) : true;
        bool guaranteedStopLoss = fields.size() > 15 ? (atoi(fields[15].c_str()) != 0) : false;
        int pipPosition = fields.size() > 16 ? atoi(fields[16].c_str()) : 0;

        bool entryHasSymbolId = true;
        if (!sourceStr.empty()) {
            std::string srcUpper = sourceStr;
            std::transform(srcUpper.begin(), srcUpper.end(), srcUpper.begin(), ::toupper);
            if (srcUpper == "ASSET" || srcUpper == "UNKNOWN") {
                entryHasSymbolId = false;
            }
        }

        if (parsedId == 0) {
            entryHasSymbolId = false;
        }

        if (name.empty()) {
            skippedWithoutId++;
            continue;
        }

        if (!entryHasSymbolId) {
            skippedWithoutId++;
            continue;
        }

        SymbolInfo si{};
        si.id = parsedId;
        si.digits = digits;
        si.pipPosition = pipPosition;
        si.lotSize = lotSize;
        si.minVolume = minVolume;
        si.maxVolume = maxVolume;
        si.stepVolume = stepVolume;
        si.maxExposure = maxExposure;
        si.commission = commission;
        si.commissionType = commissionType;
        si.swapLong = swapLong;
        si.swapShort = swapShort;
        si.swapRollover3Days = swapRollover3Days;
        si.enableShortSelling = enableShortSelling;
        si.guaranteedStopLoss = guaranteedStopLoss;
        si.subscribed = false;
        si.bid = 0;
        si.ask = 0;
        si.retry_count = 0;
        si.last_retry_time = 0;
        si.depthSubscribed = false;
        si.depthRetryCount = 0;
        si.lastDepthRetryTime = 0;
        si.requestedDepthLevels = DEFAULT_DEPTH_LEVELS;
        si.lastDepthUpdateMs = 0;
        temp[name] = si;
        loadedWithIds++;
    }
    inFile.close();

    if (loadedWithIds == 0) {
        log_to_wesocket("ASSET_CACHE", "AssetList.txt missing symbol IDs - falling back to live fetch");
        return false;
    }

    // Commit to global symbol map only if we have at least names
    Lock();
    g_symbols.clear();
    g_symbolById.clear();
    g_assetCatalog.clear();
    g_assetIdToKey.clear();
    for (auto& kv : temp) {
        g_symbols[kv.first] = kv.second;
        if (kv.second.id > 0) {
            g_symbolById[kv.second.id] = kv.first;
        }
    }
    // Asset cache load: only use what is in the cache, do not keep any old symbols/assets
    sync_asset_catalog_from_symbols_locked(true);
    Unlock();

    char msg[128];
    sprintf_s(msg, "Loaded %d symbols from cache (skipped %d without IDs)", loadedWithIds, skippedWithoutId);
    log_to_wesocket("ASSET_CACHE", msg);

    return skippedWithoutId == 0; // true only if IDs present allowing skip of fetch
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

