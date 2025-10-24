# Order Type Mély Kutatás: Zorro vs cTrader OpenAPI

**Kutató**: Claude Code
**Dátum**: 2025-10-15
**Cél**: Teljes körű összehasonlítás a Zorro platform elvárásai és a cTrader OpenAPI lehetőségei között az order type-ok terén

---

## 1. ZORRO PLATFORM ELVÁRÁSAI

### 1.1 BrokerBuy2 Függvény Specifikáció

**Függvény szignatúra**:
```c
int BrokerBuy2(char* Asset, int Amount, double StopDist, double* pPrice, double* pFill, double Limit)
```

**Paraméterek**:
- `Asset` (char*) - A kereskedési eszköz neve (pl. "EURUSD")
- `Amount` (int) - Lot mennyiség (pozitív=long, negatív=short)
- `StopDist` (double) - Stop loss távolság az aktuális ártól
- `pPrice` (double*) - Visszatérési érték: végrehajtási ár
- `pFill` (double*) - Visszatérési érték: ténylegesen végrehajtott mennyiség
- `Limit` (double) - **KULCS PARAMÉTER** az order típushoz:
  - `Limit == 0` → **MARKET ORDER**
  - `Limit > 0` → **LIMIT ORDER** (adott áron végrehajtandó)
  - `Limit < 0` → **ENTRY ORDER** (pending order, piaci áron teljesül ha eléri)

**Visszatérési értékek**:
- `0` vagy `1` - Order elutasítva vagy nem teljesült
- Pozitív szám - Egyedi trade/order ID
- `-1` - UUID trade azonosító
- `-2` - Nincs broker válasz
- `-3` - Order elfogadva, de még nincs ID

### 1.2 Order Típusok Zorro-ban

#### A) MARKET ORDER (azonnali végrehajtás)
```c
// Példa: Market order
OrderLimit = 0;  // MARKET
enterLong(5);    // 5 lot vásárlás piaci áron
```

**Jellemzők**:
- Azonnali végrehajtás az aktuális bid/ask áron
- Nincs garantált ár
- Slippage előfordulhat
- Long: Best Ask áron, Short: Best Bid áron

#### B) LIMIT ORDER (árlimit)
```c
// Példa: Limit order
OrderLimit = 100.00;  // LIMIT price
enterLong(5);         // 5 lot vásárlás 100.00-on vagy jobb áron
```

**Jellemzők**:
- Csak a megadott áron vagy jobb áron teljesül
- Nem garantált végrehajtás
- LONG limit: Ask ≤ Limit
- SHORT limit: Bid ≥ Limit

#### C) ENTRY ORDER (pending)
```c
// Példa: Entry order
Entry = 105.00;  // Entry price
enterLong(5);    // Pending order, 105.00 elérésekor market order
```

**Jellemzők**:
- Függőben lévő megbízás
- Ár elértekor MARKET order-ré alakul
- Trade Management Function (TMF) kezeli

### 1.3 Time-In-Force (TIF) Támogatás

**SET_ORDERTYPE parancs** (brokerCommand):
```c
// Példa különböző TIF beállítások
brokerCommand(SET_ORDERTYPE, ORDERTYPE_DAY);  // Day order
brokerCommand(SET_ORDERTYPE, ORDERTYPE_GTC);  // Good Till Cancelled
brokerCommand(SET_ORDERTYPE, ORDERTYPE_IOC);  // Immediate Or Cancel
brokerCommand(SET_ORDERTYPE, ORDERTYPE_FOK);  // Fill Or Kill (default)
```

**Támogatott TIF típusok**:
- `ORDERTYPE_FOK` - Fill Or Kill (alapértelmezett)
- `ORDERTYPE_IOC` - Immediate Or Cancel
- `ORDERTYPE_GTC` - Good Till Cancelled
- `ORDERTYPE_DAY` - Day order
- `ORDERTYPE_OPG` - At The Opening
- `ORDERTYPE_CLS` - At The Closing

**Fontos**: A broker plugin-nek támogatnia kell a `SET_ORDERTYPE` és `DO_CANCEL` parancsokat!

### 1.4 Stop Loss és Take Profit

**Stop Loss**:
```c
Stop = 0.01 * priceClose();  // 1% stop loss
enterLong();
```

**Take Profit**:
```c
TakeProfit = 1.08000;  // Absolute price
enterLong();
```

**Jellemzők**:
- Eszköz-specifikusak (asset() után kell beállítani)
- Absolut ár VAGY relatív távolság
- NFA compliant fiók esetén külön order-ként kell kezelni

---

## 2. cTRADER OPENAPI LEHETŐSÉGEI

### 2.1 ProtoOAOrderType ENUM

**Hivatalos definíció** (OpenApiModelMessages.proto):
```protobuf
enum ProtoOAOrderType {
    MARKET = 1;
    LIMIT = 2;
    STOP = 3;
    STOP_LOSS_TAKE_PROFIT = 4;
    MARKET_RANGE = 5;
    STOP_LIMIT = 6;
}
```

**Részletes leírás**:

