#include "../include/state.h"
#include "../include/websocket.h"
#include "../include/logger.h"
#include <cstdio>

namespace WebSocket {

bool Connect(const char* host, int port) {
    if (!host) return false;

    CsLock lock(G.csWebSocket);  // Bug #9: lock during handle creation

    Log::Info("WS", "Connecting to %s:%d", host, port);

    // Create WinHTTP session
    G.hSession = WinHttpOpen(L"cTrader/4.0",
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0);
    if (!G.hSession) {
        Log::Error("WS", "WinHttpOpen failed: %lu", GetLastError());
        return false;
    }

    // Convert host to wide string
    wchar_t wHost[256];
    MultiByteToWideChar(CP_UTF8, 0, host, -1, wHost, 256);

    // Connect
    G.hConnect = WinHttpConnect(G.hSession, wHost, (INTERNET_PORT)port, 0);
    if (!G.hConnect) {
        Log::Error("WS", "WinHttpConnect failed: %lu", GetLastError());
        Disconnect();
        return false;
    }

    // Open WebSocket request
    HINTERNET hRequest = WinHttpOpenRequest(G.hConnect, L"GET", L"/",
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        Log::Error("WS", "WinHttpOpenRequest failed: %lu", GetLastError());
        Disconnect();
        return false;
    }

    // Bug #13: SSL cert validation - conditional
    if (G.diagLevel >= 2) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
        Log::Warn("WS", "SSL cert validation DISABLED (diagLevel=%d)", G.diagLevel);
    }

    // Set WebSocket upgrade
    if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) {
        Log::Error("WS", "WebSocket upgrade option failed: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        Disconnect();
        return false;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        Log::Error("WS", "WinHttpSendRequest failed: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        Disconnect();
        return false;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        Log::Error("WS", "WinHttpReceiveResponse failed: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        Disconnect();
        return false;
    }

    // Complete WebSocket upgrade
    G.hWebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
    WinHttpCloseHandle(hRequest);

    if (!G.hWebSocket) {
        Log::Error("WS", "WebSocket upgrade failed: %lu", GetLastError());
        Disconnect();
        return false;
    }

    // Bug #7: Set receive timeout so NetworkThread doesn't block forever
    DWORD recvTimeout = 5000;  // 5 seconds
    WinHttpSetOption(G.hWebSocket, WINHTTP_OPTION_RECEIVE_TIMEOUT,
                     &recvTimeout, sizeof(recvTimeout));

    G.wsConnected = true;
    Log::Info("WS", "Connected to %s:%d", host, port);
    return true;
}

void Disconnect() {
    CsLock lock(G.csWebSocket);  // Bug #9: lock during handle destruction

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
    Log::Info("WS", "Disconnected");
}

bool Send(const char* message) {
    CsLock lock(G.csWebSocket);  // Bug #9: lock during send

    if (!G.hWebSocket || !G.wsConnected || !message) return false;

    DWORD len = (DWORD)strlen(message);
    DWORD err = WinHttpWebSocketSend(G.hWebSocket,
                                     WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                     (PVOID)message, len);
    if (err != NO_ERROR) {
        Log::Error("WS", "Send failed: %lu (len=%lu) -> disconnected", err, len);
        G.wsConnected = false;
        return false;
    }

    Log::Diag(2, "SEND: %s", message);
    return true;
}

int Receive(char* buffer, int bufferSize) {
    // Bug #9: NO lock on Receive - WinHTTP supports concurrent read/write
    if (!G.hWebSocket || !G.wsConnected || !buffer || bufferSize <= 0) return -1;

    DWORD bytesRead = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType;
    int totalRead = 0;

    do {
        DWORD err = WinHttpWebSocketReceive(G.hWebSocket,
                                            buffer + totalRead,
                                            bufferSize - totalRead - 1,
                                            &bytesRead, &bufferType);
        if (err != NO_ERROR) {
            // Bug #7: Handle timeout as "no data" (not error)
            if (err == ERROR_WINHTTP_TIMEOUT) {
                return 0;  // no data available, not an error
            }
            // Any other error = connection is dead
            G.wsConnected = false;
            Log::Warn("WS", "Receive error: %lu (CANCELLED=%d CONN_ERR=%d) -> disconnected",
                      err,
                      (err == ERROR_WINHTTP_OPERATION_CANCELLED) ? 1 : 0,
                      (err == ERROR_WINHTTP_CONNECTION_ERROR) ? 1 : 0);
            return -1;
        }

        totalRead += bytesRead;

        if (bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            Log::Warn("WS", "Server closed connection");
            G.wsConnected = false;
            return -1;
        }

    } while (bufferType == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE &&
             totalRead < bufferSize - 1);

    buffer[totalRead] = '\0';
    Log::Diag(2, "RECV: %s", buffer);
    return totalRead;
}

bool IsConnected() {
    return G.wsConnected && G.hWebSocket != NULL;
}

} // namespace WebSocket
