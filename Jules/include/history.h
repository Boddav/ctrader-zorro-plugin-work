#pragma once
#include "../include/globals.h"
#include "../zorro.h"

namespace History {

// Forward declaration for critical section
extern CRITICAL_SECTION g_cs_history;

struct HistoryRequest {
    std::string symbol;
    std::string clientMsgId;
    T6* ticksBuffer;
    int maxTicks;
    int receivedTicks;
    bool completed;
    ULONGLONG requestTime;
};

// Request historical OHLC data
bool RequestHistoricalData(const char* symbol, DATE startTime, DATE endTime, int timeframeMins, int maxTicks, T6* ticks);

// Get tick-level data (1-minute resolution)
bool GetTickData(const char* symbol, DATE startTime, DATE endTime, int maxTicks, T6* ticks);

// Get bar data with specific timeframe
bool GetBarData(const char* symbol, DATE startTime, DATE endTime, int timeframeMins, int maxBars, T6* bars);

// Convert minute timeframe to cTrader enum
int ConvertTimeframeToEnum(int minutes);

// Process historical data response from server (called by NetworkThread)
void ProcessHistoricalResponse(const char* response);

// Store pending history request
void StorePendingRequest(const std::string& msgId, const char* symbol, T6* buffer, int maxTicks);

// Wait for history request completion
bool WaitForHistoryData(const std::string& msgId, int timeoutMs = 10000);

// Get pending request by message ID
HistoryRequest* GetPendingRequest(const std::string& msgId);

} // namespace History