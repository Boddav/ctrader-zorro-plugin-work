#pragma once

namespace Trading {

// BrokerBuy2 implementation: open position or place pending order
// Returns: >0 = zorroId (filled), <0 = -zorroId (pending), 0 = error
int BuyOrder(const char* asset, int amount, double stopDist, double limit,
             double* pPrice, int* pFill);

// BrokerSell2 implementation: close or reduce position
// Returns: tradeId if still open, 0 if fully closed
int SellOrder(int tradeId, int amount, double limit,
              double* pClose, double* pCost, double* pProfit, int* pFill);

// BrokerTrade implementation: get trade status (no network call)
// Returns: signed Zorro amount (lots), or 0 if not found/closed
int GetTradeStatus(int tradeId, double* pOpen, double* pClose,
                   double* pCost, double* pProfit);

// Called from NetworkThread when ExecutionEvent (2126) arrives
void HandleExecutionEvent(const char* buffer, int bufLen);

// Called from NetworkThread when OrderErrorEvent (2132) arrives
void HandleOrderErrorEvent(const char* buffer, int bufLen);

// Called from NetworkThread when ReconcileRes (2125) arrives
void HandleReconcileRes(const char* buffer);

// Request position reconciliation (called after login)
bool RequestReconcile();

// Cancel a pending order (DO_CANCEL)
bool CancelOrder(int tradeId);

// Amend SL/TP on an existing open position (M5)
bool AmendPositionSltp(int tradeId, double stopLoss, double takeProfit);

} // namespace Trading
