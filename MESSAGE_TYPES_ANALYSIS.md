# cTrader OpenAPI Message Types - Used by Plugin

## Általunk használt üzenettípusok (payloadType)

### Authentication (2100-2105)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2100 | ApplicationAuthReq | Request | ✅ Yes | Working |
| 2101 | ApplicationAuthRes | Response | ✅ Yes | Working |
| 2102 | AccountAuthReq | Request | ✅ Yes | Working |
| 2103 | AccountAuthRes | Response | ✅ Yes | Working |
| 2104 | VersionReq | Request | ❌ No | Still unused for keep-alive |
| 2105 | VersionRes | Response | ✅ Yes | Used as keep-alive ack (logged in `NetworkThread`) |

### Trading Operations (2106-2111)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2106 | NewOrderReq | Request | ✅ Yes | Working (order flow aligned; heartbeat swap pending) |
| 2108 | CancelOrderReq | Request | ✅ Yes | Working (used by `CancelOrder`) |
| 2109 | AmendOrderReq | Request | ❌ No | Not implemented |
| 2110 | AmendPositionSltpReq | Request | ✅ Yes | Working (SL/TP) |
| 2111 | ClosePositionReq | Request | ✅ Yes | Working |

### Market Data & Symbols (2112-2115)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2112 | AssetListReq | Request | ✅ Yes | Working |
| 2113 | AssetListRes | Response | ✅ Yes | Working |
| 2114 | SymbolsListReq | Request | ✅ Yes | Fallback fetch when cache miss |
| 2115 | SymbolsListRes | Response | ✅ Yes | Parsed on cache miss |

### Account Info (2121-2122)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2121 | TraderReq | Request | ✅ Yes | Working (throttled; duplicate issue resolved) |
| 2122 | TraderRes | Response | ✅ Yes | Working |

### Events (2126, 2132, 2141, 2147-2148, 2164)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2126 | ExecutionEvent | Event | ✅ Yes | Working |
| 2132 | OrderErrorEvent | Event | ✅ Yes | Working |
| 2141 | MarginChangedEvent | Event | ✅ Yes | Working |
| 2142 | ErrorRes | Response | ✅ Yes | Working |
| 2147 | TokenInvalidatedEvent | Event | ✅ Yes | Triggers token refresh |
| 2148 | ClientDisconnectEvent | Event | ✅ Yes | Logged & resets reconnect guard |
| 2164 | AccountDisconnectEvent | Event | ✅ Yes | Logged & reconnect-safe |

### Spot Price Subscription (2127-2131)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2127 | SpotSubscribeReq | Request | ✅ Yes | Working (de-duplicated in `EnsureSubscribed`) |
| 2128 | SpotSubscribeRes | Response | ✅ Yes | Working |
| 2129 | SpotUnsubscribeReq | Request | ✅ Yes | Working (used by depth/asset cleanup) |
| 2130 | SpotUnsubscribeRes | Response | ✅ Yes | Working |
| 2131 | SpotEvent | Event | ✅ Yes | Working (price updates) |

### Historical Data (2133-2138)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2133 | DealListReq | Request | ❌ No | Not implemented |
| 2134 | DealListRes | Response | ❌ No | Not implemented |
| 2135 | SubscribeLiveTrendbarReq | Request | ❌ No | Not implemented |
| 2136 | UnsubscribeLiveTrendbarReq | Request | ❌ No | Not implemented |
| 2137 | GetTrendbarsReq | Request | ✅ Yes | Primary WebSocket history path (cap at 1000 bars, REST fallback on timeout) |
| 2138 | GetTrendbarsRes | Response | ✅ Yes | Parsed in `History::ProcessHistoricalResponse` (trendbar + legacy payload support) |

### Tick Data (2145-2146)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2145 | GetTickdataReq | Request | ✅ Yes | Working |
| 2146 | GetTickdataRes | Response | ✅ Yes | Working |

### Asset Classes & Categories (2153-2154, 2160-2161)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2153 | AssetClassReq | Request | ✅ Yes | Working |
| 2154 | AssetClassRes | Response | ✅ Yes | Working |
| 2160 | SymbolCategoryReq | Request | ✅ Yes | Working |
| 2161 | SymbolCategoryRes | Response | ✅ Yes | Working |

