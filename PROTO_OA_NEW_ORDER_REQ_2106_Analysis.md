# PROTO_OA_NEW_ORDER_REQ (2106) - Teljes Analízis

## 1. Alapinformációk

**Üzenet típus**: `PROTO_OA_NEW_ORDER_REQ`
**Payload Type szám**: `2106`
**Cél**: Új kereskedési megbízás létrehozása a cTrader szerverén

---

## 2. Aktuális Implementáció (trading.cpp:196-203)

### Küldött JSON üzenet struktúra:
```json
{
  "clientMsgId": "msg_XXX",
  "payloadType": 2106,
  "payload": {
    "ctidTraderAccountId": 44533070,
    "symbolId": 5,
    "orderType": 1,
    "tradeSide": 1,
    "volume": 100000,
    "stopLoss": 1.05500,      // ← OPCIONÁLIS - ha stopPrice > 0
    "takeProfit": 1.08000     // ← OPCIONÁLIS - ha limit > 0
  }
}
```

### Implementált mezők (trading.cpp:196-203):

| Mező neve | Típus | Kötelező | Implementálva | Megjegyzés |
|-----------|-------|----------|---------------|------------|
| **clientMsgId** | string | ✓ | ✓ | Egyedi üzenet azonosító (Utils::GetMsgId()) |
| **payloadType** | int | ✓ | ✓ | Mindig 2106 |
| **ctidTraderAccountId** | long long | ✓ | ✓ | G.CTraderAccountId |
| **symbolId** | long long | ✓ | ✓ | info->id (symbol lookup alapján) |
| **orderType** | int | ✓ | ✓ | **Fix: 1** (MARKET order) |
| **tradeSide** | int | ✓ | ✓ | 1=BUY, 2=SELL |
| **volume** | long long | ✓ | ✓ | volumeInCents (×1 szorzó) |
| **stopLoss** | double | ✗ | ✓ | Csak ha stopDist > 0 |
| **takeProfit** | double | ✗ | ✓ | Csak ha limit > 0 |

---

## 3. cTrader API Támogatott Mezők (teljes specifikáció)

A cTrader PROTO_OA_NEW_ORDER_REQ üzenet **TELJES** mezőlistája a hivatalos API alapján:

### 3.1 Kötelező mezők ✓
- `ctidTraderAccountId` (int64) - Trader account ID
- `symbolId` (int64) - Symbol ID a kereskedéshez
- `orderType` (enum ProtoOAOrderType) - Order típus
- `tradeSide` (enum ProtoOATradeSide) - BUY vagy SELL
- `volume` (int64) - Volume "cents" formátumban

### 3.2 Opcionális mezők - RÉSZBEN implementálva
- `stopLoss` (double) - **✓ Implementálva**
- `takeProfit` (double) - **✓ Implementálva**
- `relativeStopLoss` (int64) - **✗ NEM implementálva** - Relatív SL pips-ben
- `relativeTakeProfit` (int64) - **✗ NEM implementálva** - Relatív TP pips-ben
- `guaranteedStopLoss` (bool) - **✗ NEM implementálva** - Garantált SL
- `trailingStopLoss` (bool) - **✗ NEM implementálva** - Trailing SL
- `stopTriggerMethod` (enum ProtoOAOrderTriggerMethod) - **✗ NEM implementálva**

### 3.3 Opcionális mezők - LIMIT/STOP order-ekhez (NEM implementálva)
- `limitPrice` (double) - **✗ NEM implementálva** - LIMIT price
- `stopPrice` (double) - **✗ NEM implementálva** - STOP price
- `expirationTimestamp` (int64) - **✗ NEM implementálva** - Order lejárat
- `positionId` (int64) - **✗ NEM implementálva** - Meglévő pozíció módosítás
- `baseSlippagePrice` (double) - **✗ NEM implementálva** - Slippage kontroll
- `slippageInPoints` (int64) - **✗ NEM implementálva** - Max slippage
- `label` (string) - **✗ NEM implementálva** - Order címke
- `comment` (string) - **✗ NEM implementálva** - Order komment

---

## 4. Order Type Enum Értékek

### ProtoOAOrderType:
```cpp
enum ProtoOAOrderType {
    MARKET = 1,           // ✓ Implementálva (fix hardcoded)
    LIMIT = 2,            // ✗ NEM támogatott
    STOP = 3,             // ✗ NEM támogatott
    STOP_LIMIT = 4,       // ✗ NEM támogatott
    MARKET_RANGE = 5,     // ✗ NEM támogatott
    STOP_LOSS_TAKE_PROFIT = 6  // ✗ NEM támogatott (védőárfolyam-hoz)
}
```

**Probléma**: A kód **mindig orderType=1-et (MARKET) küld**, nincs támogatás LIMIT vagy STOP order-ekre!

---

## 5. Log Fájl Analízis (wesocket1.txt)

### Példa valós üzenetek a log-ból:

```json
// Példa #1 - AUDUSD vásárlás (symbolId=5)
{
  "clientMsgId": "msg_1919",
  "payloadType": 2106,
  "payload": {
    "ctidTraderAccountId": 44533070,
    "symbolId": 5,
    "orderType": 1,
    "tradeSide": 1,
    "volume": 1400000000  // 14000 standard lot (!) - ROSSZ VOLUME
  }
}
```

