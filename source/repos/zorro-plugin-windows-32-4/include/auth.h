#pragma once
#include <string>
#include <vector>

namespace Auth {

// Full login flow: connect websocket, app auth, account auth
bool Login(const char* user, const char* pwd, const char* type);

// Application auth (clientId + clientSecret)
bool ApplicationAuth();

// Account auth (accessToken + accountId)
bool AccountAuth();

// Load/save tokens to disk
bool LoadToken();
void SaveToken();

// Parse accounts CSV for credentials
bool LoadAccountsCsv(const char* user, const char* pwd);

// OAuth2 browser flow (if no token saved)
bool OAuthBrowserFlow();

// Token refresh
bool RefreshAccessToken();

// Auto-detect accounts via GetAccountsByAccessTokenReq (2149)
bool FetchAccountsList(std::vector<long long>& accountIds);

// Detect environment from Zorro Type string
void DetectEnv(const char* type);

} // namespace Auth
