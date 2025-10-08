#pragma once
#include "globals.h"

namespace Auth {
    bool LoadTokenFromDisk();
    bool SaveTokenToDisk(const std::string& access, const std::string& refresh);
    bool ParseJsonTokenFields(const std::string& body, std::string& access, std::string& refresh);
    bool BodyIndicatesError(const std::string& body, const char* extraTag = nullptr);

    bool RefreshAccessToken();
    bool FetchAccountsList(std::vector<long long>& accountIds);

    bool WinHttpGetToken(const std::wstring& host, const std::wstring& path,
                        const std::map<std::string, std::string>& params,
                        std::string& outBody);
}