#pragma once

// PayloadType enum - verified from official proto files
// https://github.com/spotware/openapi-proto-messages/blob/main/OpenApiModelMessages.proto

enum class PayloadType : int {
    // Common
    HeartbeatEvent          = 51,

    // Auth
    ApplicationAuthReq      = 2100,
    ApplicationAuthRes      = 2101,
    AccountAuthReq          = 2102,
    AccountAuthRes          = 2103,
    VersionReq              = 2104,
    VersionRes              = 2105,

    // Trading
    NewOrderReq             = 2106,
    TrailingSLChangedEvent  = 2107,
    CancelOrderReq          = 2108,
    AmendOrderReq           = 2109,
    AmendPositionSltpReq    = 2110,
    ClosePositionReq        = 2111,

    // Symbols & Assets
    AssetListReq            = 2112,
    AssetListRes            = 2113,
    SymbolsListReq          = 2114,
    SymbolsListRes          = 2115,
    SymbolByIdReq           = 2116,
    SymbolByIdRes           = 2117,
    SymbolsForConversionReq = 2118,
    SymbolsForConversionRes = 2119,
    SymbolChangedEvent      = 2120,

    // Account
    TraderReq               = 2121,
    TraderRes               = 2122,
    TraderUpdateEvent       = 2123,

    // Reconcile (position sync)
    ReconcileReq            = 2124,
    ReconcileRes            = 2125,

    // Execution
    ExecutionEvent          = 2126,

    // Spot Quotes
    SubscribeSpotsReq       = 2127,
    SubscribeSpotsRes       = 2128,
    UnsubscribeSpotsReq     = 2129,
    UnsubscribeSpotsRes     = 2130,
    SpotEvent               = 2131,

    // Order Error
    OrderErrorEvent         = 2132,

    // Deals
    DealListReq             = 2133,
    DealListRes             = 2134,

    // Live Trendbars
    SubscribeLiveTrendbarReq   = 2135,
    UnsubscribeLiveTrendbarReq = 2136,

    // Historical Trendbars
    GetTrendbarsReq         = 2137,
    GetTrendbarsRes         = 2138,

    // Margin
    ExpectedMarginReq       = 2139,
    ExpectedMarginRes       = 2140,
    MarginChangedEvent      = 2141,

    // Error
    ErrorRes                = 2142,

    // Cash Flow
    CashFlowHistoryListReq  = 2143,
    CashFlowHistoryListRes  = 2144,

    // Tick Data
    GetTickDataReq          = 2145,
    GetTickDataRes          = 2146,

    // Token/Connection Events
    AccountsTokenInvalidatedEvent = 2147,
    ClientDisconnectEvent         = 2148,

    // Account Listing
    GetAccountsByAccessTokenReq = 2149,
    GetAccountsByAccessTokenRes = 2150,
    GetCtidProfileByTokenReq    = 2151,
    GetCtidProfileByTokenRes    = 2152,

    // Asset Classes
    AssetClassListReq       = 2153,
    AssetClassListRes       = 2154,

    // Depth of Market
    DepthEvent              = 2155,
    SubscribeDepthQuotesReq = 2156,
    SubscribeDepthQuotesRes = 2157,
    UnsubscribeDepthQuotesReq = 2158,
    UnsubscribeDepthQuotesRes = 2159,

    // Symbol Categories
    SymbolCategoryReq       = 2160,
    SymbolCategoryRes       = 2161,

    // Account Logout
    AccountLogoutReq        = 2162,
    AccountLogoutRes        = 2163,
    AccountDisconnectEvent  = 2164,

    // Live Trendbar Responses
    SubscribeLiveTrendbarRes   = 2165,
    UnsubscribeLiveTrendbarRes = 2166,

    // Margin Calls
    MarginCallListReq       = 2167,
    MarginCallListRes       = 2168,
    MarginCallUpdateReq     = 2169,
    MarginCallUpdateRes     = 2170,
    MarginCallUpdateEvent   = 2171,
    MarginCallTriggerEvent  = 2172,

    // Token Refresh
    RefreshTokenReq         = 2173,
    RefreshTokenRes         = 2174,

    // Order List
    OrderListReq            = 2175,
    OrderListRes            = 2176,

    // Dynamic Leverage (NOT PositionList!)
    GetDynamicLeverageReq   = 2177,
    GetDynamicLeverageRes   = 2178,

    // Deal/Order by Position
    DealListByPositionIdReq    = 2179,
    DealListByPositionIdRes    = 2180,
    OrderDetailsReq            = 2181,
    OrderDetailsRes            = 2182,
    OrderListByPositionIdReq   = 2183,
    OrderListByPositionIdRes   = 2184,

    // Deal Offset
    DealOffsetListReq       = 2185,
    DealOffsetListRes       = 2186,

    // Unrealized PnL
    GetPositionUnrealizedPnLReq = 2187,
    GetPositionUnrealizedPnLRes = 2188,
};

constexpr int ToInt(PayloadType pt) { return static_cast<int>(pt); }

// TradeSide
enum class TradeSide : int { Buy = 1, Sell = 2 };

// OrderType
enum class OrderType : int {
    Market = 1, Limit = 2, Stop = 3,
    StopLossTakeProfit = 4, MarketRange = 5, StopLimit = 6
};

// ExecutionType
enum class ExecutionType : int {
    OrderAccepted = 2, OrderFilled = 3, OrderReplaced = 4,
    OrderCancelled = 5, OrderExpired = 6, OrderRejected = 7,
    OrderCancelRejected = 8, Swap = 9, DepositWithdraw = 10,
    OrderPartialFill = 11, BonusDepositWithdraw = 12
};

// TrendbarPeriod
enum class TrendbarPeriod : int {
    M1 = 1, M2 = 2, M3 = 3, M4 = 4, M5 = 5,
    M10 = 6, M15 = 7, M30 = 8,
    H1 = 9, H4 = 10, H12 = 11,
    D1 = 12, W1 = 13, MN1 = 14
};

namespace Protocol {

// Build a JSON message string with payloadType and payload
// Returns allocated string in static buffer
const char* BuildMessage(const char* clientMsgId, PayloadType type, const char* payloadJson);

// Extract payloadType from a JSON message buffer
int ExtractPayloadType(const char* buffer);

// Check if buffer contains a specific payloadType
bool ContainsPayloadType(const char* buffer, PayloadType type);

// Extract a string field value from JSON (returns pointer into static buffer)
const char* ExtractString(const char* buffer, const char* fieldName);

// Extract an int64 field value from JSON
long long ExtractInt64(const char* buffer, const char* fieldName);

// Extract a double field value from JSON
double ExtractDouble(const char* buffer, const char* fieldName);

// Extract a bool field value from JSON
bool ExtractBool(const char* buffer, const char* fieldName);

// Extract an integer field value from JSON
int ExtractInt(const char* buffer, const char* fieldName);

// Extract a JSON array as string (returns the raw [...] content)
const char* ExtractArray(const char* buffer, const char* fieldName);

// Count elements in a JSON array string
int CountArrayElements(const char* arrayStr);

// Get the Nth element from a JSON array (0-based), returns {...} or value
const char* GetArrayElement(const char* arrayStr, int index);

// Check if a field exists in JSON buffer
bool HasField(const char* buffer, const char* fieldName);

} // namespace Protocol
