# cTrader Open API payload integration plan

## Summary
- **Official payload types:** 89 (2100–2188) according to Spotware's Open API documentation.
- **Handled in plugin today:** 19 payload IDs, extracted via `payloads.py` (spot + depth market-data stack now included).
- **Immediate issue:** Several legacy paths still use mismatched protocol IDs (2105/2106, 2108/2111, 2112/2137, 2113/2138) and must be realigned.
- **Missing coverage:** 70 payloads, including version negotiation, order amendments, streaming depth deltas beyond top-of-book, margin events, cash-flow history, and authentication refresh flows.
- **Goal:** Align the plugin's request/response handling with the official protocol and extend functionality to the missing message families.

## Current payload usage vs. official definitions
| ID  | Official name | Observed usage in repo | Status & corrective action |
|-----|---------------|-------------------------|----------------------------|
| 2100 | PROTO_OA_APPLICATION_AUTH_REQ | `BrokerLogin` app auth, reconnect | ✅ Correct. Keep as-is. |
| 2101 | PROTO_OA_APPLICATION_AUTH_RES | `BrokerLogin` response validation | ✅ Correct. |
| 2102 | PROTO_OA_ACCOUNT_AUTH_REQ | Account login in `BrokerLogin` / `Reconnect` | ✅ Correct. |
| 2103 | PROTO_OA_ACCOUNT_AUTH_RES | Account auth confirmation | ✅ Correct. |
| 2105 | PROTO_OA_VERSION_RES | Used by `Trading::PlaceOrder` to submit market orders | ❌ Should be 2106 (NEW_ORDER_REQ). Introduce dedicated request builder and add proper 2105 response handler in `main.cpp`. |
| 2106 | PROTO_OA_NEW_ORDER_REQ | Used as heartbeat/ping in `NetworkThread` | ❌ Heartbeat must use `PROTO_OA_VERSION_REQ`/`RES` or server-side ping logic. Reassign ID 2106 to order submission path. |
| 2108 | PROTO_OA_CANCEL_ORDER_REQ | Used to close open positions | ⚠️ Swap to 2111 (CLOSE_POSITION_REQ). Implement real cancel order support on top of 2108. |
| 2112 | PROTO_OA_ASSET_LIST_REQ | `Symbols::FetchAssetList` at login to hydrate `AssetList.txt` | ✅ Correct. Historical module still needs migration to 2137/2145 for candle/tick data. |
| 2113 | PROTO_OA_ASSET_LIST_RES | Parsed via new asset catalog builder and cached to `AssetList.txt` | ✅ Correct. Add history parser for 2138/2146 as part of data module rework. |
| 2114 | PROTO_OA_SYMBOLS_LIST_REQ | Fetch full symbol catalogue | ✅ Correct. |
| 2115 | PROTO_OA_SYMBOLS_LIST_RES | Parse symbol catalogue | ✅ Correct. |
| 2121 | PROTO_OA_TRADER_REQ | (a) Account info refresh in `Account::RequestAccountInfo`; (b) Legacy spot subscription hook | ⚠️ (a) OK for trader info. (b) ✅ Completed: live code now uses 2127/2129 for spot subscriptions. |
| 2155–2159 | PROTO_OA_SUBSCRIBE_DEPTH_QUOTES_REQ/RES, PROTO_OA_DEPTH_EVENT | `Symbols` depth subscription helpers, `NetworkThread` dispatcher | ✅ Depth subscription/send (2155/2156) and event ingestion (2159) implemented; unsubscribe wiring pending. |

## High-priority alignment tasks (Phase 0)
1. **Correct payload IDs in existing code paths**
   - `Trading::PlaceOrder` → use 2106 request + handle 2126 execution events and 2132 order errors.
   - `NetworkThread` heartbeat → switch to proper keep-alive (2148 disconnect handling + periodic 2104 version probe if needed) or implement transport-level ping via WinHTTP API.
   - `Trading::ClosePosition` → issue 2111 close request; add 2108 cancel order helper.
   - `History::RequestHistoricalData` → map to 2137/2138 for bars and 2145/2146 for tick data.
   - `Symbols` spot subscriptions → ✅ Completed (2025-10-03). Ensure unsubscribe clean-up once Phase 2 is finalized.

2. **Introduce explicit enumerations / helpers**
   - Define `enum class PayloadType : int` in `globals.h` or a new header to centralize IDs and prevent drift.
   - Replace magic numbers in request builders with enum constants to enable compiler-level validation.

3. **Refactor NetworkThread dispatch**
   - Replace chained `if` blocks with `switch` on the parsed payload type.
   - Add logging for unknown IDs to speed up future integrations.

