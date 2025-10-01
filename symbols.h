#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <windows.h> // For CRITICAL_SECTION

// Represents a single symbol's state
struct SymbolInfo {
    long long id;
    int digits;
    bool subscribed;
    long long bid;
    long long ask;
    // For retry logic
    int64_t retry_count;
    int64_t last_retry_time;
};

namespace Symbols {

    // Initializes the symbols module
    void Initialize();

    // Cleans up resources used by the symbols module
    void Cleanup();

    // Adds a symbol to the internal collections
    void AddSymbol(const std::string& name, long long id, int digits);

    // Ensures a symbol is subscribed. Returns false on error.
    bool EnsureSubscribed(long long ctidTraderAccountId, const std::string& symbolName);

    // Updates the quote for a given symbol ID
    void UpdateQuote(long long symbolId, long long bid, long long ask);

    // Resubscribes to all previously subscribed symbols after a reconnect
    void BatchResubscribe(long long ctidTraderAccountId);

    // Retrieves symbol information by name
    SymbolInfo* GetSymbol(const std::string& symbolName);
    
    // Handles an explicit subscription response from the server
    void HandleSubscriptionResponse(const std::string& clientMsgId, bool success, const std::string& error_details);

    // Iterates through symbols and retries any failed subscriptions
    void ProcessRetries(long long ctidTraderAccountId);

    // Generates the assetonly.csv file based on available symbols and AssetFix.csv
    void GenerateBrokerAssetsFile(const char* dllPath);

    // Public API for locking
    void Lock();
    void Unlock();

    // --- EXTENSION POINTS (for future implementation) ---
    // 1. To support order book data, a new subscription function could be added:
    //    `bool EnsureOrderBookSubscribed(const std::string& symbolName);`
    //    This would send a different payload type and store the depth data within SymbolInfo.
    
    // 2. To support unsubscribing, a function could be added:
    //    `void Unsubscribe(const std::string& symbolName);`
    //    This would send a payload to unsubscribe from quotes/depth and update the `subscribed` flag.

    // 3. To support lazy loading of symbol metadata (pip size, leverage, etc.):
    //    The `SymbolInfo` struct could be extended, and a function like:
    //    `void FetchSymbolMetadata(const std::string& symbolName);`
    //    could be called on-demand, caching the results.

} // namespace Symbols