| Érték | Típus | Leírás | Zorro megfelelő |
|-------|-------|--------|-----------------|
| 1 | **MARKET** | Azonnali végrehajtás piaci áron | ✓ OrderLimit=0 |
| 2 | **LIMIT** | Adott áron vagy jobb áron | ✓ OrderLimit>0 |
| 3 | **STOP** | Ár elérésekor market order | ✓ Entry>0 |
| 4 | **STOP_LOSS_TAKE_PROFIT** | Védőárfolyam módosítás | ✗ Nem támogatott |
| 5 | **MARKET_RANGE** | Market + slippage kontroll | ✗ Nem támogatott |
| 6 | **STOP_LIMIT** | Stop + Limit kombináció | ✗ Nem támogatott |

### 2.2 PROTO_OA_NEW_ORDER_REQ Üzenet

**Kötelező mezők**:
```json
{
  "ctidTraderAccountId": 44533070,    // Trader account ID
  "symbolId": 5,                      // Symbol ID
  "orderType": 1,                     // ProtoOAOrderType enum
  "tradeSide": 1,                     // 1=BUY, 2=SELL
  "volume": 100000                    // Volume in cents
}
```

**Opcionális mezők - Order Type specifikus**:

#### MARKET ORDER (orderType=1):
```json
{
  "orderType": 1,
  "stopLoss": 1.05500,           // Absolute SL price
  "takeProfit": 1.08000,         // Absolute TP price
  "relativeStopLoss": 500,       // SL pips-ben (500 = 50 pips)
  "relativeTakeProfit": 800,     // TP pips-ben (800 = 80 pips)
  "guaranteedStopLoss": true,    // Garantált SL
  "trailingStopLoss": true       // Trailing SL
}
```

**FIGYELEM**: cTrader dokumentáció szerint:
> "Stop loss/take profit not supported for MARKET orders"

**Ellentmondás**: Valójában TÁMOGATVA VAN! A jelenlegi implementációnk is használja.

#### LIMIT ORDER (orderType=2):
```json
{
  "orderType": 2,
  "limitPrice": 1.10000,         // KÖTELEZŐ - Limit price
  "stopLoss": 1.09500,           // Opcionális
  "takeProfit": 1.10500,         // Opcionális
  "timeInForce": "GOOD_TILL_CANCEL",
  "expirationTimestamp": 1730000000000
}
```

#### STOP ORDER (orderType=3):
```json
{
  "orderType": 3,
  "stopPrice": 1.12000,          // KÖTELEZŐ - Stop trigger price
  "stopLoss": 1.11500,           // Opcionális
  "takeProfit": 1.12500,         // Opcionális
  "stopTriggerMethod": "TRADE"   // TRADE vagy BID_ASK
}
```

#### MARKET_RANGE ORDER (orderType=5):
```json
{
  "orderType": 5,
  "baseSlippagePrice": 1.10000,  // Referencia ár
  "slippageInPoints": 10,        // Max slippage (10 points)
  "stopLoss": 1.09500,           // Opcionális
  "takeProfit": 1.10500          // Opcionális
}
```

#### STOP_LIMIT ORDER (orderType=6):
```json
{
  "orderType": 6,
  "stopPrice": 1.12000,          // Stop trigger price
  "limitPrice": 1.12050,         // Limit price a trigger után
  "stopTriggerMethod": "TRADE"   // TRADE vagy BID_ASK
}
```

### 2.3 Időzítés és Lejárat

**ProtoOATimeInForce ENUM**:
```protobuf
enum ProtoOATimeInForce {
    GOOD_TILL_DATE = 1;          // Határidőig érvényes
    GOOD_TILL_CANCEL = 2;        // Törlésig érvényes
    IMMEDIATE_OR_CANCEL = 3;     // Azonnal vagy törlés
    FILL_OR_KILL = 4;            // Teljes végrehajtás vagy törlés
    MARKET_ON_OPEN = 5;          // Nyitáskor piaci
}
```

**Lejárati időbélyeg**:
```json
{
  "timeInForce": "GOOD_TILL_DATE",
  "expirationTimestamp": 1730000000000  // Unix timestamp millisec-ben
}
```

### 2.4 Védőárfolyam Típusok

#### Absolute Stop Loss / Take Profit:
```json
{
  "stopLoss": 1.09500,       // Abszolút ár
  "takeProfit": 1.10500      // Abszolút ár
}
```

#### Relative Stop Loss / Take Profit:
```json
{
  "relativeStopLoss": 500,   // 500 * 0.00001 = 50 pips
  "relativeTakeProfit": 800  // 800 * 0.00001 = 80 pips
}
```

**Fontos**: 1/100000 egységben (decimális 5 helyiérték)

#### Guaranteed Stop Loss:
```json
{
  "guaranteedStopLoss": true  // Bróker garantálja
}
```

**Megjegyzés**: Limited Risk fiókoknál KÖTELEZŐ!

#### Trailing Stop Loss:
```json
{
  "trailingStopLoss": true,
  "relativeStopLoss": 500     // Kezdeti távolság
}
```

### 2.5 Slippage Kontroll

**baseSlippagePrice** és **slippageInPoints**:
```json
{
  "orderType": 5,  // MARKET_RANGE
  "baseSlippagePrice": 1.10000,
  "slippageInPoints": 10  // Max 10 point csúszás
}
```

