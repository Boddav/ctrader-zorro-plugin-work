# BrokerCommand Implementation Status

## Currently Implemented ✅

### Information Commands (GET_*)
- **0 (GET_TIME)** - Last incoming quote timestamp from server
- **1 (GET_DIGITS)** - Number of decimal digits for symbol
- **2 (GET_STOPLEVEL)** - Safety net stop level (default 5 pips)
- **3 (GET_TRADEALLOWED)** - Check if trading allowed for asset
- **4 (GET_MINLOT)** - Minimum lot size (0.01 lots)
- **5 (GET_LOTSTEP)** - Lot size step (0.01 lots)
- **6 (GET_MAXLOT)** - Maximum lot size (100 lots)
- **7 (GET_MARGININIT)** - Initial margin requirement (~1000 per lot)
- **8 (GET_MARGINMAINTAIN)** - Maintenance margin (same as initial)
- **9 (GET_MARGINHEDGED)** - Hedged margin (0 - cTrader uses netting)
- **10 (GET_COMPLIANCE)** - Account trading restrictions (0 = none)
- **11 (GET_SERVERSTATE)** - Broker server connection status
- **16 (GET_NTRADES)** - Number of open trades
- **17 (GET_POSITION)** - Net open amount for symbol
- **18 (GET_AVGENTRY)** - Average entry price
- **19 (GET_ACCOUNT)** - Fill string with account name

### Configuration Commands (SET_*)
- **128 (SET_PRICETYPE)** - Set price data retrieval method
- **129 (SET_ORDERTYPE)** - Set order type (0=market, 2=limit, 4=stop)
- **130 (SET_SYMBOL)** - Set current symbol for subsequent commands
- **132 (SET_MULTIPLIER)** - Return account leverage (e.g., 500.0 for 1:500)
- **138 (SET_DIAGNOSTICS)** - Set diagnostic level

### Custom Close Commands (7000 range)
- **7001 (CMD_CLOSE_ALL_SHORT)** - Close all short positions
- **7002 (CMD_CLOSE_ALL_LONG)** - Close all long positions
- **7003 (CMD_CLOSE_ALL_PROFIT)** - Close all profitable positions
- **7004 (CMD_CLOSE_ALL_LOSS)** - Close all losing positions

---

## Missing Standard Commands ❌

### Advanced Information Commands (GET_*)
- **GET_DATA** - Send/receive REST API requests (could use BrokerRequest)
- **GET_PRICE** - Return specific price types
- **GET_VOLUME** - Return specific volume data
- **GET_BOOK** - Fill array with order book quotes (not available in cTrader API)
- **GET_TRADES** - Fill array with open positions (use GET_POSITION instead)
- **GET_OPTIONS** - Retrieve options chain (not available - forex only)
- **GET_FUTURES** - Retrieve futures chain (not available - forex only)
- **GET_FOP** - Retrieve futures-on-options chain (not available)
- **GET_CHAIN** - Retrieve contract chain data (not available)

### Configuration Commands (SET_*)
- **SET_DELAY** - Set delay between broker commands (not needed - API handles timing)
- **SET_WAIT** - Set maximum wait time for confirmations (not needed - async)
- **SET_MAGIC** - Set magic number for trade identification (not needed - orderId used)
- **SET_AMOUNT** - Set lot size (handled in BrokerBuy/BrokerSell)
- **SET_LEVERAGE** - Set trading leverage (not supported by cTrader API)

### Plotting Commands
- **PLOT_HLINE** - Place horizontal line in chart
- **PLOT_TEXT** - Place text in chart
- **PLOT_MOVE** - Move graphical elements
- **PLOT_REMOVE** - Remove graphical elements

### User-Defined Command Ranges
- **2000-2999** - Single numerical parameter commands
- **3000-3999** - Commands with 8-parameter arrays
- **4000-5999** - Text parameter commands

---

## cTrader Open API Mapping

### Available in cTrader API:
1. **GET_TIME** → Use ProtoOASpotEvent timestamp
2. **GET_DIGITS** → Use symbolDigits from ProtoOASymbol
3. **GET_MINLOT** → Use minVolume from ProtoOASymbol
4. **GET_MAXLOT** → Use maxVolume from ProtoOASymbol
5. **GET_LOTSTEP** → Use stepVolume from ProtoOASymbol
6. **GET_SERVERSTATE** → Check WebSocket connection status
7. **GET_NTRADES** → Count positions from ProtoOAReconcileRes
8. **GET_POSITION** → Sum netVolume from positions
9. **GET_AVGENTRY** → Calculate from ProtoOAPosition entryPrice
10. **GET_ACCOUNT** → Use ctidTraderAccountId
11. **GET_TRADES** → Use ProtoOAReconcileRes positions array
12. **GET_MARGININIT/MAINTAIN** → Use margin from ProtoOAPosition
13. **SET_LEVERAGE** → Use ProtoOAAmendPositionSLTPReq (partial support)
14. **SET_PRICETYPE** → Already implemented via BrokerCommand(SET_PRICETYPE, ...)

### Not Available in cTrader API:
- GET_BOOK (order book depth)
- GET_OPTIONS/FUTURES/FOP/CHAIN (derivatives)
- PLOT_* commands (Zorro-side only)
- Some advanced margin calculations

---

## Implementation Summary

### ✅ All Core Commands Implemented (22 total)
All high and medium priority BrokerCommand codes are now implemented:

**GET Commands (16):** TIME, DIGITS, STOPLEVEL, TRADEALLOWED, MINLOT, LOTSTEP, MAXLOT, MARGININIT, MARGINMAINTAIN, MARGINHEDGED, COMPLIANCE, SERVERSTATE, NTRADES, POSITION, AVGENTRY, ACCOUNT

**SET Commands (5):** PRICETYPE, ORDERTYPE, SYMBOL, MULTIPLIER, DIAGNOSTICS

**Custom Commands (4):** CLOSE_ALL_SHORT, CLOSE_ALL_LONG, CLOSE_ALL_PROFIT, CLOSE_ALL_LOSS

### 🔄 Future Enhancements (Optional)
- **GET_PRICE/GET_VOLUME** - Specific price/volume data extraction
- **GET_DATA** - Could leverage existing BrokerRequest for REST API calls
- **Plotting commands** - Zorro-side only, not broker-specific

---

## Implementation Notes

### Current Architecture
- BrokerCommand implementation in: `src/main.cpp:1740`
- Symbol info available in: `Symbols::GetSymbolByIdOrName()`
- Trade data available in: `Trading` namespace
- Account data available in: `Account` namespace

### Next Steps
1. Add symbol info GET commands using existing Symbols:: functions
2. Add trade info GET commands using existing Trading:: data
3. Add account GET commands using G.CTraderAccountId
4. Add configuration SET commands for leverage, order types
5. Document custom command ranges (7000+) for cTrader-specific features
