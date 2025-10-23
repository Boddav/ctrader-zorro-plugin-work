#include "../stdafx.h"
#include "../include/network.h"
#include "../include/utils.h"

namespace Network {

// Static variables for error tracking
static std::string lastError;
static DWORD receiveTimeoutMs = 5000; // Default 5 second timeout

bool Connect(const char* host, const char* /*portStr*/) {
    // Close any previous handles
    Disconnect();

    // Convert host to wide string
    std::wstring whost;
    for (const char* p = host; *p; ++p) {
        whost.push_back((wchar_t)*p);
    }

    G.hSession = WinHttpOpen(L"Zorro-cTrader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!G.hSession) {
        Utils::ShowMsg("WinHTTP open failed");
        return false;
    }

    // Set timeouts to make WebSocket receive non-blocking
    if (!WinHttpSetTimeouts(G.hSession, 10000, 10000, 5000, 2000)) {
        Utils::ShowMsg("Failed to set WinHTTP timeouts");
    }

    G.hConnect = WinHttpConnect(G.hSession, whost.c_str(), CTRADER_WS_PORT, 0);
    if (!G.hConnect) {
        Utils::ShowMsg("WinHTTP connect failed");
        Disconnect();
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(G.hConnect, L"GET", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        Utils::ShowMsg("WinHTTP open request failed");
        Disconnect();
        return false;
    }

    // Upgrade to WebSocket: pass NULL and 0 as per WinHTTP docs
    if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) {
        DWORD dwErr = ::GetLastError();
        char emsg[128];
        sprintf_s(emsg, "WinHTTP set upgrade option failed (err=%lu)", dwErr);
        Utils::ShowMsg(emsg);
        lastError = emsg;
        WinHttpCloseHandle(hRequest);
        Disconnect();
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwErr = ::GetLastError();
        char emsg[128];
        sprintf_s(emsg, "WinHTTP send/receive failed (err=%lu)", dwErr);
        Utils::ShowMsg(emsg);
        lastError = emsg;
        WinHttpCloseHandle(hRequest);
        Disconnect();
        return false;
    }

    G.hWebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
    WinHttpCloseHandle(hRequest); // no longer needed

    if (!G.hWebSocket) {
        Utils::ShowMsg("WebSocket upgrade failed");
        Disconnect();
        return false;
    }

    G.wsConnected = true;
    G.lastPingMs = GetTickCount64();
    Utils::ShowMsg("WebSocket connected.");

    return true;
}

void Disconnect() {
    if (G.hWebSocket) {
        WinHttpWebSocketClose(G.hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
        WinHttpCloseHandle(G.hWebSocket);
        G.hWebSocket = NULL;
    }
    if (G.hConnect) {
        WinHttpCloseHandle(G.hConnect);
        G.hConnect = NULL;
    }
    if (G.hSession) {
        WinHttpCloseHandle(G.hSession);
        G.hSession = NULL;
    }
    G.wsConnected = false;
}

bool Send(const char* data) {
    if (!G.wsConnected || !G.hWebSocket) return false;

    size_t len = strlen(data);
    if (G.Diag >= 1) Utils::ShowMsg("WS SEND:", data);

    DWORD hr = WinHttpWebSocketSend(G.hWebSocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (void*)data, (DWORD)len);
    if (hr != S_OK) {
        Disconnect();
        return false;
    }
    return true;
}

int Receive(char* buffer, size_t size) {
    if (!G.wsConnected || !G.hWebSocket) return -1;

    std::string accum;
    accum.reserve(4096);

    DWORD dwRead = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;

    do {
        char tmp[4096];
        dwRead = 0;
        type = (WINHTTP_WEB_SOCKET_BUFFER_TYPE)0;

        HRESULT hr = WinHttpWebSocketReceive(G.hWebSocket, tmp, sizeof(tmp), &dwRead, &type);
        if (hr != S_OK) {
            char errMsg[256];
            sprintf_s(errMsg, "WinHttpWebSocketReceive failed with HRESULT: 0x%08X", hr);
            Utils::LogToFile("WS_RECEIVE_ERROR", errMsg);
            Disconnect();
            return -1;
        }

        if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            Utils::LogToFile("WS_CLOSE", "Received CLOSE_BUFFER_TYPE from server");
            Disconnect();
            return -1;
        }

        if (dwRead) accum.append(tmp, tmp + dwRead);

    } while (type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE ||
             type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE);

    if (accum.empty()) return 0;

    int copy = (int)std::min(accum.size(), (size_t)(size-1));
    memcpy(buffer, accum.data(), copy);
    buffer[copy] = '\0';

    if (G.Diag >= 1) Utils::ShowMsg("WS RECV:", buffer);

    return copy;
}

// Additional enhanced functions from grok2.txt

bool Connect(const char* host, int port) {
    char portStr[16];
    sprintf_s(portStr, "%d", port);
    return Connect(host, portStr);
}

bool IsConnected() {
    return G.wsConnected && G.hWebSocket != NULL;
}

std::string GetLastError() {
    return lastError;
}

void SetReceiveTimeout(DWORD timeoutMs) {
    receiveTimeoutMs = timeoutMs;
    Utils::LogToFile("NETWORK", ("Receive timeout set to " + std::to_string(timeoutMs) + "ms").c_str());
}

} // namespace Network

