#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../include/oauth_utils.h"
#include "../include/auth.h"
#include "../include/utils.h"
#include <sstream>

namespace OAuth {

bool ListenForCode(std::string& outCode, int port, DWORD timeoutMs) {
    outCode.clear();

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        Utils::ShowMsg("Failed to create listen socket");
        return false;
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons((u_short)port);

    if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        Utils::ShowMsg("Failed to bind to port 53123");
        return false;
    }

    if (listen(listenSocket, 1) == SOCKET_ERROR) {
        closesocket(listenSocket);
        Utils::ShowMsg("Failed to listen");
        return false;
    }

    DWORD timeout = timeoutMs;
    setsockopt(listenSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    Utils::ShowMsg("Waiting for OAuth callback...");

    SOCKET acceptSocket = accept(listenSocket, NULL, NULL);
    if (acceptSocket == INVALID_SOCKET) {
        closesocket(listenSocket);
        Utils::ShowMsg("Timeout waiting for callback");
        return false;
    }

    char buffer[4096] = {0};
    int bytesReceived = recv(acceptSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';

        const char* pCode = strstr(buffer, "code=");
        if (pCode) {
            pCode += 5;
            const char* pEnd = strstr(pCode, "&");
            if (!pEnd) pEnd = strstr(pCode, " ");
            if (!pEnd) pEnd = strstr(pCode, "\r");
            if (!pEnd) pEnd = strstr(pCode, "\n");

            if (pEnd) {
                outCode = std::string(pCode, pEnd - pCode);
            } else {
                outCode = std::string(pCode);
            }
        }

        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body>"
            "<h1>Authorization Successful!</h1>"
            "<p>You can close this window.</p>"
            "</body></html>";

        send(acceptSocket, response, (int)strlen(response), 0);
    }

    closesocket(acceptSocket);
    closesocket(listenSocket);

    return !outCode.empty();
}

bool ExchangeCodeForTokens(const std::string& code, std::string& accessOut, std::string& refreshOut) {
    std::map<std::string, std::string> params;
    params["grant_type"] = "authorization_code";
    params["code"] = code;
    params["redirect_uri"] = "http://127.0.0.1:53123/callback";
    params["client_id"] = G.ClientId;
    params["client_secret"] = G.ClientSecret;

    std::string responseBody;
    if (!Auth::WinHttpGetToken(L"openapi.ctrader.com", L"/apps/token", params, responseBody)) {
        Utils::ShowMsg("Token exchange failed");
        return false;
    }

    if (!Auth::ParseJsonTokenFields(responseBody, accessOut, refreshOut)) {
        Utils::ShowMsg("Parse failed");
        return false;
    }

    return !accessOut.empty();
}

bool PerformInteractiveFlow() {
    Utils::ShowMsg("=== OAuth2 Flow ===");

    std::ostringstream urlStream;
    urlStream << "https://openapi.ctrader.com/apps/auth";
    urlStream << "?client_id=" << G.ClientId;
    urlStream << "&redirect_uri=http://127.0.0.1:53123/callback";
    urlStream << "&scope=trading";

    std::string url = urlStream.str();
    Utils::ShowMsg("Opening browser...");

    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    std::string code;
    if (!ListenForCode(code, 53123, 60000)) {
        return false;
    }

    std::string accessToken, refreshToken;
    if (!ExchangeCodeForTokens(code, accessToken, refreshToken)) {
        return false;
    }

    strncpy_s(G.Token, sizeof(G.Token), accessToken.c_str(), _TRUNCATE);
    if (!refreshToken.empty()) {
        strncpy_s(G.RefreshToken, sizeof(G.RefreshToken), refreshToken.c_str(), _TRUNCATE);
    }

    Auth::SaveTokenToDisk(accessToken, refreshToken);
    Utils::ShowMsg("OAuth completed!");
    return true;
}

// Additional functions from grok2.txt integration

std::string UrlEncode(const std::string& str) {
    std::string encoded;
    char hex[4];

    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        }
        else {
            sprintf_s(hex, sizeof(hex), "%%%02X", c);
            encoded += hex;
        }
    }

    return encoded;
}

std::string ExtractAuthCode(const std::string& callbackUrl) {
    const char* pCode = strstr(callbackUrl.c_str(), "code=");
    if (!pCode) return "";

    pCode += 5; // Skip "code="
    const char* pEnd = strstr(pCode, "&");
    if (!pEnd) pEnd = strstr(pCode, " ");
    if (!pEnd) pEnd = strstr(pCode, "\r");
    if (!pEnd) pEnd = strstr(pCode, "\n");

    if (pEnd) {
        return std::string(pCode, pEnd - pCode);
    } else {
        return std::string(pCode);
    }
}

std::string GenerateAuthUrl(const std::string& clientId, const std::string& redirectUri, const std::string& scope) {
    std::string url = "https://openapi.ctrader.com/apps/auth";
    url += "?client_id=" + UrlEncode(clientId);
    url += "&redirect_uri=" + UrlEncode(redirectUri);
    url += "&scope=" + UrlEncode(scope);
    url += "&response_type=code";
    return url;
}

} // namespace OAuth