**Alternatíva** - limitPrice és stopPrice közötti tartomány.

---

## 3. KOMPATIBILITÁSI MÁTRIX

### 3.1 Order Type Mapping

| Zorro Order Type | Zorro Paraméter | cTrader OrderType | cTrader Mező | Kompatibilis? |
|------------------|-----------------|-------------------|--------------|---------------|
| **MARKET** | Limit=0 | MARKET (1) | orderType=1 | ✓ TELJES |
| **LIMIT** | Limit>0 | LIMIT (2) | orderType=2, limitPrice | ✓ TELJES |
| **ENTRY** | Entry>0 | STOP (3) | orderType=3, stopPrice | ✓ TELJES |
| **Stop Loss** | Stop>0 | stopLoss | stopLoss (absolute) | ✓ TELJES |
| **Take Profit** | TakeProfit>0 | takeProfit | takeProfit (absolute) | ✓ TELJES |

### 3.2 Time-In-Force Mapping

| Zorro TIF | Zorro Konstans | cTrader TIF | cTrader Enum | Kompatibilis? |
|-----------|----------------|-------------|--------------|---------------|
| Fill Or Kill | ORDERTYPE_FOK | FILL_OR_KILL | 4 | ✓ TELJES |
| Immediate Or Cancel | ORDERTYPE_IOC | IMMEDIATE_OR_CANCEL | 3 | ✓ TELJES |
| Good Till Cancelled | ORDERTYPE_GTC | GOOD_TILL_CANCEL | 2 | ✓ TELJES |
| Day Order | ORDERTYPE_DAY | GOOD_TILL_DATE | 1 | ✓ RÉSZLEGES* |
| Market On Open | ORDERTYPE_OPG | MARKET_ON_OPEN | 5 | ✓ TELJES |
| Market On Close | ORDERTYPE_CLS | - | - | ✗ NINCS |

*ORDERTYPE_DAY → GOOD_TILL_DATE + expirationTimestamp (nap vége)

### 3.3 Speciális Funkciók

| Funkció | Zorro Támogatás | cTrader Támogatás | Implementációs Összetettség |
|---------|-----------------|-------------------|----------------------------|
| **Relative SL/TP** | ✗ Nem natív | ✓ relativeStopLoss/TP | KÖZEPES |
| **Guaranteed SL** | ✗ Nem natív | ✓ guaranteedStopLoss | ALACSONY |
| **Trailing SL** | ✓ Trail paraméter | ✓ trailingStopLoss | MAGAS |
| **Slippage Control** | ✗ Nem natív | ✓ MARKET_RANGE | KÖZEPES |
| **Stop Limit** | ✗ Nem natív | ✓ STOP_LIMIT (6) | MAGAS |
| **Label/Comment** | ✗ Nem natív | ✓ label, comment | ALACSONY |

---

## 4. JELENLEGI IMPLEMENTÁCIÓ KRITIKAI ÉRTÉKELÉSE

### 4.1 Implementált Funkciók (trading.cpp)

**Aktuális kód (trading.cpp:196-203)**:
```cpp
sprintf_s(request,
    "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
    "{\"ctidTraderAccountId\":%lld,"
    "\"symbolId\":%lld,\"orderType\":1,\"tradeSide\":%d,\"volume\":%lld%s%s}}",
    clientMsgId.c_str(),
    ToInt(PayloadType::NewOrderReq),  // 2106
    G.CTraderAccountId, symbolId,
    tradeSide, volumeInCents, stopLossStr.c_str(), takeProfitStr.c_str());
```

**Problémák**:

1. **HARDCODED orderType=1** ❌
   - Mindig MARKET order
   - Nem veszi figyelembe a Zorro `Limit` paramétert
   - Nem támogatja LIMIT és STOP order-eket

2. **Nincs Limit paraméter feldolgozás** ❌
   - BrokerBuy2 `Limit` paramétere ignorálva
   - Nem hozza létre a `limitPrice` mezőt

3. **Nincs TIF támogatás** ❌
   - SET_ORDERTYPE parancs nem implementált
   - timeInForce mező mindig hiányzik

4. **Absolute SL/TP CSAK** ⚠️
   - relativeStopLoss/TP nem támogatott
   - Pips-ben megadott védőárfolyamok nem lehetségesek

5. **Speciális funkciók hiányoznak** ❌
   - Guaranteed SL: NEM
   - Trailing SL: NEM
   - Slippage control: NEM
   - Label/Comment: NEM

### 4.2 Kritikus Hiányzó Logika

**A) Limit paraméter feldolgozás (NEM implementált)**:
```cpp
// KELLENE valami ilyesmi:
int orderType = 1;  // MARKET default
double limitPrice = 0.0;
double stopPrice = 0.0;

if (limit > 0) {
    // LIMIT ORDER
    orderType = 2;
    limitPrice = limit;
} else if (limit < 0) {
    // ENTRY ORDER (STOP)
    orderType = 3;
    stopPrice = -limit;  // Entry价 pozitívvá alakítva
}

// JSON építés orderType alapján...
```

