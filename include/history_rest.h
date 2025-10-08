#pragma once

#include <string>
#include <vector>
#include "../zorro.h" // For T6 definition

namespace HistoryRest {

// Structure to hold a single bar of data from the REST API
struct Bar {
    long long timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
};

// Main function to download historical data via REST API and save to CSV
// Returns the number of bars downloaded, or 0 on failure.
int GetHistoryRest(
    const std::string& symbol,
    int timeframe_minutes,
    long long from_ts,
    long long to_ts,
    const std::string& token,
    long long ctid_account_id
);

// Helper function to load bars from a CSV file into Zorro's T6 format
// This will be called from BrokerHistory2 after GetHistoryRest succeeds.
int LoadBarsFromCsv(const std::string& filepath, T6* ticks, int max_ticks);

} // namespace HistoryRest