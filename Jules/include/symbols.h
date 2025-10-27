#pragma once
#include <string>
#include <cstdint>

struct SymbolInfo {
    long long id;
    int digits;
    bool subscribed;
    long long bid;
    long long ask;
    int64_t retry_count;
    int64_t last_retry_time;
};

namespace Symbols {
    void Initialize();
    void Cleanup();
    void AddSymbol(const std::string& name, long long id, int digits);
    bool EnsureSubscribed(long long ctidTraderAccountId, const std::string& symbolName);
    void UpdateQuote(long long symbolId, long long bid, long long ask);
    void BatchResubscribe(long long ctidTraderAccountId);
    SymbolInfo* GetSymbol(const std::string& symbolName);
    void HandleSubscriptionResponse(const std::string& clientMsgId, bool success, const std::string& error_details);
    void ProcessRetries(long long ctidTraderAccountId);
    void GenerateBrokerAssetsFile(const char* dllPath);
    void Lock();
    void Unlock();

    // Symbol format normalization
    std::string NormalizeSymbol(const char* symbol);
    std::string AddSlashToSymbol(const char* symbol);
    SymbolInfo* GetSymbolByIdOrName(const char* symbol);
}