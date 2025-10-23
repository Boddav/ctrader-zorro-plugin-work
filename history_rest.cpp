#include "../include/history_rest.h"
#include "../include/globals.h"
#include "../include/http_api.h"
#include "../include/symbols.h"
#include "../include/utils.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace HistoryRest {

namespace {
constexpr double DAYS_1900_TO_1970 = 25569.0;
constexpr const char* CTRADER_REST_HISTORY_URL = "https://api.ctraderapi.com/v1/history/bars";
constexpr int REST_HISTORY_CHUNK_LIMIT = 1000;
constexpr int REST_RATE_LIMIT_DELAY_MS = 250;

std::string TimeframeToRestPeriod(int minutes) {
    switch (minutes) {
    case 1: return "m1";
    case 2: return "m2";
    case 3: return "m3";
    case 4: return "m4";
    case 5: return "m5";
    case 6: return "m6";
    case 7: return "m7";
    case 8: return "m8";
    case 9: return "m9";
    case 10: return "m10";
    case 15: return "m15";
    case 20: return "m20";
    case 30: return "m30";
    case 45: return "m45";
    case 60: return "h1";
    case 120: return "h2";
    case 180: return "h3";
    case 240: return "h4";
    case 360: return "h6";
    case 480: return "h8";
    case 720: return "h12";
    case 1440: return "d1";
    case 10080: return "w1";
    case 43200: return "mn1";
    default:
        Utils::LogToFile("HISTORY_REST_DEBUG", "Unknown timeframe for REST period; defaulting to h1");
        return "h1";
    }
}

int TimeframeMinutesToSeconds(int minutes) {
    if (minutes <= 0) {
        return 60;
    }
    return minutes * 60;
}

struct ParsedBar {
    long long timestampSec{};
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
};

bool ExtractJsonLongLong(const std::string& source, const char* key, long long& outValue) {
    size_t pos = source.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    pos += std::strlen(key);
    while (pos < source.size() && (source[pos] == ' ' || source[pos] == ':' || source[pos] == '"')) {
        ++pos;
    }
    const char* start = source.c_str() + pos;
    char* endPtr = nullptr;
    long long value = _strtoi64(start, &endPtr, 10);
    if (!endPtr || endPtr == start) {
        return false;
    }
    outValue = value;
    return true;
}

bool ExtractJsonDouble(const std::string& source, const char* key, double& outValue) {
    size_t pos = source.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    pos += std::strlen(key);
    while (pos < source.size() && (source[pos] == ' ' || source[pos] == ':' || source[pos] == '"')) {
        ++pos;
    }
    const char* start = source.c_str() + pos;
    char* endPtr = nullptr;
    double value = std::strtod(start, &endPtr);
    if (!endPtr || endPtr == start) {
        return false;
    }
    outValue = value;
    return true;
}

std::string ExtractDataArray(const std::string& response) {
    size_t dataPos = response.find("\"data\"");
    size_t arrayStart = std::string::npos;
    if (dataPos != std::string::npos) {
        arrayStart = response.find('[', dataPos);
    }
    if (arrayStart == std::string::npos) {
        arrayStart = response.find('[');
    }
    if (arrayStart == std::string::npos) {
        return {};
    }

    int depth = 0;
    size_t cursor = arrayStart;
    for (; cursor < response.size(); ++cursor) {
        if (response[cursor] == '[') {
            ++depth;
        } else if (response[cursor] == ']') {
            --depth;
            if (depth == 0) {
                return response.substr(arrayStart, cursor - arrayStart + 1);
            }
        }
    }
    return {};
}

std::vector<ParsedBar> ParseBars(const std::string& payload, int digits) {
    std::vector<ParsedBar> bars;
    if (payload.empty()) {
        return bars;
    }

    const double scale = 100000.0; // cTrader API prices are always scaled by 100,000

    size_t cursor = 0;
    while (cursor < payload.size()) {
        size_t objStart = payload.find('{', cursor);
        if (objStart == std::string::npos) {
            break;
        }
        int braceDepth = 1;
        size_t i = objStart + 1;
        while (i < payload.size() && braceDepth > 0) {
            if (payload[i] == '{') {
                ++braceDepth;
            } else if (payload[i] == '}') {
                --braceDepth;
            }
            ++i;
        }
        if (braceDepth != 0) {
            break;
        }
        size_t objEnd = i;
        std::string obj = payload.substr(objStart, objEnd - objStart);

        long long timestampSec = 0;
        if (!ExtractJsonLongLong(obj, "\"timestamp\"", timestampSec)) {
            ExtractJsonLongLong(obj, "\"utcTimestamp\"", timestampSec);
        }
        if (timestampSec == 0) {
            cursor = objEnd;
            continue;
        }
        if (timestampSec > 100000000000LL) {
            timestampSec /= 1000;
        }

        // Parse prices as INTEGERS (long long), not doubles!
        // cTrader API sends prices as integers scaled by 100,000
        long long lowVal = 0;
        long long deltaOpen = 0, deltaHigh = 0, deltaClose = 0;
        bool hasLow = ExtractJsonLongLong(obj, "\"low\"", lowVal);
        bool hasDeltaOpen = ExtractJsonLongLong(obj, "\"deltaOpen\"", deltaOpen);
        bool hasDeltaHigh = ExtractJsonLongLong(obj, "\"deltaHigh\"", deltaHigh);
        bool hasDeltaClose = ExtractJsonLongLong(obj, "\"deltaClose\"", deltaClose);

        double openVal = 0.0, highVal = 0.0, closeVal = 0.0, volumeVal = 0.0;
        bool hasOpen = ExtractJsonDouble(obj, "\"open\"", openVal);
        bool hasHigh = ExtractJsonDouble(obj, "\"high\"", highVal);
        bool hasClose = ExtractJsonDouble(obj, "\"close\"", closeVal);
        bool hasVolume = ExtractJsonDouble(obj, "\"volume\"", volumeVal);
        if (!hasVolume) {
            hasVolume = ExtractJsonDouble(obj, "\"tickVolume\"", volumeVal);
        }

        ParsedBar bar{};
        bar.timestampSec = timestampSec;

        if (hasLow && hasDeltaOpen && hasDeltaHigh && hasDeltaClose) {
            // INTEGER arithmetic first (like Python reference), then divide by 100,000
            // Python ref: df2['open'] = (df['low'] + df['deltaOpen']) / 100_000
            bar.low = (double)lowVal / scale;
            bar.open = (double)(lowVal + deltaOpen) / scale;
            bar.high = (double)(lowVal + deltaHigh) / scale;
            bar.close = (double)(lowVal + deltaClose) / scale;
        } else {
            if (scale > 1.0) {
                bar.open = hasOpen ? openVal / scale : 0.0;
                bar.high = hasHigh ? highVal / scale : 0.0;
                bar.low = hasLow ? lowVal / scale : 0.0;
                bar.close = hasClose ? closeVal / scale : 0.0;
            } else {
                bar.open = hasOpen ? openVal : 0.0;
                bar.high = hasHigh ? highVal : 0.0;
                bar.low = hasLow ? lowVal : 0.0;
                bar.close = hasClose ? closeVal : 0.0;
            }
        }

        bar.volume = hasVolume ? volumeVal : 0.0;
        bars.push_back(bar);
        cursor = objEnd;
    }

    return bars;
}

} // namespace

