#include "../include/history.h"
#include "../include/globals.h"
#include "../include/utils.h"
#include "../include/network.h"
#include "../include/symbols.h"
#include "../include/http_api.h"
#include "../include/history_rest.h"
#include <map>
#include <limits>
#include <cmath>
#include <float.h>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cstring>

namespace {
constexpr double DAYS_1900_TO_1970 = 25569.0;
constexpr long long MILLIS_PER_DAY = 24LL * 60LL * 60LL * 1000LL;
constexpr long long DEFAULT_LOOKBACK_MS = 60LL * 60LL * 1000LL;  // 1 hour (working range)
constexpr long long MAX_HISTORY_CHUNK_MS = 7LL * 24LL * 60LL * 60LL * 1000LL; // 7 days per chunk (server limit)
constexpr long long MAX_TICK_WINDOW_MS = 7LL * 24LL * 60LL * 60LL * 1000LL; // Open API tick limit
constexpr DWORD TICK_REQUEST_COOLDOWN_MS = 50000; // 50s rate limit between tick requests

long long CurrentUnixMillis() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    constexpr long long EPOCH_DIFF = 116444736000000000LL; // 1601 to 1970 in 100-ns ticks
    if (uli.QuadPart < static_cast<unsigned long long>(EPOCH_DIFF)) {
        return 0;
    }
    return static_cast<long long>((uli.QuadPart - EPOCH_DIFF) / 10000LL);
}

long long DateToUnixMillisOrFallback(DATE value, long long fallback) {
    if (value > 0.0) {
        double daysSinceEpoch = value - DAYS_1900_TO_1970;
        double millis = daysSinceEpoch * static_cast<double>(MILLIS_PER_DAY);
        if (_finite(millis) && millis > 0.0) {
            return static_cast<long long>(millis);
        }
    }
    if (fallback >= 0) {
        return fallback;
    }
    return CurrentUnixMillis();
}

std::string QuoteTypeLabel(int quoteType) {
    switch (quoteType) {
    case 2: return "ASK";
    case 1: return "BID";
    default: return "UNKNOWN";
    }
}
} // namespace