### Depth Quotes (2155-2159)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2155 | SubscribeDepthQuotesReq | Request | ✅ Yes | Working (symbol depth manager) |
| 2156 | SubscribeDepthQuotesRes | Response | ✅ Yes | Working |
| 2157 | UnsubscribeDepthQuotesReq | Request | ✅ Yes | Working |
| 2158 | UnsubscribeDepthQuotesRes | Response | ✅ Yes | Working |
| 2159 | DepthEvent | Event | ✅ Yes | Working (delta & full updates) |

### Order/Position Snapshots (2175-2176)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2175 | OrderListReq | Request | ✅ Yes | Working |
| 2176 | OrderListRes | Response | ✅ Yes | Working |
| 2177 | PositionListReq | Request | ❌ No | **REMOVED (requires leverageId)** |

### Position Reconciliation (2187-2188)
| PayloadType | Name | Request/Response | Used? | Status |
|-------------|------|------------------|-------|--------|
| 2187 | ReconcileReq | Request | ✅ Yes | Working (throttled to 30s intervals) |
| 2188 | ReconcileRes | Response | ✅ Yes | Working (updates Trade.grossUnrealizedPnL and netUnrealizedPnL) |

**Purpose**: Request server-side position unrealized P&L calculations
**Implementation**:
- Request sent by `Account::RequestReconcile()` every 30 seconds
- Response parsed in `NetworkThread` (main.cpp:262-341)
- Updates `Trade.grossUnrealizedPnL` and `Trade.netUnrealizedPnL` for all open positions
- More accurate than client-side calculations

---

## PROBLÉMÁK

### 1. Duplikált TraderReq (2121) hívások
**Tünetek**:
- Ugyanaz a `msg_XXX` ID kétszer küldhető el
- Például: `msg_162` kétszer küldeödött 2121-es payloadType-pal

**Log bizonyíték**:
```
!WS SEND: {"clientMsgId":"msg_162","payloadType":2121,...}
!WS SEND: {"clientMsgId":"msg_162","payloadType":2121,...}
```

**Lehetséges okok**:
- `BrokerAccount()` több szálból/helyről hívódik
- Retry logika újrahívja
- TraderReq spam throttling nem megfelelő

**Ellenőrizendő fájlok**:
- `src/account.cpp` - TraderReq hívások
- `src/main.cpp` - BrokerAccount hívások

### 2. Duplikált SpotSubscribeReq (2127) hívások
**Tünetek**:
- Ugyanaz a symbolId-re többször történik feliratkozás
- Például: `symbolId:[5]` (AUDUSD) háromszor egymás után

**Log bizonyíték**:
```
!WS SEND: {"clientMsgId":"msg_155","payloadType":2127,"payload":{..."symbolId":[5]}}
!WS SEND: {"clientMsgId":"msg_155","payloadType":2127,"payload":{..."symbolId":[5]}}
```

**Lehetséges okok**:
- `EnsureSubscribed()` többször hívódik ugyanarra a szimbólumra
- `BrokerAsset()` minden híváskor próbál feliratkozni
- Nincs megfelelő ellenőrzés hogy már feliratkozott-e

**Ellenőrizendő fájlok**:
- `src/symbols.cpp` - EnsureSubscribed, SubscribeToSpotPrices
- `src/main.cpp` - BrokerAsset

### 3. GetTrendbarsReq (2137) timeout
**Tünetek**:
- 10 másodperc timeout történik
- Szerver nem válaszol

**OK**: Túl hosszú időtartomány (19 hónap)

**JAVÍTVA**: ✅ 90 napos limit hozzáadva

---

## KÖVETKEZŐ LÉPÉSEK

1. **Legacy heartbeat lecserélése**
   - `NetworkThread` jelenleg 2106-os pongot küld; állítsuk át 2104/2148 alapú keep-alive-ra.

2. **Történeti adatkérések migrálása**
   - `History::RequestHistoricalData` továbbra is 2112/2113-as (AssetList) sémát küldi.
   - Vezessük át a teljes modult 2137/2138 (trendbar) és 2145/2146 (tick) payloadokra.

3. **Margin & analytics bővítés**
   - `MarginChangedEvent` már naplózott, de további kockázati metrikákat kell szinkronizálni.
   - Integrálni kell a cash-flow / deal list üzenetcsaládot (2133–2144) a Zorro riporttal.