int FetchHistoryRest(SymbolInfo* info,
                     long long startMs,
                     long long endMs,
                     int timeframeMins,
                     int maxTicks,
                     T6* buffer) {
    if (!info || !buffer || maxTicks <= 0) {
        return 0;
    }

    Utils::LogToFile("HISTORY_REST", "FetchHistoryRest CALLED");

    // Build cTrader REST API timeframe string
    std::string period = TimeframeToRestPeriod(timeframeMins);
    if (period.empty()) {
        Utils::LogToFile("HISTORY_REST_ERROR", "Unsupported timeframe");
        return 0;
    }

    int totalBars = 0;
    long long currentFromMs = startMs;
    const int maxBarsPerRequest = REST_HISTORY_CHUNK_LIMIT; // 1000

    // CHUNK-BASED ITERATION (like Jules version)
    while (currentFromMs < endMs && totalBars < maxTicks) {
        int remainingCapacity = maxTicks - totalBars;
        int requestCount = std::min(remainingCapacity, maxBarsPerRequest);

        char url[768];
        sprintf_s(url, sizeof(url),
            "https://api.ctraderapi.com/v1/history/bars?symbolId=%lld&period=%s&from=%lld&to=%lld&count=%d&ctidTraderAccountId=%lld",
            info->id, period.c_str(), currentFromMs, endMs, requestCount, G.CTraderAccountId);

        char authHeader[512];
        sprintf_s(authHeader, sizeof(authHeader), "Authorization: Bearer %s", G.Token);

        Utils::LogToFile("HISTORY_REST_REQUEST", url);

        // Make HTTP GET request with RETRY and RATE LIMITING (like Jules)
        std::string response;
        int retries = 0;
        const int maxRetries = 5;

        while (retries < maxRetries) {
            response = HttpApi::HttpRequest(url, nullptr, authHeader, "GET");

            // Check for rate limiting (429)
            if (response.find("429") != std::string::npos || response.find("Too Many Requests") != std::string::npos) {
                long long backoffMs = (long long)(200 * pow(2.0, retries));
                char backoffMsg[128];
                sprintf_s(backoffMsg, "Rate limit hit, backing off %lld ms", backoffMs);
                Utils::LogToFile("HISTORY_REST_RATELIMIT", backoffMsg);
                Sleep((DWORD)backoffMs);
                retries++;
                continue;
            }

            // Check for invalid token
            if (response.find("INVALID_OAUTH_TOKEN") != std::string::npos) {
                Utils::LogToFile("HISTORY_REST_ERROR", "OAuth token invalid - needs refresh");
                return totalBars; // Return what we have so far
            }

            break; // Success
        }

        if (response.empty()) {
            Utils::LogToFile("HISTORY_REST_ERROR", "Empty response from server");
            break;
        }

        // Check for other errors
        if (response.find("\"errorCode\":") != std::string::npos) {
            Utils::LogToFile("HISTORY_REST_ERROR", response.c_str());
            break;
        }

        // Extract data array from JSON response
        std::string dataArray = ExtractDataArray(response);
        if (dataArray.empty()) {
            Utils::LogToFile("HISTORY_REST_ERROR", "No more data in response");
            break;
        }

        // Parse bars
        std::vector<ParsedBar> bars = ParseBars(dataArray, info->digits);
        if (bars.empty()) {
            Utils::LogToFile("HISTORY_REST_INFO", "No bars parsed - end of data");
            break;
        }

        // Convert to Zorro T6 format
        long long lastTimestamp = 0;
        for (size_t i = 0; i < bars.size() && totalBars < maxTicks; ++i) {
            const ParsedBar& bar = bars[i];
            lastTimestamp = bar.timestampSec * 1000LL; // Store in milliseconds

            // Convert Unix timestamp (seconds) to Zorro DATE
            double zorroTime = DAYS_1900_TO_1970 + (bar.timestampSec / (24.0 * 60.0 * 60.0));

            buffer[totalBars].time = zorroTime;
            buffer[totalBars].fOpen = bar.open;
            buffer[totalBars].fHigh = bar.high;
            buffer[totalBars].fLow = bar.low;
            buffer[totalBars].fClose = bar.close;
            buffer[totalBars].fVal = bar.volume;

            totalBars++;
        }

        // Check if we got fewer bars than requested - means no more data
        if ((int)bars.size() < requestCount) {
            Utils::LogToFile("HISTORY_REST_INFO", "Received fewer bars than requested - end of data");
            break;
        }

        // Move to next chunk (lastTimestamp + 1ms to avoid duplicates)
        currentFromMs = lastTimestamp + 1;

        // Rate limit between chunks
        Sleep(REST_RATE_LIMIT_DELAY_MS); // 250ms
    }

    char msg[256];
    sprintf_s(msg, "REST API returned %d bars for symbol ID %lld (requested %d)", totalBars, info->id, maxTicks);
    Utils::LogToFile("HISTORY_REST_SUCCESS", msg);

    return totalBars;
}

} // namespace HistoryRest
