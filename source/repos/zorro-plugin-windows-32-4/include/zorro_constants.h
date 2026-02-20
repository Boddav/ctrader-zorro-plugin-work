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
#define GET_DELAY           41
#define GET_WAIT            42
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
#define SET_COMMENT         180
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

// Trade flags (from Zorro trading.h)
#define TR_LONG     0
#define TR_SHORT    1             // short position
#define TR_OPEN     (1<<1)        // position is open (=2)

// Zorro TRADE struct - MUST match Zorro's trading.h layout exactly (32-bit, default MSVC alignment)
// Used for GET_TRADES command. Plugin fills nID, nLots, flags, fEntryPrice.
// All other fields zeroed.
struct ZorroTrade {
    float   fEntryPrice;          // buy price
    float   fExitPrice;           // sell price per unit
    float   fResult;              // current profit
    float   fEntryLimit;          // entry limit/stop
    float   fProfitLimit;         // profit limit price
    float   fTrailLock;           // profit target distance
    float   fStopLimit;           // stop loss limit price
    float   fStopDiff;            // stop loss distance
    float   fTrailLimit;          // trail limit price
    float   fTrailDiff;           // trail adaption distance
    float   fTrailSlope;          // stop loss adaption factor
    float   fTrailStep;           // stop loss step factor
    float   fSpread;              // spread
    float   fMAE, fMFE;           // adverse/favorable excursion
    float   fRoll;                // rollover
    float   fSlippage;            // slippage
    float   fUnits;               // conversion factor
    float   fTrailSpeed;          // break even speed
    int     nExitTime;            // sell at market after N bars
    int     nEntryTime;           // wait N bars for entry limit
    int     nLots;                // filled lots
    unsigned int nBarOpen;        // bar number of entry
    unsigned int nBarClose;       // bar number of exit
    int     nID;                  // trade/order ID
    double  tEntryDate;           // DATE: entry time
    double  tExitDate;            // DATE: exit time
    int     flags;                // TR_OPEN | TR_SHORT etc
    float   fArg[8];              // TMF arguments
    double  Skill[8];             // var=double, NUM_SKILLS=8
    int     nContract;            // contract type
    float   fStrike;              // strike price
    float   fUnl;                 // underlying price
    char    sInfo[8];             // trading class
    float   fMarginCost;          // margin cost
    float   fCommission;          // commission
    int     flags2;               // internal flags
    int     nLotsTarget;          // original lots
    int     nAttempts;            // close attempts
    int     nExpiry;              // expiration date
    void*   algo;                 // 4 bytes (32-bit)
    void*   manage;               // 4 bytes
    double* vExtraData;           // var* = 4 bytes (32-bit)
    float   fLastStop;            // last stop update
    float   fPad[1];              // padding
    int     nBarLast;             // last trail step bar
};
