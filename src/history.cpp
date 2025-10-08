#include "../include/history.h"
#include "../include/globals.h"
#include "../include/utils.h"
#include "../include/network.h"
#include "../include/symbols.h"
#include <map>

namespace History {

// Global storage for pending history requests
static std::map<std::string, HistoryRequest> g_pendingRequests;
CRITICAL_SECTION g_cs_history;

bool RequestHistoricalData(const char* symbol, DATE startTime, DATE endTime, int timeframeMins, int maxTicks, T6* ticks) {
    if (!symbol || !ticks) return false;

    SymbolInfo* info = Symbols::GetSymbolByIdOrName(symbol);
    if (!info) {
        char msg[128];
        sprintf_s(msg, "Symbol not found for history: %s (normalized: %s)",
                  symbol, Symbols::NormalizeSymbol(symbol).c_str());
        Utils::ShowMsg(msg);
        return false;
    }

    // Convert DATE to Unix timestamp (milliseconds)
    // DATE is days since 1900-01-01, Unix epoch is 1970-01-01
    const double DAYS_1900_TO_1970 = 25569.0; // Days between 1900-01-01 and 1970-01-01
    long long startMs = (long long)((startTime - DAYS_1900_TO_1970) * 24 * 60 * 60 * 1000);
    long long endMs = (long long)((endTime - DAYS_1900_TO_1970) * 24 * 60 * 60 * 1000);

    // Convert timeframe minutes to cTrader timeframe enum
    int timeframe = ConvertTimeframeToEnum(timeframeMins);

    std::string clientMsgId = Utils::GetMsgId();

    // Store pending request
    StorePendingRequest(clientMsgId, symbol, ticks, maxTicks);

    // Request historical data
    char request[512];
    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
        "{\"ctidTraderAccountId\":%lld,"
        "\"symbolId\":%lld,\"period\":%d,\"fromTimestamp\":%lld,\"toTimestamp\":%lld}}",
        clientMsgId.c_str(), (int)PayloadType::PROTO_OA_GET_TRENDBARS_REQ, G.CTraderAccountId,
        info->id, timeframe, startMs, endMs);

    if (!Network::Send(request)) {
        Utils::ShowMsg("Historical data request failed");
        return false;
    }

    char msg[256];
    sprintf_s(msg, "Requested history for %s: %d mins timeframe, %d max ticks, msgId: %s",
              symbol, timeframeMins, maxTicks, clientMsgId.c_str());
    Utils::LogToFile("HISTORY_REQUEST", msg);

    // Wait for response (synchronous behavior expected by Zorro)
    return WaitForHistoryData(clientMsgId, 10000);
}

int ConvertTimeframeToEnum(int minutes) {
    // cTrader timeframe enums (approximate mapping)
    switch (minutes) {
        case 1:    return 1;   // M1
        case 5:    return 2;   // M5
        case 15:   return 3;   // M15
        case 30:   return 4;   // M30
        case 60:   return 5;   // H1
        case 240:  return 6;   // H4
        case 1440: return 7;   // D1
        case 10080: return 8;  // W1
        case 43200: return 9;  // MN1
        default:   return 5;   // Default to H1
    }
}

bool GetTickData(const char* symbol, DATE startTime, DATE endTime, int maxTicks, T6* ticks) {
    // Request tick data (1-minute timeframe as closest to ticks)
    return RequestHistoricalData(symbol, startTime, endTime, 1, maxTicks, ticks);
}

bool GetBarData(const char* symbol, DATE startTime, DATE endTime, int timeframeMins, int maxBars, T6* bars) {
    return RequestHistoricalData(symbol, startTime, endTime, timeframeMins, maxBars, bars);
}