namespace History {

// Global storage for pending history requests
static std::map<std::string, HistoryRequest> g_pendingRequests;
CRITICAL_SECTION g_cs_history;

// Backward compatibility shim for legacy function name
inline void StorePendingRequest(const std::string& msgId, const char* symbol, T6* buffer, int maxTicks, int timeframeMins = 0) {
    RegisterPendingRequest(msgId, symbol, buffer, maxTicks, false, 0, timeframeMins);
}

bool RequestHistoricalData(const char* symbol, DATE startTime, DATE endTime, int timeframeMins, int maxTicks, T6* ticks) {
    // DEBUG: Entry logging
    char entryMsg[512];
    sprintf_s(entryMsg, "RequestHistoricalData ENTRY: symbol=%s, startTime=%.5f, endTime=%.5f, timeframeMins=%d, maxTicks=%d",
              symbol ? symbol : "NULL", startTime, endTime, timeframeMins, maxTicks);
    Utils::LogToFile("HISTORY_DEBUG", entryMsg);

    if (!symbol || !ticks) {
        Utils::LogToFile("HISTORY_DEBUG", "NULL parameter check failed");
        return 0;
    }

    SymbolInfo* info = Symbols::GetSymbolByIdOrName(symbol);
    if (!info) {
        char msg[256];
        sprintf_s(msg, "Symbol not found for history: %s (normalized: %s)",
                  symbol, Symbols::NormalizeSymbol(symbol).c_str());
        Utils::LogToFile("HISTORY_DEBUG", msg);
        Utils::ShowMsg(msg);
        return 0;
    } else {
        char symbolMsg[256];
        sprintf_s(symbolMsg, "Symbol FOUND: name=%s, ID=%lld, digits=%d", symbol, info->id, info->digits);
        Utils::LogToFile("HISTORY_DEBUG", symbolMsg);
    }

    long long nowMs = CurrentUnixMillis();
    long long endMs = DateToUnixMillisOrFallback(endTime, nowMs);
    long long startMs = DateToUnixMillisOrFallback(startTime, endMs - DEFAULT_LOOKBACK_MS);

    // Lookback handling: if Zorro requests last N bars (tStart == 0) use timeframe * N to compute window
    if (startTime <= 0.0 && timeframeMins > 0 && maxTicks > 0) {
        long long lookbackMs = (long long)timeframeMins * 60000LL * (long long)maxTicks;

        // Enhanced margin calculation based on timeframe and market characteristics
        // For intraday (< 1 day): add 50% margin for session gaps
        // For daily+: add 100% margin for weekends/holidays
        double marginFactor = (timeframeMins < 1440) ? 1.5 : 2.0;
        long long enhancedMargin = (long long)(lookbackMs * (marginFactor - 1.0));

        // Cap minimum margin at 24 hours and maximum at 7 days
        long long minMarginMs = 24LL * 60LL * 60LL * 1000LL;  // 24 hours
        long long maxMarginMs = 7LL * 24LL * 60LL * 60LL * 1000LL;  // 7 days
        long long marginMs = std::max(minMarginMs, std::min(enhancedMargin, maxMarginMs));

        long long computedStart = endMs - (lookbackMs + marginMs);
        if (computedStart < 0) computedStart = 0;
        startMs = computedStart;

        char lbMsg[256];
        sprintf_s(lbMsg, "LOOKBACK: timeframe=%d min, ticks=%d -> window=%lld ms (margin=%.0f%%, %lld ms) | startMs=%lld, endMs=%lld",
                  timeframeMins, maxTicks, lookbackMs + marginMs, (marginFactor - 1.0) * 100, marginMs, startMs, endMs);
        Utils::LogToFile("HISTORY_LOOKBACK", lbMsg);
    }
    // DEBUG: Timestamp logging
    char tsMsg[512];
    sprintf_s(tsMsg, "Timestamps: nowMs=%lld, startMs=%lld, endMs=%lld", nowMs, startMs, endMs);
    Utils::LogToFile("HISTORY_DEBUG", tsMsg);

    if (endMs <= 0) {
        Utils::LogToFile("HISTORY_RANGE", "End time missing; defaulting to current time");
        endMs = nowMs;
    }

    // If end time is in the future or too far ahead, cap it to now
    if (endMs > nowMs + 86400000) {  // More than 1 day in the future
        char msg[256];
        sprintf_s(msg, "End time %lld is in the future, capping to current time %lld", endMs, nowMs);
        Utils::LogToFile("HISTORY_RANGE", msg);
        endMs = nowMs;
    }

    if (startMs < 0 || startMs > endMs) {
        Utils::LogToFile("HISTORY_RANGE", "Start time invalid; using fallback window");
        startMs = endMs - DEFAULT_LOOKBACK_MS;
    }
    if (startMs < 0) {
        startMs = 0;
    }

    // Limit history range to 60 days (working range for cTrader API)
    constexpr long long MAX_HISTORY_RANGE_MS = 60LL * 86400000;  // 60 days (working range)
    constexpr DWORD HISTORY_REQUEST_COOLDOWN_MS = 2000;  // 2 second rate limit between requests
    if (startMs < endMs - MAX_HISTORY_RANGE_MS) {
        char msg[256];
        sprintf_s(msg, "History range limited to 60 days (requested %.1f days)", (endMs - startMs) / 86400000.0);
        Utils::LogToFile("HISTORY_RANGE_LIMIT", msg);
        startMs = endMs - MAX_HISTORY_RANGE_MS;
    }

    // Server supports max 7-day chunks, split longer requests into multiple chunks
    long long requestedRange = endMs - startMs;
    int period = ConvertTimeframeToEnum(timeframeMins);
    int totalReceived = 0;

    // DEBUG: Period and range logging
    char periodMsg[512];
    sprintf_s(periodMsg, "Period enum=%d (from %d mins), requestedRange=%lld ms (%.2f days)",
              period, timeframeMins, requestedRange, requestedRange / 86400000.0);
    Utils::LogToFile("HISTORY_DEBUG", periodMsg);

    if (requestedRange > MAX_HISTORY_CHUNK_MS) {
        // Split into multiple 7-day chunks
        int numChunks = (int)((requestedRange + MAX_HISTORY_CHUNK_MS - 1) / MAX_HISTORY_CHUNK_MS);
        char msg[256];
        sprintf_s(msg, "Requested range %.1f days, splitting into %d chunks of 7 days each",
                  requestedRange / (double)MILLIS_PER_DAY, numChunks);
        Utils::LogToFile("HISTORY_RANGE", msg);

        // Request chunks from oldest to newest
        for (int i = 0; i < numChunks && totalReceived < maxTicks; i++) {
            long long chunkStart = startMs + (i * MAX_HISTORY_CHUNK_MS);
            long long chunkEnd = std::min(chunkStart + MAX_HISTORY_CHUNK_MS, endMs);
            int remainingCapacity = maxTicks - totalReceived;

            std::string clientMsgId = Utils::GetMsgId();
            StorePendingRequest(clientMsgId, symbol, ticks + totalReceived, remainingCapacity, timeframeMins);

            std::ostringstream oss;
            oss << "{\"clientMsgId\":\"" << clientMsgId << "\",\"payloadType\":"
                << ToInt(PayloadType::GetTrendbarsReq)
                << ",\"payload\":{\"ctidTraderAccountId\":" << G.CTraderAccountId
                << ",\"symbolId\":" << info->id
                << ",\"period\":" << period
                << ",\"fromTimestamp\":" << chunkStart
                << ",\"toTimestamp\":" << chunkEnd
                << "}}";

            if (!Network::Send(oss.str().c_str())) {
                Utils::LogToFile("HISTORY_ERROR", "Chunk request failed");
                break;
            }

            char chunkMsg[256];
            sprintf_s(chunkMsg, "Chunk %d/%d: %s, %lld to %lld, msgId: %s",
                      i + 1, numChunks, symbol, chunkStart, chunkEnd, clientMsgId.c_str());
            Utils::LogToFile("HISTORY_REQUEST", chunkMsg);

            int chunkReceived = 0;
            if (WaitForHistoryData(clientMsgId, 5000, &chunkReceived)) {
                totalReceived += chunkReceived;
            } else {
                Utils::LogToFile("HISTORY_ERROR", "Chunk timeout or failed");
                break;
            }
        }

        return totalReceived;
    }

    // Single request (fits within 7-day limit)
    std::string clientMsgId = Utils::GetMsgId();
    StorePendingRequest(clientMsgId, symbol, ticks, maxTicks, timeframeMins);

    int requestedCount = (maxTicks > 0) ? maxTicks : 0;

    std::ostringstream oss;
    oss << "{\"clientMsgId\":\"" << clientMsgId << "\",\"payloadType\":"
        << ToInt(PayloadType::GetTrendbarsReq)
        << ",\"payload\":{\"ctidTraderAccountId\":" << G.CTraderAccountId
        << ",\"symbolId\":" << info->id
        << ",\"period\":" << period
        << ",\"fromTimestamp\":" << startMs
        << ",\"toTimestamp\":" << endMs
        << "}}";

    const std::string request = oss.str();

    // DEBUG: Log full request before sending
    Utils::LogToFile("HISTORY_DEBUG", "About to send ProtoOAGetTrendbarsReq:");
    Utils::LogToFile("HISTORY_DEBUG", request.c_str());

    if (!Network::Send(request.c_str())) {
        Utils::LogToFile("HISTORY_DEBUG", "Network::Send() returned FALSE - send failed");
        Utils::ShowMsg("Historical data request failed");
        return 0;
    }

    Utils::LogToFile("HISTORY_DEBUG", "Network::Send() returned TRUE - request sent successfully");

    char msg[256];
    sprintf_s(msg, "Requested history for %s: %d mins timeframe, %d max ticks, msgId: %s",
              symbol, timeframeMins, maxTicks, clientMsgId.c_str());
    Utils::LogToFile("HISTORY_REQUEST", msg);

    // Adaptive timeout based on requested data size
    // Base: 5s, +1s per 100 ticks requested, max 30s
    int timeoutMs = 5000 + ((maxTicks / 100) * 1000);
    timeoutMs = std::min(timeoutMs, 30000);

    int receivedTicks = 0;
    if (WaitForHistoryData(clientMsgId, timeoutMs, &receivedTicks)) {
        // Check if we received significantly less than requested
        if (receivedTicks > 0 && maxTicks > 0 && receivedTicks < maxTicks) {
            double percentage = (double)receivedTicks / (double)maxTicks * 100.0;
            if (percentage < 80.0) {  // Less than 80% of requested data
                char warnMsg[256];
                sprintf_s(warnMsg, "WARNING: Received only %d/%d bars (%.0f%%) - may indicate insufficient lookback period or market gaps",
                          receivedTicks, maxTicks, percentage);
                Utils::LogToFile("HISTORY_PARTIAL_DATA", warnMsg);
                Utils::ShowMsg("Partial history data", warnMsg);
            }
        }
        return receivedTicks;  // Return actual tick count, not boolean!
    }

    char timeoutMsg[128];
    sprintf_s(timeoutMsg, "WebSocket history request timed out after %d ms", timeoutMs);
    Utils::LogToFile("HISTORY_TIMEOUT", timeoutMsg);
    return 0;  // Return 0 on timeout, not false
}

int ConvertTimeframeToEnum(int minutes) {
    // Map Zorro minute intervals to cTrader ProtoOA period enum values
    // Reference: ProtoOACommonMessages.ProtoOATrendbarPeriod (Open API v6)
    switch (minutes) {
        case 1:    return 1;  // M1
        case 2:    return 2;  // M2
        case 3:    return 3;  // M3
        case 4:    return 4;  // M4
        case 5:    return 5;  // M5
        case 10:   return 6;  // M10
        case 15:   return 7;  // M15
        case 30:   return 8;  // M30
        case 45:   return 8;  // Approximate with M30 (closest available)
        case 60:   return 9;  // H1
        case 120:  return 10; // H2
        case 180:  return 10; // Approximate with H2 (closest available)
        case 240:  return 11; // H4
        case 360:  return 11; // Approximate with H4
        case 480:  return 12; // H8
        case 720:  return 12; // Approximate with H8
        case 1440: return 13; // D1
        case 10080:return 14; // W1
        case 43200:return 15; // MN1
        default:
            Utils::LogToFile("HISTORY_RANGE", "Unknown timeframe; defaulting to H1 (ProtoOA=9)");
            return 9;  // fallback to H1
    }
}

bool GetTickData(const char* symbol, DATE startTime, DATE endTime, int maxTicks, T6* ticks) {
    if (!symbol || !ticks) return 0;

    SymbolInfo* info = Symbols::GetSymbolByIdOrName(symbol);
    if (!info || info->id <= 0) {
        char msg[128];
        sprintf_s(msg, "Symbol not found for tick data: %s (normalized: %s)",
                  symbol, Symbols::NormalizeSymbol(symbol).c_str());
        Utils::ShowMsg(msg);
        return 0;
    }

    long long nowMs = CurrentUnixMillis();
    long long requestedEnd = DateToUnixMillisOrFallback(endTime, nowMs);
    long long requestedStart = DateToUnixMillisOrFallback(startTime, requestedEnd - DEFAULT_LOOKBACK_MS);

    if (requestedEnd <= 0) {
        Utils::LogToFile("TICK_RANGE", "End time missing; defaulting to current time");
        requestedEnd = nowMs;
    }
    if (requestedStart < 0 || requestedStart > requestedEnd) {
        Utils::LogToFile("TICK_RANGE", "Start time invalid; using fallback window");
        requestedStart = requestedEnd - DEFAULT_LOOKBACK_MS;
    }
    if (requestedStart < 0) {
        requestedStart = 0;
    }

    const int maxAllowed = (maxTicks > 0) ? maxTicks : std::numeric_limits<int>::max();
    int totalReceived = 0;
    long long chunkStart = requestedStart;
    int chunkIndex = 0;
    int quoteType = 1;
    std::string quoteLabel = QuoteTypeLabel(quoteType);

    while (chunkStart < requestedEnd && totalReceived < maxAllowed) {
        long long chunkEnd = std::min(chunkStart + MAX_TICK_WINDOW_MS, requestedEnd);
        std::string clientMsgId = Utils::GetMsgId();
        int remainingCapacity = maxAllowed - totalReceived;

        RegisterPendingRequest(clientMsgId, symbol, ticks + totalReceived, remainingCapacity, true, quoteType);

        char request[512];
        sprintf_s(request,
            "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
            "{\"ctidTraderAccountId\":%lld,\"symbolId\":%lld,\"type\":%d,\"fromTimestamp\":%lld,\"toTimestamp\":%lld}}",
            clientMsgId.c_str(), ToInt(PayloadType::GetTickdataReq),
            G.CTraderAccountId, info->id, quoteType, chunkStart, chunkEnd);

        if (!Network::Send(request)) {
            Utils::ShowMsg("Tick data request failed");
            return totalReceived;  // Return what we got so far
        }

        char logMsg[256];
        sprintf_s(logMsg, "Requested tick chunk %d for %s (%s): %lld-%lld ms, msgId: %s",
                  ++chunkIndex, symbol, quoteLabel.c_str(), chunkStart, chunkEnd, clientMsgId.c_str());
        Utils::LogToFile("TICK_REQUEST", logMsg);

        int chunkReceived = 0;
        if (!WaitForHistoryData(clientMsgId, 5000, &chunkReceived)) {
            Utils::LogToFile("TICK_REQUEST", "Tick chunk request timed out or failed");
            return totalReceived;  // Return what we got so far
        }

        totalReceived += chunkReceived;
        chunkStart = chunkEnd;

        if (chunkStart < requestedEnd) {
            Utils::LogToFile("TICK_REQUEST", "Respecting tick rate limit; sleeping 50 seconds before next chunk");
            Sleep(TICK_REQUEST_COOLDOWN_MS);
        }

    }

    if (totalReceived == 0) {
        Utils::LogToFile("TICK_REQUEST", "Tick request returned no data");
    }

    return totalReceived > 0;  // Return actual count received
}

bool GetBarData(const char* symbol, DATE startTime, DATE endTime, int timeframeMins, int maxBars, T6* bars) {
    return RequestHistoricalData(symbol, startTime, endTime, timeframeMins, maxBars, bars);
}

void ProcessHistoricalResponse(const char* response) {
    if (!strstr(response, "\"payloadType\":2138")) return;

    Utils::LogToFile("HISTORY_RESPONSE", "Processing historical data response");

    const char* msgIdStart = strstr(response, "\"clientMsgId\":\"");
    if (!msgIdStart) return;

    msgIdStart += 15;
    const char* msgIdEnd = strchr(msgIdStart, '"');
    if (!msgIdEnd) return;

    std::string clientMsgId(msgIdStart, msgIdEnd - msgIdStart);

    EnterCriticalSection(&g_cs_history);
    HistoryRequest* req = GetPendingRequest(clientMsgId);
    if (!req || !req->ticksBuffer) {
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    // Check for error response
    const char* errorCode = strstr(response, "\"errorCode\":\"");
    if (errorCode) {
        errorCode += 13;
        const char* errorEnd = strchr(errorCode, '"');
        std::string error(errorCode, errorEnd ? (errorEnd - errorCode) : 20);

        const char* description = strstr(response, "\"description\":\"");
        std::string desc = "No description";
        if (description) {
            description += 15;
            const char* descEnd = strchr(description, '"');
            if (descEnd) desc.assign(description, descEnd - description);
        }

        char msg[256];
        sprintf_s(msg, "History request failed for %s: %s - %s",
                  req->symbol.c_str(), error.c_str(), desc.c_str());
        Utils::ShowMsg(msg);
        Utils::LogToFile("HISTORY_ERROR", msg);

        req->receivedTicks = 0;
        req->completed = true;
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    const char* dataStart = strstr(response, "\"trendbar\":[");
    if (!dataStart) {
        dataStart = strstr(response, "\"historicalData\":[");
    }

    if (!dataStart) {
        // Don't show error immediately
        // char msg[256];
        // sprintf_s(msg, "%s history unavailable!", req->symbol.c_str());
        // Utils::ShowMsg(msg);

        // Log first 500 chars of response for debugging
        char debugMsg[600];
        int len = std::min(500, (int)strlen(response));
        snprintf(debugMsg, sizeof(debugMsg), "HISTORY_RESPONSE_RAW: %.500s", response);
        Utils::LogToFile("HISTORY_DEBUG", debugMsg);

        Utils::LogToFile("HISTORY_RESPONSE", "Trendbar array missing in response payload");
        req->receivedTicks = 0;
        req->completed = true;
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    // Move pointer past the array label
    dataStart = strchr(dataStart, '[');
    if (!dataStart) {
        req->completed = true;
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    dataStart += 1;
    const char* current = dataStart;
    int tickCount = 0;

    // cTrader API ALWAYS uses 100,000 scale (not 10^digits) - see downloadtickdatasample.md reference
    // Python ref: df2['open'] = (df['low'] + df['deltaOpen']) / 100_000
    // Use literal division to match Python exactly (no double constant to avoid precision loss)

    while (current && *current && tickCount < req->maxTicks) {
        const char* barStart = strchr(current, '{');
        if (!barStart) break;

        const char* barEnd = strchr(barStart, '}');
        if (!barEnd) break;

        // Debug: log first bar raw JSON
        if (tickCount == 0) {
            int barLen = (int)(barEnd - barStart + 1);
            if (barLen > 0 && barLen < 500) {
                char firstBar[550];
                snprintf(firstBar, sizeof(firstBar), "HISTORY_FIRST_BAR_RAW: %.*s", barLen, barStart);
                Utils::LogToFile("HISTORY_DEBUG", firstBar);
            }
        }

        long long timestampMinutes = 0;
        long long low = 0, deltaOpen = 0, deltaHigh = 0, deltaClose = 0;
        double volume = 0;

        // cTrader uses utcTimestampInMinutes (not milliseconds!)
        const char* tsPos = strstr(barStart, "\"utcTimestampInMinutes\":");
        if (tsPos && tsPos < barEnd) {
            tsPos += 24;
            timestampMinutes = _atoi64(tsPos);
        }
        // Fallback to old format if needed
        if (timestampMinutes == 0) {
            tsPos = strstr(barStart, "\"timestamp\":");
            if (tsPos && tsPos < barEnd) {
                tsPos += 12;
                timestampMinutes = _atoi64(tsPos) / 60000; // Convert ms to minutes
            }
        }

        // Debug: Log parsed timestamp and raw values
        if (tickCount < 3) {  // Only log first 3 bars
            char tsDebug[512];
            snprintf(tsDebug, sizeof(tsDebug),
                "BAR[%d] RAW: timestampMinutes=%lld, low=%lld, deltaOpen=%lld, deltaHigh=%lld, deltaClose=%lld, volume=%.6f",
                tickCount, timestampMinutes, low, deltaOpen, deltaHigh, deltaClose, volume);
            Utils::LogToFile("HISTORY_DEBUG", tsDebug);
        }

        // cTrader uses delta encoding: low is absolute, others are relative to low
        // These are INTEGERS in the API (long long), not doubles!
        const char* lowPos = strstr(barStart, "\"low\":");
        if (lowPos && lowPos < barEnd) low = _atoi64(lowPos + 6);

        const char* deltaOpenPos = strstr(barStart, "\"deltaOpen\":");
        if (deltaOpenPos && deltaOpenPos < barEnd) deltaOpen = _atoi64(deltaOpenPos + 12);

        const char* deltaHighPos = strstr(barStart, "\"deltaHigh\":");
        if (deltaHighPos && deltaHighPos < barEnd) deltaHigh = _atoi64(deltaHighPos + 12);

        const char* deltaClosePos = strstr(barStart, "\"deltaClose\":");
        if (deltaClosePos && deltaClosePos < barEnd) deltaClose = _atoi64(deltaClosePos + 13);

        const char* volPos = strstr(barStart, "\"volume\":");
        if (volPos && volPos < barEnd) {
            volume = atof(volPos + 9);
        } else {
            // Fallback to tickVolume if volume not found (like REST API does)
            const char* tickVolPos = strstr(barStart, "\"tickVolume\":");
            if (tickVolPos && tickVolPos < barEnd) volume = atof(tickVolPos + 13);
        }

        // Convert minutes to milliseconds, then to Zorro DATE
        long long timestampMs = timestampMinutes * 60000LL;
        DATE zorroTime = DAYS_1900_TO_1970 + (timestampMs / (24.0 * 60.0 * 60.0 * 1000.0));

        // Debug: Log conversion
        if (tickCount < 3) {  // Only log first 3 bars
            char convDebug[300];
            snprintf(convDebug, sizeof(convDebug),
                "BAR[%d] timestampMs=%lld, zorroTime=%.8f (DAYS_1900_TO_1970=%.1f)",
                tickCount, timestampMs, zorroTime, DAYS_1900_TO_1970);
            Utils::LogToFile("HISTORY_DEBUG", convDebug);
        }

        // Calculate prices with INTEGER arithmetic first, then divide by 100,000
        // Matching Python reference: df2['open'] = (df['low'] + df['deltaOpen']) / 100_000
        // cTrader API sends all prices as long long integers, scaled by 100,000
        T6* tick = &req->ticksBuffer[tickCount];
        tick->time = zorroTime;
        tick->fOpen = (double)(low + deltaOpen) / 100000.0;
        tick->fHigh = (double)(low + deltaHigh) / 100000.0;
        tick->fLow = (double)low / 100000.0;
        tick->fClose = (double)(low + deltaClose) / 100000.0;
        tick->fVal = volume;

        // Debug: Log converted T6 values for first 3 bars
        if (tickCount < 3) {
            char t6Debug[512];
            snprintf(t6Debug, sizeof(t6Debug),
                "BAR[%d] T6: time=%.8f, fOpen=%.6f, fHigh=%.6f, fLow=%.6f, fClose=%.6f, fVal=%.6f",
                tickCount, tick->time, tick->fOpen, tick->fHigh, tick->fLow, tick->fClose, tick->fVal);
            Utils::LogToFile("HISTORY_DEBUG", t6Debug);
        }

        // Sanity check: Detect if price values are suspiciously large (possible timestamp contamination)
        if (tick->fOpen > 1000000.0 || tick->fHigh > 1000000.0 || tick->fLow > 1000000.0 || tick->fClose > 1000000.0) {
            char warnMsg[512];
            sprintf_s(warnMsg, "WARNING: Suspiciously large price detected! BAR[%d] O=%.2f H=%.2f L=%.2f C=%.2f (raw: low=%lld deltaO=%lld deltaH=%lld deltaC=%lld)",
                      tickCount, tick->fOpen, tick->fHigh, tick->fLow, tick->fClose, low, deltaOpen, deltaHigh, deltaClose);
            Utils::LogToFile("HISTORY_SANITY_CHECK_FAIL", warnMsg);
        }

        tickCount++;
        current = barEnd + 1;

        while (current && *current && *current != ',' && *current != ']') current++;
        if (*current == ',') current++;
    }

    // CRITICAL FIX: If we received MORE than requested (due to lookback margin),
    // only keep the LAST maxTicks bars (most recent data).
    // Otherwise Zorro will use the FIRST maxTicks bars (oldest data) - WRONG!
    if (req->maxTicks > 0 && tickCount > req->maxTicks) {
        int excess = tickCount - req->maxTicks;
        char trimMsg[256];
        sprintf_s(trimMsg, "LOOKBACK_TRIM: Received %d bars but Zorro requested only %d - trimming %d oldest bars",
                  tickCount, req->maxTicks, excess);
        Utils::LogToFile("HISTORY_TRIM", trimMsg);

        // Move the LAST maxTicks bars to the beginning of the buffer
        // memmove is safe for overlapping memory regions
        memmove(req->ticksBuffer, req->ticksBuffer + excess, req->maxTicks * sizeof(T6));
        tickCount = req->maxTicks;

        char trimmedMsg[256];
        sprintf_s(trimmedMsg, "LOOKBACK_TRIM: After trim - First bar time=%.8f, Last bar time=%.8f",
                  req->ticksBuffer[0].time, req->ticksBuffer[tickCount-1].time);
        Utils::LogToFile("HISTORY_TRIM", trimmedMsg);
    }

    req->receivedTicks = tickCount;
    req->completed = true;

    char msg[256];
    sprintf_s(msg, "Parsed %d historical bars for msgId: %s (maxTicks=%d)",
              tickCount, clientMsgId.c_str(), req->maxTicks);
    Utils::LogToFile("HISTORY_PARSED", msg);

    LeaveCriticalSection(&g_cs_history);
}

void HandleTickDataResponse(const char* response) {
    if (!response) return;
    if (!strstr(response, "\"payloadType\":2146")) return;

    const char* msgIdStart = strstr(response, "\"clientMsgId\":\"");
    if (!msgIdStart) return;
    msgIdStart += 15;
    const char* msgIdEnd = strchr(msgIdStart, '"');
    if (!msgIdEnd) return;

    std::string clientMsgId(msgIdStart, msgIdEnd - msgIdStart);

    EnterCriticalSection(&g_cs_history);

    HistoryRequest* req = GetPendingRequest(clientMsgId);
    if (!req || !req->ticksBuffer) {
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    // Check for error response
    const char* errorCode = strstr(response, "\"errorCode\":\"");
    if (errorCode) {
        errorCode += 13;
        const char* errorEnd = strchr(errorCode, '"');
        std::string error(errorCode, errorEnd ? (errorEnd - errorCode) : 20);

        const char* description = strstr(response, "\"description\":\"");
        std::string desc = "No description";
        if (description) {
            description += 15;
            const char* descEnd = strchr(description, '"');
            if (descEnd) desc.assign(description, descEnd - description);
        }

        char msg[256];
        sprintf_s(msg, "Tick data request failed for %s: %s - %s",
                  req->symbol.c_str(), error.c_str(), desc.c_str());
        Utils::ShowMsg(msg);
        Utils::LogToFile("TICK_ERROR", msg);

        req->receivedTicks = 0;
        req->completed = true;
        LeaveCriticalSection(&g_cs_history);
        return;
    }

    req->isTickData = true;

    // cTrader API ALWAYS uses 100,000 scale (not 10^digits)
    // Use literal division to match Python reference

    const char* dataStart = strstr(response, "\"tickData\":[");
    if (!dataStart) {
        req->completed = true;
        LeaveCriticalSection(&g_cs_history);
        return;
    }
    dataStart += 12;

    const char* cursor = dataStart;
    long long prevTimestamp = req->lastTimestamp;
    long long prevTickValue = req->lastTickValue;
    int index = 0;

    while (cursor && *cursor && req->receivedTicks < req->maxTicks) {
        if (*cursor == ']') break;

        const char* objStart = strchr(cursor, '{');
        if (!objStart) break;
        const char* objEnd = strchr(objStart, '}');
        if (!objEnd) break;

        long long rawTimestamp = 0;
        long long rawTick = 0;

        const char* tsPos = strstr(objStart, "\"timestamp\":");
        if (tsPos && tsPos < objEnd) rawTimestamp = _atoi64(tsPos + 12);
        const char* tickPos = strstr(objStart, "\"tick\":");
        if (tickPos && tickPos < objEnd) rawTick = _atoi64(tickPos + 7);

        long long absoluteTimestamp = 0;
        long long absoluteTick = 0;

        if (req->receivedTicks == 0 && index == 0 && req->lastTimestamp == 0) {
            absoluteTimestamp = rawTimestamp;
            absoluteTick = rawTick;
        } else if (index == 0) {
            absoluteTimestamp = req->lastTimestamp + rawTimestamp;
            absoluteTick = req->lastTickValue + rawTick;
        } else {
            absoluteTimestamp = prevTimestamp + rawTimestamp;
            absoluteTick = prevTickValue + rawTick;
        }

        double price = (double)absoluteTick / 100000.0;
        DATE zorroTime = DAYS_1900_TO_1970 + (absoluteTimestamp / (24.0 * 60.0 * 60.0 * 1000.0));

        T6* tick = &req->ticksBuffer[req->receivedTicks];
        tick->time = zorroTime;
        tick->fOpen = tick->fHigh = tick->fLow = tick->fClose = price;
        tick->fVal = 0.0;

        req->receivedTicks++;
        prevTimestamp = absoluteTimestamp;
        prevTickValue = absoluteTick;

        cursor = objEnd + 1;
        while (cursor && *cursor && *cursor != '{' && *cursor != ']') cursor++;
        index++;
    }

    req->lastTimestamp = prevTimestamp;
    req->lastTickValue = prevTickValue;
    int parsedCount = req->receivedTicks;
    bool exhausted = (req->receivedTicks >= req->maxTicks);
    req->completed = true;

    LeaveCriticalSection(&g_cs_history);

    if (parsedCount > 0) {
        char msg[160];
        sprintf_s(msg, "Parsed %d tick records for msgId: %s", parsedCount, clientMsgId.c_str());
        Utils::LogToFile("HISTORY_TICK_PARSED", msg);
    } else {
        Utils::LogToFile("HISTORY_TICK_PARSED", "Tick response contained no data");
    }

    const char* hasMore = strstr(response, "\"hasMore\":");
    if (hasMore && strncmp(hasMore + 10, "true", 4) == 0) {
        Utils::LogToFile("HISTORY_TICK", "Server indicated additional tick data beyond request window");
    }

    if (exhausted) {
        Utils::LogToFile("HISTORY_TICK", "Tick buffer limit reached during parsing");
    }
}

void RegisterPendingRequest(const std::string& msgId, const char* symbol, T6* buffer, int maxTicks,
                         bool isTickData, int quoteType, int timeframeMins) {
    EnterCriticalSection(&g_cs_history);

    HistoryRequest req;
    req.symbol = symbol;
    req.clientMsgId = msgId;
    req.ticksBuffer = buffer;
    req.maxTicks = (maxTicks > 0) ? maxTicks : std::numeric_limits<int>::max();
    req.receivedTicks = 0;
    req.completed = false;
    req.requestTime = GetTickCount64();
    req.isTickData = isTickData;
    req.quoteType = quoteType;
    req.lastTimestamp = 0;
    req.lastTickValue = 0;
    req.timeframeMins = timeframeMins;

    g_pendingRequests[msgId] = req;

    LeaveCriticalSection(&g_cs_history);
}

bool WaitForHistoryData(const std::string& msgId, int timeoutMs, int* outReceived) {
    ULONGLONG startTime = GetTickCount64();

    while (GetTickCount64() - startTime < (ULONGLONG)timeoutMs) {
        EnterCriticalSection(&g_cs_history);

        auto it = g_pendingRequests.find(msgId);
        if (it != g_pendingRequests.end() && it->second.completed) {
            int receivedTicks = it->second.receivedTicks;
            g_pendingRequests.erase(it);
            LeaveCriticalSection(&g_cs_history);

            if (outReceived) {
                *outReceived = receivedTicks;
            }
            char msg[128];
            sprintf_s(msg, "History data completed: %d ticks received", receivedTicks);
            Utils::LogToFile("HISTORY_COMPLETE", msg);
            return true;
        }

        LeaveCriticalSection(&g_cs_history);
        Sleep(50);
    }

    EnterCriticalSection(&g_cs_history);
    g_pendingRequests.erase(msgId);
    LeaveCriticalSection(&g_cs_history);

    if (outReceived) {
        *outReceived = 0;
    }
    Utils::LogToFile("HISTORY_TIMEOUT", "Historical data request timed out");
    return false;
}

HistoryRequest* GetPendingRequest(const std::string& msgId) {
    auto it = g_pendingRequests.find(msgId);
    return (it != g_pendingRequests.end()) ? &it->second : nullptr;
}

} // namespace History