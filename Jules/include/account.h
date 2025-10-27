#pragma once

namespace Account {

// Request fresh account information from server
bool RequestAccountInfo();

// Update account info (called from NetworkThread when response received)
void UpdateAccountInfo(double balance, double equity, double margin, const char* currency);

// Get current account data (automatically refreshes if stale)
bool GetAccountData(double* pBalance, double* pEquity, double* pMargin, double* pFreeMargin, char* currency = nullptr);

// Quick access functions
double GetBalance();
double GetEquity();
double GetMargin();
double GetFreeMargin();

} // namespace Account