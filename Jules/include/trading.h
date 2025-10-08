#pragma once

namespace Trading {

// Place market order with stop loss and take profit support
int PlaceOrder(const char* symbol, int amount, double stopDist, double limit, double* pPrice, int* pFill);

// Close position (full or partial)
int ClosePosition(int tradeId, int amount);

// Get trade information
int GetTradeInfo(int tradeId, double* pOpen, double* pClose, double* pRoll, double* pProfit);

} // namespace Trading