**B) TIF támogatás (NEM implementált)**:
```cpp
// KELLENE valami ilyesmi a brokerCommand-ban:
case SET_ORDERTYPE:
    switch (dwValue) {
        case ORDERTYPE_FOK:
            G.currentTimeInForce = "FILL_OR_KILL";
            break;
        case ORDERTYPE_IOC:
            G.currentTimeInForce = "IMMEDIATE_OR_CANCEL";
            break;
        case ORDERTYPE_GTC:
            G.currentTimeInForce = "GOOD_TILL_CANCEL";
            break;
        // stb...
    }
    break;
```

---

## 5. ZORRO MŰKÖDÉSI MECHANIZMUS (Belső Folyamat)

### 5.1 enterLong() / enterShort() Hívási Lánc

**1. Zorro Script (C-lite)**:
```c
function run() {
    asset("EURUSD");
    OrderLimit = 1.10000;  // Limit order
    Stop = 0.01 * priceClose();
    enterLong(5);
}
```

**2. Zorro Core (belső C++ kód)**:
```cpp
// Zorro belső függvény
int enterLong(int amount) {
    double limit = OrderLimit;  // Global variable
    double stop = Stop;         // Global variable

    // Plugin függvény hívás
    int tradeId = BrokerBuy2(Asset, amount, stop, NULL, NULL, limit);

    return tradeId;
}
```

**3. Plugin DLL (cTrader.dll)**:
```cpp
DLLFUNC int BrokerBuy2(char* Asset, int nAmount,
                       double dStopDist, double* pPrice,
                       double* pFill, double Limit)
{
    // JELENLEGI IMPLEMENTÁCIÓ:
    // Limit paraméter IGNORÁLVA ❌
    // Mindig orderType=1 (MARKET)

    return Trading::PlaceOrder(Asset, nAmount, dStopDist, Limit, pPrice, (int*)pFill);
}
```

**4. Trading::PlaceOrder() (trading.cpp)**:
```cpp
int PlaceOrder(const char* symbol, int amount,
               double stopDist, double limit,
               double* pPrice, int* pFill)
{
    // PROBLEM: limit paraméter NEM használva! ❌
    // orderType mindig 1 (MARKET)

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
        "{\"ctidTraderAccountId\":%lld,"
        "\"symbolId\":%lld,\"orderType\":1,\"tradeSide\":%d,\"volume\":%lld%s%s}}",
        ...);
}
```

### 5.2 Példa Forgatókönyvek

#### Forgatókönyv A: MARKET Order (működik ✓)
```c
// Zorro script
OrderLimit = 0;  // MARKET
enterLong(5);

// BrokerBuy2 hívás: Limit=0
// PlaceOrder: orderType=1, limitPrice nincs
// cTrader: MARKET order azonnal végrehajtva
```

**Eredmény**: ✓ Működik helyesen

#### Forgatókönyv B: LIMIT Order (NEM működik ❌)
```c
// Zorro script
OrderLimit = 1.10000;  // LIMIT
enterLong(5);

// BrokerBuy2 hívás: Limit=1.10000
// PlaceOrder: orderType=1 (MARKET!), limitPrice nincs ❌
// cTrader: MARKET order piaci áron (1.10000 figyelmen kívül hagyva!)
```

**Eredmény**: ❌ HIBÁS - piaci áron végrehajtva limit ár helyett!

#### Forgatókönyv C: ENTRY/STOP Order (NEM működik ❌)
```c
// Zorro script
Entry = 1.12000;  // ENTRY
enterLong(5);

// BrokerBuy2 hívás: Limit=-1.12000 (negatív!)
// PlaceOrder: orderType=1 (MARKET!), stopPrice nincs ❌
// cTrader: MARKET order azonnal végrehajtva (1.12 pending helyett!)
```

**Eredmény**: ❌ HIBÁS - azonnal végrehajtva pending helyett!

---

## 6. IMPLEMENTÁCIÓS ÚTMUTATÓ

### 6.1 ALAPVETŐ Fix (High Priority)

**Cél**: LIMIT és STOP order támogatás

**1. lépés: main.cpp - BrokerBuy2 javítása**

**ELŐTTE** (main.cpp):
```cpp
DLLFUNC int BrokerBuy2(char* Asset, int nAmount,
                       double dStopDist, double* pPrice,
                       double* pFill, double Limit)
{
    return Trading::PlaceOrder(Asset, nAmount, dStopDist, Limit, pPrice, (int*)pFill);
}
```

**UTÁNA** (main.cpp):
```cpp
DLLFUNC int BrokerBuy2(char* Asset, int nAmount,
                       double dStopDist, double* pPrice,
                       double* pFill, double Limit)
{
    // LIMIT paraméter átadása
    return Trading::PlaceOrder(Asset, nAmount, dStopDist, Limit, pPrice, (int*)pFill);
}
```
*(Ebben az esetben már átadjuk, szóval OK)*

**2. lépés: trading.cpp - PlaceOrder módosítása**

