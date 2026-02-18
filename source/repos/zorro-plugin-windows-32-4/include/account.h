#pragma once

namespace Account {

// Request trader account info (balance, equity, etc.)
bool RequestTraderInfo();

// Process TraderRes (2122)
void HandleTraderRes(const char* buffer);

// Process MarginChangedEvent (2141)
void HandleMarginChangedEvent(const char* buffer);

// Process TraderUpdateEvent (2123)
void HandleTraderUpdateEvent(const char* buffer);

} // namespace Account
