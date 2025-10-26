#pragma once

namespace Trading {

// Close filter types for batch position closing
enum class CloseFilter {
    AllShort,        // Close all short positions
    AllLong,         // Close all long positions
    AllProfitable,   // Close all positions with profit > 0
    AllLosing        // Close all positions with profit < 0
};

// Place market order with stop loss and take profit support
int PlaceOrder(const char* symbol, int amount, double stopDist, double limit, double* pPrice, int* pFill);

// Close position (full or partial)
int ClosePosition(int tradeId, int amount);

// Close multiple positions based on filter criteria
bool ClosePositionsByFilter(CloseFilter filter);

// Get trade information
int GetTradeInfo(int tradeId, double* pOpen, double* pClose, double* pRoll, double* pProfit);

} // namespace Trading