**ELŐTTE** (trading.cpp:196-203):
```cpp
int tradeSide = (amount > 0) ? 1 : 2; // 1=BUY, 2=SELL
long long volumeInCents = (long long)abs(amount) * 1;

// ... stopPrice, takeProfitPrice számítás ...

sprintf_s(request,
    "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
    "{\"ctidTraderAccountId\":%lld,"
    "\"symbolId\":%lld,\"orderType\":1,\"tradeSide\":%d,\"volume\":%lld%s%s}}",
    clientMsgId.c_str(),
    ToInt(PayloadType::NewOrderReq),
    G.CTraderAccountId, symbolId,
    tradeSide, volumeInCents, stopLossStr.c_str(), takeProfitStr.c_str());
```

**UTÁNA** (trading.cpp - ÚJ LOGIKA):
```cpp
int tradeSide = (amount > 0) ? 1 : 2; // 1=BUY, 2=SELL
long long volumeInCents = (long long)abs(amount) * 1;

// ORDER TYPE DETECTION
int orderType = 1;  // MARKET default
std::string orderTypeSpecificFields;

if (limit > 0) {
    // LIMIT ORDER
    orderType = 2;
    char limitBuf[64];
    sprintf_s(limitBuf, ",\"limitPrice\":%.5f", limit);
    orderTypeSpecificFields = limitBuf;

    Utils::LogToFile("ORDER_TYPE", "LIMIT order detected");
}
else if (limit < 0) {
    // ENTRY ORDER → STOP order
    orderType = 3;
    double stopTriggerPrice = -limit;  // Pozitívvá alakítás
    char stopBuf[64];
    sprintf_s(stopBuf, ",\"stopPrice\":%.5f", stopTriggerPrice);
    orderTypeSpecificFields = stopBuf;

    Utils::LogToFile("ORDER_TYPE", "STOP order detected (entry)");
}
else {
    // MARKET ORDER
    Utils::LogToFile("ORDER_TYPE", "MARKET order detected");
}

// ... stopPrice, takeProfitPrice számítás (ugyanaz) ...

// JSON ÉPÍTÉS
sprintf_s(request,
    "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
    "{\"ctidTraderAccountId\":%lld,"
    "\"symbolId\":%lld,\"orderType\":%d,\"tradeSide\":%d,\"volume\":%lld%s%s%s}}",
    clientMsgId.c_str(),
    ToInt(PayloadType::NewOrderReq),
    G.CTraderAccountId, symbolId,
    orderType, tradeSide, volumeInCents,
    orderTypeSpecificFields.c_str(),
    stopLossStr.c_str(), takeProfitStr.c_str());
```

### 6.2 Time-In-Force Támogatás (Medium Priority)

**Cél**: SET_ORDERTYPE parancs támogatása

**3. lépés: globals.h - TIF állapot hozzáadása**

```cpp
// globals.h
struct GlobalState {
    // ... meglévő mezők ...

    // ÚJ MEZŐK
    std::string currentTimeInForce;  // "GOOD_TILL_CANCEL", "FILL_OR_KILL", stb.
    long long orderExpiration;       // Unix timestamp millisec

    GlobalState() :
        // ... meglévő init ...
        currentTimeInForce("FILL_OR_KILL"),  // Default
        orderExpiration(0)
    {}
};
```

**4. lépés: main.cpp - SET_ORDERTYPE implementáció**

```cpp
// main.cpp - BrokerCommand switch
case SET_ORDERTYPE:
    switch (dwValue) {
        case 1:  // ORDERTYPE_FOK
            G.currentTimeInForce = "FILL_OR_KILL";
            G.orderExpiration = 0;
            Utils::LogToFile("TIF", "Set to FILL_OR_KILL");
            break;
        case 2:  // ORDERTYPE_IOC
            G.currentTimeInForce = "IMMEDIATE_OR_CANCEL";
            G.orderExpiration = 0;
            Utils::LogToFile("TIF", "Set to IMMEDIATE_OR_CANCEL");
            break;
        case 3:  // ORDERTYPE_GTC
            G.currentTimeInForce = "GOOD_TILL_CANCEL";
            G.orderExpiration = 0;
            Utils::LogToFile("TIF", "Set to GOOD_TILL_CANCEL");
            break;
        case 4:  // ORDERTYPE_DAY
            G.currentTimeInForce = "GOOD_TILL_DATE";
            // Számítsuk ki a nap végét (23:59:59 GMT)
            G.orderExpiration = CalculateEndOfDay();
            Utils::LogToFile("TIF", "Set to DAY order");
            break;
        default:
            Utils::LogToFile("TIF", "Unknown TIF type");
            break;
    }
    return 1;
```

**5. lépés: trading.cpp - TIF hozzáadása JSON-hoz**

