#pragma once
#include <string>

namespace HttpApi {

// Generic HTTP request function
std::string HttpRequest(const char* url, const char* data = nullptr, const char* headers = nullptr, const char* method = nullptr);

// HTTP method specific functions
std::string Get(const char* url, const char* headers = nullptr);
std::string Post(const char* url, const char* data, const char* headers = nullptr);
std::string Put(const char* url, const char* data, const char* headers = nullptr);
std::string Delete(const char* url, const char* headers = nullptr);

// cTrader REST API specific functions
std::string GetAccountInfo(long long accountId);
std::string GetSymbolInfo(const char* symbol);
std::string GetPositions(long long accountId);

} // namespace HttpApi