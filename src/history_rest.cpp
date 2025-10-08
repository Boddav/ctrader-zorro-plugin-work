#include "../include/history_rest.h"
#include "../include/http_api.h"
#include "../include/utils.h"
#include "../include/nlohmann/json.hpp"
#include "../include/symbols.h"
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

namespace HistoryRest {

// Helper function to map Zorro timeframe in minutes to cTrader API string
static std::string MapZorroTimeframeToCtrader(int minutes) {
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
        case 30: return "m30";
        case 60: return "h1";
        case 240: return "h4";
        case 720: return "h12";
        case 1440: return "d1";
        case 10080: return "w1";
        // Note: Monthly timeframe is not supported by a fixed number of minutes
        default: return "";
    }
}

// Main function implementation
int GetHistoryRest(
    const std::string& symbol,
    int timeframe_minutes,
    long long from_ts_seconds,
    long long to_ts_seconds,
    const std::string& token,
    long long ctid_account_id) {

    SymbolInfo* symbol_info = Symbols::GetSymbol(symbol.c_str());
    if (!symbol_info) {
        Utils::LogToFile("HISTORY_REST", "Error: Symbol not found.");
        return 0;
    }
    long long symbolId = symbol_info->id;
    int digits = symbol_info->digits;

    std::string period = MapZorroTimeframeToCtrader(timeframe_minutes);
    if (period.empty()) {
        Utils::LogToFile("HISTORY_REST", "Error: Unsupported timeframe.");
        return 0;
    }

    std::ofstream csv_file("ctrader_history.csv");
    if (!csv_file.is_open()) {
        Utils::LogToFile("HISTORY_REST", "Error: Could not open ctrader_history.csv for writing.");
        return 0;
    }
    // Write header
    csv_file << "Timestamp,Open,High,Low,Close,Volume\n";

    long long current_from_ts = from_ts_seconds * 1000;
    long long to_ts_ms = to_ts_seconds * 1000;
    int total_bars_downloaded = 0;
    const int max_bars_per_request = 1000;

    while (current_from_ts < to_ts_ms) {
        char url[512];
        sprintf_s(url, sizeof(url),
                  "https://api.ctraderapi.com/v1/history/bars?symbolId=%lld&period=%s&from=%lld&to=%lld&count=%d&ctidTraderAccountId=%lld",
                  symbolId, period.c_str(), current_from_ts, to_ts_ms, max_bars_per_request, ctid_account_id);

        char auth_header[256];
        sprintf_s(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token.c_str());

        std::string response;
        int retries = 0;
        const int max_retries = 5;

        while(retries < max_retries) {
            response = HttpApi::HttpRequest(url, nullptr, auth_header, "GET");

            if (response.find("\"errorCode\":\"INVALID_OAUTH_TOKEN\"") != std::string::npos) {
                 Utils::LogToFile("HISTORY_REST", "OAuth token invalid, attempting refresh is needed (not implemented in this function).");
                 // In a real scenario, you would trigger a token refresh here.
                 return 0;
            }

            if (response.find("429 Too Many Requests") != std::string::npos) {
                long long backoff_ms = (long long)(200 * pow(2, retries));
                Utils::LogToFile("HISTORY_REST", "Rate limit hit, backing off for %lld ms.", backoff_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
                retries++;
                continue;
            }
            break;
        }

        if (response.empty()) {
            Utils::LogToFile("HISTORY_REST", "Error: No response from server.");
            break;
        }

        try {
            json j = json::parse(response);
            if (j.contains("data")) {
                j = j["data"];
            }

            if (!j.is_array() || j.empty()) {
                Utils::LogToFile("HISTORY_REST", "No more bars in response.");
                break;
            }

            std::vector<Bar> bars;
            long long last_timestamp = 0;

            for (const auto& item : j) {
                Bar bar;
                bar.timestamp = item.value("timestamp", 0LL);
                last_timestamp = bar.timestamp;

                if (item.contains("low") && item.contains("deltaOpen")) { // Relative prices
                    double low = item["low"].get<long long>() / pow(10, digits);
                    bar.open = (item["low"].get<long long>() + item["deltaOpen"].get<int>()) / pow(10, digits);
                    bar.high = (item["low"].get<long long>() + item["deltaHigh"].get<int>()) / pow(10, digits);
                    bar.low = low;
                    bar.close = (item["low"].get<long long>() + item["deltaClose"].get<int>()) / pow(10, digits);
                } else { // Absolute prices
                    bar.open = item.value("open", 0.0);
                    bar.high = item.value("high", 0.0);
                    bar.low = item.value("low", 0.0);
                    bar.close = item.value("close", 0.0);
                }

                bar.volume = item.value("volume", item.value("tickVolume", 0.0));
                bars.push_back(bar);
            }

            // The API returns bars in descending order (newest first). We need to reverse them for chronological processing.
            std::reverse(bars.begin(), bars.end());

            for(const auto& bar : bars) {
                csv_file << bar.timestamp / 1000 << "," << bar.open << "," << bar.high << "," << bar.low << "," << bar.close << "," << bar.volume << "\n";
            }

            total_bars_downloaded += bars.size();

            if (bars.size() < max_bars_per_request) {
                break; // No more data to fetch
            }

            current_from_ts = last_timestamp + 1;

        } catch (json::parse_error& e) {
            Utils::LogToFile("HISTORY_REST", "JSON parse error: %s", e.what());
            Utils::LogToFile("HISTORY_REST_RESPONSE", response.c_str());
            break;
        }
    }

    csv_file.close();
    return total_bars_downloaded;
}

// Implementation for loading bars from CSV
int LoadBarsFromCsv(const std::string& filepath, T6* ticks, int max_ticks) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Utils::LogToFile("HISTORY_REST", "Error: Cannot open CSV file for reading.");
        return 0;
    }

    std::string line;
    // Skip header
    std::getline(file, line);

    int count = 0;
    while (std::getline(file, line) && count < max_ticks) {
        std::stringstream ss(line);
        std::string value;

        long long timestamp_sec;
        double open, high, low, close, volume;

        std::getline(ss, value, ','); timestamp_sec = std::stoll(value);
        std::getline(ss, value, ','); open = std::stod(value);
        std::getline(ss, value, ','); high = std::stod(value);
        std::getline(ss, value, ','); low = std::stod(value);
        std::getline(ss, value, ','); close = std::stod(value);
        std::getline(ss, value, ','); volume = std::stod(value);

        ticks[count].time = (double)timestamp_sec / (24. * 60. * 60.) + 25569.; // OLE Automation date
        ticks[count].open = open;
        ticks[count].high = high;
        ticks[count].low = low;
        ticks[count].close = close;
        ticks[count].vol = volume;

        count++;
    }

    // Zorro expects data to be reversed (oldest first)
    std::reverse(ticks, ticks + count);

    return count;
}


} // namespace HistoryRest