```cpp
// trading.cpp - PlaceOrder
std::string tifFields;
if (!G.currentTimeInForce.empty() && G.currentTimeInForce != "FILL_OR_KILL") {
    // FOK az alapértelmezett, nem kell küldeni
    char tifBuf[128];
    sprintf_s(tifBuf, ",\"timeInForce\":\"%s\"", G.currentTimeInForce.c_str());
    tifFields = tifBuf;

    if (G.orderExpiration > 0) {
        char expBuf[64];
        sprintf_s(expBuf, ",\"expirationTimestamp\":%lld", G.orderExpiration);
        tifFields += expBuf;
    }
}

// JSON építés:
sprintf_s(request,
    "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
    "{\"ctidTraderAccountId\":%lld,"
    "\"symbolId\":%lld,\"orderType\":%d,\"tradeSide\":%d,\"volume\":%lld%s%s%s%s}}",
    clientMsgId.c_str(),
    ToInt(PayloadType::NewOrderReq),
    G.CTraderAccountId, symbolId,
    orderType, tradeSide, volumeInCents,
    orderTypeSpecificFields.c_str(),
    tifFields.c_str(),
    stopLossStr.c_str(), takeProfitStr.c_str());
```

### 6.3 Relative SL/TP Támogatás (Low Priority)

**Cél**: Pips-ben megadott stop loss / take profit

**6. lépés: Relative vs Absolute detektálás**

```cpp
// trading.cpp - PlaceOrder
bool useRelativeSLTP = false;  // Flag

if (stopPrice > 0.0) {
    // Eldöntjük: absolute vagy relative?
    // Ha stopPrice sokkal kisebb mint az aktuális ár → relative (pips)
    // Ha stopPrice közel az aktuális árhoz → absolute (price)

    double currentPrice = (tradeSide == 1) ? marketAsk : marketBid;
    double priceRange = std::abs(stopPrice - currentPrice);

    if (priceRange < 0.01) {
        // Valószínűleg PIPS-ben megadva (pl. 50 pips = 0.0050)
        useRelativeSLTP = true;
        int relativeSLPips = (int)(priceRange * 100000);  // Convert to 1/100000 unit

        char relSLBuf[64];
        sprintf_s(relSLBuf, ",\"relativeStopLoss\":%d", relativeSLPips);
        stopLossStr = relSLBuf;

        Utils::LogToFile("SL_TYPE", "Relative SL used");
    } else {
        // Absolute price
        Utils::LogToFile("SL_TYPE", "Absolute SL used");
    }
}
```

### 6.4 Label és Comment Támogatás (Low Priority)

**Cél**: Order címkézés és kommentálás

**7. lépés: Label/Comment hozzáadása**

```cpp
// trading.cpp - PlaceOrder
std::string labelComment;

// Label: Trade ID vagy asset név
char labelBuf[64];
sprintf_s(labelBuf, ",\"label\":\"Zorro_%s_%d\"", symbol, zorroTradeId);
labelComment = labelBuf;

// Comment: Timestamp vagy stratégia név
char commentBuf[256];
sprintf_s(commentBuf, ",\"comment\":\"Order placed at %lld\"", GetTickCount64());
labelComment += commentBuf;

// JSON építés:
sprintf_s(request,
    "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
    "{\"ctidTraderAccountId\":%lld,"
    "\"symbolId\":%lld,\"orderType\":%d,\"tradeSide\":%d,\"volume\":%lld%s%s%s%s%s}}",
    clientMsgId.c_str(),
    ToInt(PayloadType::NewOrderReq),
    G.CTraderAccountId, symbolId,
    orderType, tradeSide, volumeInCents,
    orderTypeSpecificFields.c_str(),
    tifFields.c_str(),
    stopLossStr.c_str(), takeProfitStr.c_str(),
    labelComment.c_str());
```

---

## 7. TESZTELÉSI TERV

### 7.1 Tesztelendő Forgatókönyvek

#### Test Case 1: MARKET Order
```c
// Zorro script
OrderLimit = 0;
Stop = 0.01 * priceClose();
TakeProfit = 1.08000;
enterLong(5);
```

**Elvárt**:
- orderType = 1
- stopLoss és takeProfit mezők jelennek meg
- Azonnali végrehajtás

#### Test Case 2: LIMIT Order
```c
// Zorro script
OrderLimit = 1.10000;
enterLong(5);
```

**Elvárt**:
- orderType = 2
- limitPrice = 1.10000
- Order pending állapotban
- Végrehajtás amikor Ask ≤ 1.10000

#### Test Case 3: STOP Order (Entry)
```c
// Zorro script
Entry = 1.12000;
enterLong(5);
```

**Elvárt**:
- orderType = 3
- stopPrice = 1.12000
- Order pending állapotban
- Végrehajtás amikor Ask ≥ 1.12000

#### Test Case 4: TIF - Good Till Cancelled
```c
// Zorro script
brokerCommand(SET_ORDERTYPE, ORDERTYPE_GTC);
OrderLimit = 1.10000;
enterLong(5);
```

**Elvárt**:
- timeInForce = "GOOD_TILL_CANCEL"
- Order aktív marad törlésig

#### Test Case 5: TIF - Day Order
```c
// Zorro script
brokerCommand(SET_ORDERTYPE, ORDERTYPE_DAY);
OrderLimit = 1.10000;
enterLong(5);
```

**Elvárt**:
- timeInForce = "GOOD_TILL_DATE"
- expirationTimestamp = nap vége (23:59:59 GMT)
- Order automatikusan törlődik nap végén

### 7.2 Log Ellenőrzési Pontok

**ORDER_TYPE log**:
```
[MARKET|LIMIT|STOP] order detected
```