## Missing payload families and suggested home modules
| Category | Payload IDs | Recommended module(s) | Notes |
|----------|-------------|------------------------|-------|
| **Protocol & session management** | 2104, 2147, 2148, 2162–2164 | `main.cpp`, `network.cpp`, `reconnect.cpp` | Implement version negotiation, token invalidation notification, client disconnect handling, and graceful logout flows.|
| **Order lifecycle enhancements** | 2107, 2109–2111, 2126, 2132, 2175–2184 | `trading.cpp`, `main.cpp` | Add trailing stop updates, amend order SL/TP/volume, close position, execution event parsing, order error reporting, and order list retrieval utilities to sync with Zorro positions.|
| **Market data subscriptions** | 2127–2131, 2155–2159 | `symbols.cpp`, `main.cpp` | Implement subscribe/unsubscribe for spots, depth quotes, and handle associated events (`SPOT_EVENT`, `DEPTH_EVENT`). Update `Symbols::UpdateQuote` to consume 2131 payload schema.|
| **Historical & analytical data** | 2133–2138, 2143–2146, 2185–2188 | `history.cpp`, new `analytics.cpp` (optional) | Extend history module to support deal lists, cash-flow history, trendbars, tick data, unrealized PnL, and deal offsets.
| **Margin & risk monitoring** | 2139–2141, 2167–2172 | `account.cpp`, `main.cpp` | Request expected margin, handle margin change events, and process margin call lifecycle messages to keep Zorro margin metrics current.
| **Authentication & identity** | 2149–2154, 2173–2174 | `auth.cpp`, `oauth_utils.cpp` | Fetch accounts via token, obtain CTID profile, refresh access tokens proactively when receiving 2147/2148 notifications.|
| **Symbol metadata expansion** | 2112–2120, 2153–2154, 2160–2161 | `symbols.cpp` | Add asset class, conversion symbols, symbol-by-id lookups, category metadata, and symbol change event handling.

## Proposed implementation roadmap
1. **Phase 0 – Correctness baseline**
   - Introduce payload enum and update existing request builders/handlers (see high-priority list).
   - Add regression tests (unit-style or scripted) verifying the JSON payload produced for key requests.
   - Update `payloads.py` to read enum names instead of raw literals once refactor is done.

2. **Phase 1 – Market data parity**
   - [x] Build new subscription manager covering 2127–2131 (implemented 2025-10-03).
   - [x] Parse `PROTO_OA_SPOT_EVENT` (2131) into `Symbols::UpdateQuote` (NetworkThread switch updated 2025-10-03).
   - [x] Add depth quote support for assets where depth trading is needed (2155/2156/2159 wired on 2025-10-03; unsubscribe still queued).

3. **Phase 2 – Trading feature completeness**
   - [x] Implement amend/cancel/close flows with optimistic local cache updates.
   - [x] Handle `PROTO_OA_EXECUTION_EVENT` (2126) and `PROTO_OA_ORDER_ERROR_EVENT` (2132) to keep trade state in sync.
   - [x] Wire order/position list endpoints for portfolio reconciliation snapshots.
   - _Status_: Phase 2 core workflows validated; proceed to depth unsubscribe and Phase 3 analytics.

4. **Phase 3 – Account analytics & resilience**
   - [ ] Integrate margin and cash-flow messages (`PROTO_OA_MARGIN_CHANGED_EVENT`, `PROTO_OA_CASH_FLOW_EVENT`).
   - [ ] React to token invalidation and refresh flows automatically (2147/2148 notifications → proactive refresh pipeline).
   - [ ] Persist and expose asset class / symbol category metadata for Zorro (asset catalogue enrichment, category mapping).
   - [ ] Capture session lifecycle events (2162–2164) to drive reconnect/backoff state machine.
   - [ ] Add regression script to diff broker account balances/equity against API events.

## Next steps checklist
- [ ] Refactor payload constants (enum + replacement of literals).
- [ ] Update existing request builders to use the correct payload IDs.
- [ ] Extend `NetworkThread` switch to log unhandled payloads and stub handlers for Phase 1 messages.
- [ ] Harden execution event handling (2126/2132) and add depth unsubscribe/delta compression to stabilize live trading.
- [ ] Expand `payloads.py` / `compare_payloads.py` to use the enum once refactoring lands, ensuring the scripts stay useful as automated coverage checks.
- [ ] Design Phase 3 telemetry (margin/cash-flow/unit tests) and create fixtures for simulated account events.

Once Phase 0 is complete, rerun `compare_payloads.py` to confirm that handled IDs now match their official meanings and to track progress on integrating the remaining payload families.
