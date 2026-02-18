#pragma once
#include <string>

namespace Symbols {

// Request and process the full symbol list
bool RequestSymbolList();

// Request detailed info for symbols by ID (batched)
bool RequestSymbolDetails();

// Subscribe to spot quotes for a symbol
bool Subscribe(const char* symbolName);

// Batch resubscribe all previously subscribed symbols
void BatchResubscribe();

// Process incoming SpotEvent
void HandleSpotEvent(const char* buffer);

// Process SymbolsListRes
void HandleSymbolsListRes(const char* buffer);

// Process SymbolByIdRes
void HandleSymbolByIdRes(const char* buffer);

// Lookup symbol by name (thread-safe)
bool GetSymbol(const char* name, SymbolInfo& out);

// Lookup symbol name by ID
const char* GetNameById(long long symbolId);

} // namespace Symbols
