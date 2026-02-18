#pragma once
// BrokerCommand codes from Zorro SDK trading.h
// Only the constants we need - NOT the full header (avoids T6/GLOBALS conflicts)

// GET_* commands
#define GET_TIME            5
#define GET_DIGITS          12
#define GET_STOPLEVEL       14
#define GET_TRADEALLOWED    22
#define GET_MINLOT          23
#define GET_LOTSTEP         24
#define GET_MAXLOT          25
#define GET_MARGININIT      29
#define GET_MARGINMAINTAIN  30
#define GET_MARGINHEDGED    31
#define GET_BROKERZONE      40
#define GET_MAXTICKS        43
#define GET_MAXREQUESTS     45
#define GET_LOCK            46
#define GET_HEARTBEAT       47
#define GET_COMPLIANCE      51
#define GET_NTRADES         52
#define GET_POSITION        53
#define GET_ACCOUNT         54
#define GET_AVGENTRY        55
#define GET_SERVERSTATE     68
#define GET_TRADES          71
#define GET_DATA            72
#define GET_PRICETYPE       150
#define GET_VOLTYPE         152
#define GET_UUID            154

// SET_* commands
#define SET_PATCH           128
#define SET_SLIPPAGE        129
#define SET_MAGIC           130
#define SET_ORDERTEXT       131
#define SET_SYMBOL          132
#define SET_CLASS           134
#define SET_LIMIT           135
#define SET_HISTORY         136
#define SET_DIAGNOSTICS     138
#define SET_AMOUNT          139
#define SET_LEVERAGE        140
#define SET_PRICETYPE       151
#define SET_VOLTYPE         153
#define SET_UUID            155
#define SET_ORDERTYPE       157
#define SET_DELAY           169
#define SET_WAIT            170
#define SET_LOCK            171
#define SET_HWND            172
#define SET_MAXTICKS        173
#define SET_SERVER          182

// DO_* commands
#define DO_CANCEL           301

// Custom plugin commands (M5: SL/TP modification)
#define SET_STOPLOSS        2001  // *(double*)dwParameter = SL price (0 = remove)
#define SET_TAKEPROFIT      2002  // *(double*)dwParameter = TP price (0 = remove)
#define DO_MODIFY_SLTP      2003  // dwParameter = tradeId -> send AmendPositionSltpReq