**TIF log**:
```
Set to [FILL_OR_KILL|IMMEDIATE_OR_CANCEL|GOOD_TILL_CANCEL|DAY order]
```

**WS SEND log**:
```
{"clientMsgId":"msg_XXX","payloadType":2106,"payload":{...}}
```

**Ellenőrizendő mezők**:
- `orderType` (1/2/3)
- `limitPrice` (ha orderType=2)
- `stopPrice` (ha orderType=3)
- `timeInForce` (ha nem FOK)
- `expirationTimestamp` (ha DAY order)

---

## 8. POTENCIÁLIS PROBLÉMÁK ÉS MEGOLDÁSOK

### 8.1 Probléma: LIMIT és STOP order slippage

**Jelenség**:
- LIMIT order rosszabb áron teljesül
- STOP order nem várt áron trigger-el

**Ok**:
- Piaci volatilitás
- Alacsony likviditás

**Megoldás**:
```cpp
// STOP_LIMIT order használata STOP helyett
if (limit < 0) {
    orderType = 6;  // STOP_LIMIT
    double stopTriggerPrice = -limit;
    double limitPriceAfterTrigger = stopTriggerPrice + 0.0010;  // +10 pips slippage limit

    char stopLimitBuf[128];
    sprintf_s(stopLimitBuf, ",\"stopPrice\":%.5f,\"limitPrice\":%.5f",
              stopTriggerPrice, limitPriceAfterTrigger);
    orderTypeSpecificFields = stopLimitBuf;
}
```

### 8.2 Probléma: Order törlés LIMIT/STOP esetén

**Jelenség**:
- DO_CANCEL parancs nem működik pending order-ekre

**Ok**:
- Jelenlegi BrokerSell2 csak position close-ra

**Megoldás**:
```cpp
// main.cpp - BrokerCommand
case DO_CANCEL:
    // Order cancel helyett position close
    return Trading::CancelOrder((int)dwValue);
```

```cpp
// trading.cpp - Új CancelOrder függvény
int CancelOrder(int zorroId) {
    long long orderId = 0;

    EnterCriticalSection(&G.cs_trades);
    auto oit = G.zorroIdToOrderId.find(zorroId);
    if (oit != G.zorroIdToOrderId.end()) {
        orderId = oit->second;
    }
    LeaveCriticalSection(&G.cs_trades);

    if (orderId == 0) {
        Utils::LogToFile("CANCEL_ORDER", "OrderId not found");
        return 0;
    }

    // PROTO_OA_CANCEL_ORDER_REQ (2108)
    std::string clientMsgId = Utils::GetMsgId();
    char request[512];
    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":"
        "{\"ctidTraderAccountId\":%lld,\"orderId\":%lld}}",
        clientMsgId.c_str(), 2108, G.CTraderAccountId, orderId);

    if (!Network::Send(request)) {
        return 0;
    }

    return 1;
}
```

### 8.3 Probléma: DAY order időzóna

**Jelenség**:
- DAY order nem jó időpontban jár le

**Ok**:
- Időzóna eltérés (local vs broker server time)

**Megoldás**:
```cpp
long long CalculateEndOfDay() {
    // Broker szerver idő lekérése
    long long brokerTime = G.lastServerTime;  // ProtoOA_APPLICATION_AUTH_RES

    // Convert to UTC
    time_t serverTimeUTC = brokerTime / 1000;
    struct tm* utcTime = gmtime(&serverTimeUTC);

    // Nap végének számítása (23:59:59 UTC)
    utcTime->tm_hour = 23;
    utcTime->tm_min = 59;
    utcTime->tm_sec = 59;

    time_t endOfDay = mktime(utcTime);
    return (long long)endOfDay * 1000;  // Millisec-re
}
```

---

## 9. TELJESÍTMÉNY ÉS OPTIMALIZÁLÁS

### 9.1 JSON Építés Hatékonysága

**Jelenlegi megközelítés**: sprintf_s (elfogadható)

**Alternatíva** (rapidjson használata):
```cpp
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

// JSON építés rapidjson-nal
rapidjson::Document doc;
doc.SetObject();
auto& allocator = doc.GetAllocator();

doc.AddMember("clientMsgId", rapidjson::Value(clientMsgId.c_str(), allocator), allocator);
doc.AddMember("payloadType", 2106, allocator);

rapidjson::Value payload(rapidjson::kObjectType);
payload.AddMember("ctidTraderAccountId", G.CTraderAccountId, allocator);
payload.AddMember("symbolId", symbolId, allocator);
payload.AddMember("orderType", orderType, allocator);
// ... többi mező ...

doc.AddMember("payload", payload, allocator);

// Serialize
rapidjson::StringBuffer buffer;
rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
doc.Accept(writer);

std::string request = buffer.GetString();
```

**Előnyök**:
- Típusbiztonság
- Automatikus escape-elés
- Olvashatóbb kód

**Hátrányok**:
- Külső függőség
- Nagyobb bináris méret
- Lassabb compile time

**Ajánlás**: sprintf_s elegendő, rapidjson csak nagy projektnél

### 9.2 Order típus detekció gyorsítása