**Megállapítás**: A log-ban látható volume értékek (1,400,000,000 cents) NEM REÁLISAK. Ez azt jelenti, hogy a korábbi volume konverzió hibás volt.

---

## 6. Összehasonlítás: MI VAN vs MI NINCS

### ✓ MEGVALÓSÍTOTT funkciók:
1. **Alapvető MARKET order** - ctidTraderAccountId, symbolId, orderType=1, tradeSide, volume
2. **Stop Loss támogatás** - stopLoss mező (absolute price)
3. **Take Profit támogatás** - takeProfit mező (absolute price)
4. **Dinamikus volumen számítás** - Zorro amount → cTrader volume cents
5. **Szimbólum ID lookup** - Symbols::GetSymbolByIdOrName()
6. **Hibaelhárás timeout-okkal** - 15 másodperces timeout + OrderErrorEvent kezelés

### ✗ HIÁNYZÓ funkciók (ami az API TÁMOGAT, de NINCS implementálva):
1. **LIMIT order típus** - Adott áron végrehajtandó megbízás
2. **STOP order típus** - Adott ár elérése után indított market order
3. **STOP LIMIT order típus** - Kombinált stop+limit
4. **Relative Stop Loss/Take Profit** - Pips-ben megadott védőárfolyamok
5. **Guaranteed Stop Loss** - GSL (bróker garancia)
6. **Trailing Stop Loss** - Követő stop loss
7. **Slippage kontroll** - baseSlippagePrice, slippageInPoints
8. **Order lejárat (expiration)** - expirationTimestamp
9. **Order label és comment** - Megbízás címkézés
10. **Pozíció módosítás** - positionId a meglévő pozíció SL/TP frissítésére

---

## 7. Volume Konverzió Probléma Történet

### Időrendi változások (backupmentesorder.TXT alapján):
1. **Kezdeti**

: `volumeInCents = amount × 100000` → 1000 lot (ROSSZ ❌)
2. **×1000 próba**: `volumeInCents = amount × 1000` → 10 lot (ROSSZ ❌)
3. **×10 próba**: `volumeInCents = amount × 10` → 0.1 lot (ROSSZ ❌)
4. **VÉGSŐ MEGOLDÁS ✓**: `volumeInCents = amount × 1` → **1:1 mapping (HELYES ✓)**

### Aktuális implementáció (trading.cpp:207):
```cpp
long long volumeInCents = (long long)abs(amount) * 1;
```

**Magyarázat**:
- Zorro belsőleg: slider 1.0 → nAmount = 100,000
- cTrader API: 100,000 cents = 1.0 standard lot
- Ezért: **×1 szorzó = direct pass-through**

---

## 8. Potenciális Fejlesztések Prioritás Szerint

### MAGAS prioritás (fontos funkciók):
1. ✅ **Intelligent digits fallback** (JPY=3, XAU=3) - **MÁR IMPLEMENTÁLVA**
2. **LIMIT order támogatás** - Adott áron vásárlás/eladás
3. **Relative SL/TP** - Pips-ben megadott védőárfolyamok

### KÖZEPES prioritás (kényelmi funkciók):
4. **STOP order támogatás** - Breakout stratégiákhoz
5. **Order label/comment** - Megbízás azonosítás
6. **Slippage kontroll** - Maximális csúszás korlátozása

### ALACSONY prioritás (speciális funkciók):
7. **Trailing Stop Loss** - Automatikus profitkövetés
8. **Guaranteed Stop Loss** - Extra védelem
9. **Order expiration** - Időzített megbízások

---

## 9. Következtetés

### Összefoglalás:
A jelenlegi implementáció **MŰKÖDŐ**, de **KORLÁTOZOTT** funkcióval rendelkezik:
- ✓ **MARKET order-ek működnek** absolute SL/TP-vel
- ✗ **LIMIT/STOP order-ek NEM támogatottak**
- ✗ **Speciális funkciók (trailing, GSL, slippage) HIÁNYOZNAK**

### Legfontosabb hiányzó funkció:
**LIMIT order támogatás** - Ez lehetővé tenné:
- Jobb árú belépést várni (buy limit az aktuális ár alatt)
- Profitot biztosítani (sell limit nyitott long pozíció fölött)
- Stratégiák szélesebb körét használni

### Ajánlás:
1. **AZONNALI**: Zárja le Zorro-t és másolja az új DLL-t (digits fallback-kal)
2. **KÖVETKEZŐ**: Implementálja a LIMIT order típust (orderType=2, limitPrice mező)
3. **OPCIONÁLIS**: Relative SL/TP támogatás (relativeStopLoss, relativeTakeProfit)

---

## 10. Kód Referenciák

### Fő fájlok:
- **trading.cpp:196-203** - PlaceOrder üzenet építés
- **payloads.h:12** - NewOrderReq = 2106 definíció
- **main.cpp:1542-1572** - Intelligent digits fallback (ÚJ ✓)
- **symbols.cpp** - Symbol lookup és AssetList.txt generálás

### Log fájlok:
- **wesocket1.txt** - Valós 2106-os üzenetek
- **backupmentesorder.TXT** - Volume konverzió fejlődés története

---

**Utolsó frissítés**: 2025-10-15 12:52:38
**DLL verzió**: 299,520 bytes (digits fallback-kal)
**Státusz**: ✓ Build sikeres, DLL másolás PENDING (Zorro locked)