void ProcessHistoricalResponse(const char* response) {
    char expected_response[128];
    sprintf_s(expected_response, "\"payloadType\":%d", (int)PayloadType::PROTO_OA_GET_TRENDBARS_RES);
    if (!strstr(response, expected_response)) return;

    Utils::LogToFile("HISTORY_RESPONSE", "Processing historical data response");

    // Extract clientMsgId from response
    const char* msgIdStart = strstr(response, "\"clientMsgId\":\"");
    if (!msgIdStart) return;

    msgIdStart += 15; // skip "clientMsgId":"
    const char* msgIdEnd = strchr(msgIdStart, '"');
    if (!msgIdEnd) return;

    std::string clientMsgId(msgIdStart, msgIdEnd - msgIdStart);

    EnterCriticalSection(&g_cs_history);
    HistoryRequest* req = GetPendingRequest(clientMsgId);
    if (!req || !req->ticksBuffer) {
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    // Parse historical data array
    const char* dataStart = strstr(response, "\"historicalData\":[");
    if (!dataStart) {
        req->completed = true;
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    dataStart += 18; // skip "historicalData":["
    const char* current = dataStart;
    int tickCount = 0;

    // Parse each OHLC bar: {"timestamp":123,"open":1.2,"high":1.3,"low":1.1,"close":1.25,"volume":1000}
    while (current && *current && tickCount < req->maxTicks) {
        // Find opening brace
        const char* barStart = strchr(current, '{');
        if (!barStart) break;

        const char* barEnd = strchr(barStart, '}');
        if (!barEnd) break;

        // Extract values
        long long timestamp = 0;
        double open = 0, high = 0, low = 0, close = 0, volume = 0;

        // Parse timestamp
        const char* tsPos = strstr(barStart, "\"timestamp\":");
        if (tsPos && tsPos < barEnd) {
            timestamp = _atoi64(tsPos + 12);
        }

        // Parse OHLC values
        const char* openPos = strstr(barStart, "\"open\":");
        if (openPos && openPos < barEnd) {
            open = atof(openPos + 7);
        }

        const char* highPos = strstr(barStart, "\"high\":");
        if (highPos && highPos < barEnd) {
            high = atof(highPos + 7);
        }

        const char* lowPos = strstr(barStart, "\"low\":");
        if (lowPos && lowPos < barEnd) {
            low = atof(lowPos + 6);
        }

        const char* closePos = strstr(barStart, "\"close\":");
        if (closePos && closePos < barEnd) {
            close = atof(closePos + 8);
        }

        const char* volPos = strstr(barStart, "\"volume\":");
        if (volPos && volPos < barEnd) {
            volume = atof(volPos + 9);
        }

        // Convert timestamp to DATE format (Zorro expects days since 1900-01-01)
        const double DAYS_1900_TO_1970 = 25569.0;
        DATE zorroTime = DAYS_1900_TO_1970 + (timestamp / (24.0 * 60.0 * 60.0 * 1000.0));

        // Fill T6 structure
        T6* tick = &req->ticksBuffer[tickCount];
        tick->time = zorroTime;
        tick->fOpen = (float)open;
        tick->fHigh = (float)high;
        tick->fLow = (float)low;
        tick->fClose = (float)close;
        tick->fVal = (float)volume;

        tickCount++;
        current = barEnd + 1;

        // Skip to next bar (look for comma or end)
        while (current && *current && *current != ',' && *current != ']') current++;
        if (*current == ',') current++;
    }

    req->receivedTicks = tickCount;
    req->completed = true;

    char msg[256];
    sprintf_s(msg, "Parsed %d historical bars for msgId: %s", tickCount, clientMsgId.c_str());
    Utils::LogToFile("HISTORY_PARSED", msg);

    LeaveCriticalSection(&g_cs_history);
}

void StorePendingRequest(const std::string& msgId, const char* symbol, T6* buffer, int maxTicks) {
    EnterCriticalSection(&g_cs_history);

    HistoryRequest req;
    req.symbol = symbol;
    req.clientMsgId = msgId;
    req.ticksBuffer = buffer;
    req.maxTicks = maxTicks;
    req.receivedTicks = 0;
    req.completed = false;
    req.requestTime = GetTickCount64();

    g_pendingRequests[msgId] = req;

    LeaveCriticalSection(&g_cs_history);
}

bool WaitForHistoryData(const std::string& msgId, int timeoutMs) {
    ULONGLONG startTime = GetTickCount64();

    while (GetTickCount64() - startTime < (ULONGLONG)timeoutMs) {
        EnterCriticalSection(&g_cs_history);

        auto it = g_pendingRequests.find(msgId);
        if (it != g_pendingRequests.end() && it->second.completed) {
            int receivedTicks = it->second.receivedTicks;
            g_pendingRequests.erase(it); // Cleanup
            LeaveCriticalSection(&g_cs_history);

            char msg[128];
            sprintf_s(msg, "History data completed: %d ticks received", receivedTicks);
            Utils::LogToFile("HISTORY_COMPLETE", msg);
            return true;
        }

        LeaveCriticalSection(&g_cs_history);
        Sleep(50); // Wait 50ms before checking again
    }

    // Timeout - cleanup pending request
    EnterCriticalSection(&g_cs_history);
    g_pendingRequests.erase(msgId);
    LeaveCriticalSection(&g_cs_history);

    Utils::LogToFile("HISTORY_TIMEOUT", "Historical data request timed out");
    return false;
}

HistoryRequest* GetPendingRequest(const std::string& msgId) {
    auto it = g_pendingRequests.find(msgId);
    return (it != g_pendingRequests.end()) ? &it->second : nullptr;
}

} // namespace History