**Optimalizált verzió**:
```cpp
// Előre kiszámolt konstansok
static const int ORDER_TYPE_MARKET = 1;
static const int ORDER_TYPE_LIMIT = 2;
static const int ORDER_TYPE_STOP = 3;

// Branch prediction hint
int orderType = ORDER_TYPE_MARKET;
if (__builtin_expect(limit > 0, 0)) {
    orderType = ORDER_TYPE_LIMIT;
} else if (__builtin_expect(limit < 0, 0)) {
    orderType = ORDER_TYPE_STOP;
}
```

**Megjegyzés**: Micro-optimalizálás, csak kritikus path-ban érdemes

---

## 10. ÖSSZEFOGLALÁS ÉS AJÁNLÁS

### 10.1 Kompatibilitás Státusz

| Kategória | Zorro Támogatás | cTrader Támogatás | Plugin Implementáció | Státusz |
|-----------|-----------------|-------------------|---------------------|---------|
| **MARKET Order** | ✓ Natív | ✓ Natív | ✓ **Implementálva** | ✅ OK |
| **LIMIT Order** | ✓ Natív | ✓ Natív | ✗ **Hiányzik** | ❌ CRITICAL |
| **STOP Order** | ✓ Natív (Entry) | ✓ Natív | ✗ **Hiányzik** | ❌ HIGH |
| **Absolute SL/TP** | ✓ Natív | ✓ Natív | ✓ **Implementálva** | ✅ OK |
| **Relative SL/TP** | ✗ Nem natív | ✓ Natív | ✗ **Hiányzik** | ⚠️ MEDIUM |
| **Time-In-Force** | ✓ Natív | ✓ Natív | ✗ **Hiányzik** | ⚠️ MEDIUM |
| **Guaranteed SL** | ✗ Nem natív | ✓ Natív | ✗ **Hiányzik** | ⚠️ LOW |
| **Trailing SL** | ✓ Natív | ✓ Natív | ✗ **Hiányzik** | ⚠️ MEDIUM |
| **Slippage Control** | ✗ Nem natív | ✓ Natív | ✗ **Hiányzik** | ⚠️ LOW |
| **Label/Comment** | ✗ Nem natív | ✓ Natív | ✗ **Hiányzik** | ⚠️ LOW |

### 10.2 Prioritizált Fejlesztési Roadmap

#### ✅ AZONNAL (CRITICAL)
1. **LIMIT Order támogatás** (2-4 óra)
   - orderType=2 detekció
   - limitPrice mező hozzáadása
   - Tesztelés valódi broker-rel

2. **STOP Order támogatás** (1-2 óra)
   - orderType=3 detekció
   - stopPrice mező hozzáadása
   - Entry paraméter feldolgozása

#### ⚠️ HAMAROSAN (HIGH)
3. **Time-In-Force támogatás** (2-3 óra)
   - SET_ORDERTYPE implementáció
   - timeInForce mező
   - expirationTimestamp számítás

4. **DO_CANCEL parancs** (1-2 óra)
   - PROTO_OA_CANCEL_ORDER_REQ
   - Order ID tracking javítása

#### 📋 KÉSŐBB (MEDIUM)
5. **Relative SL/TP** (2-3 óra)
   - relativeStopLoss/TP mezők
   - Automatikus detekció (absolute vs relative)

6. **Trailing Stop Loss** (3-4 óra)
   - trailingStopLoss mező
   - Zorro Trail paraméter feldolgozása

#### 🎯 OPCIONÁLIS (LOW)
7. **Guaranteed Stop Loss** (30 perc)
   - guaranteedStopLoss mező
8. **Label és Comment** (30 perc)
   - label, comment mezők
9. **Slippage Control** (1 óra)
   - MARKET_RANGE order type
   - baseSlippagePrice, slippageInPoints

### 10.3 Végső Ajánlás

**AZONNALI TEENDŐK**:
1. **Zárja le Zorro-t** és másolja az új DLL-t (digits fallback)
2. **Implementálja a LIMIT order támogatást** (CRITICAL!)
3. **Tesztelje valódi broker-fiókkal** (demo először)

**KÖZEPES TÁVON**:
- TIF támogatás hozzáadása
- STOP order finomhangolása
- Relative SL/TP opció

**HOSSZÚ TÁVON**:
- Trailing stop integráció
- Speciális order típusok (MARKET_RANGE, STOP_LIMIT)
- Advanced risk management (Guaranteed SL)

**ÖSSZEGZÉS**:
A Zorro és cTrader OpenAPI **KOMPATIBILISEK**, de a jelenlegi plugin implementáció **HIÁNYOS**. A legkritikusabb hiányzó funkció a **LIMIT order támogatás**, ami nélkül a stratégiák nagy része nem működik megfelelően.

---

**Dokumentum vége**

*Források*:
- Zorro Manual: https://manual.zorro-project.com/
- cTrader OpenAPI: https://help.ctrader.com/open-api/
- GitHub: spotware/openapi-proto-messages

*Utolsó frissítés*: 2025-10-15 13:15
*Szerző*: Claude Code (AI Assistant)
*Státusz*: ✓ Teljes kutatás kész, implementációra vár
