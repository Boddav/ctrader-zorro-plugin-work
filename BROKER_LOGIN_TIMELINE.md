# BROKER LOGIN TIMELINE - Teljes folyamat amikor megnyomod a TRADE gombot

## 1. WEBSOCKET KAPCSOLÓDÁS
```
→ Network::Connect()
↓
✓ WebSocket kapcsolat felépítve
```

## 2. ACCOUNT AUTHENTICATION (SYNC)
```
Line 1354-1451: TryAccountAuth()

SEND → 2102 (ACCOUNT_AUTH_REQ) - ctidTraderAccountId + accessToken
↓ WAIT (BLOCKING)
RECV ← 2103 (ACCOUNT_AUTH_RES)
↓
✓ Account authenticated
```

## 3. TRADER INFO REQUEST (ASYNC - NEM VÁRUNK RÁ!)
```
Line 1453-1460:

SEND → 2110 (TRADER_REQ) - leverage, balance info kérése
↓ (folytatjuk, nem várunk válaszra!)
```

## 4. ORDER & DEAL LIST REQUEST (ASYNC - NEM VÁRUNK RÁ!)
```
Line 1464-1465:

SEND → 2138 (ORDER_LIST_REQ) - meglévő nyitott pozíciók kérése
SEND → 2124 (DEAL_LIST_REQ) - deal history kérése
↓ (folytatjuk, nem várunk válaszra!)
```

## 5. SYMBOL LOADING - CACHE VAGY FETCH
```
Line 1468: Symbols::LoadAssetCache()

IF Cache létezik (AssetList.txt):
   ✓ Load from cache
   → Ugrás 6. lépésre

ELSE Cache NINCS:
   Line 1480-1629: SYMBOLS_LIST_REQ (SYNC)

   SEND → 2114 (SYMBOLS_LIST_REQ)
   ↓ WAIT (BLOCKING)
   RECV ← 2115 (SYMBOLS_LIST_RES) - TELJES symbol lista
                                     • symbolId
                                     • digits
                                     • assetId
                                     • pipPosition
                                     • pipSize
                                     • tradingMode
                                     • minVolume
                                     • maxVolume
                                     • lotSize
                                     • swapLong
                                     • swapShort
                                     • swapRollover3Days
   ↓
   Parse symbols: Line 1524-1623
   ✓ Symbols loaded: AddSymbol() minden symbolra
```

## 6. ASSET LIST FETCH (SYNC - DE EZ A PROBLÉMA!)
```
Line 1631: Symbols::FetchAssetList()

SEND → 2112 (ASSET_LIST_REQ)
↓ WAIT (BLOCKING) - Network::Receive()

⚠️ PROBLÉMA: Most VÁR egy választ, de melyik érkezik először?

Lehetséges válaszok a sorban:
   - 2122 (TRADER_RES) - a 3. lépés válasza!
   - 2139 (ORDER_LIST_RES) - a 4. lépés válasza!
   - 2125 (DEAL_LIST_RES) - a 4. lépés válasza!
   - 2113 (ASSET_LIST_RES) - a most küldött 2112 válasza

IF FetchAssetList megkapja a 2113-at ELSŐNEK:
   ✓ Parse asset list
   ✓ Update symbol metadata (pipPosition, pipSize, digits)

ELSE FetchAssetList megkapja a 2122/2139/2125-öt ELSŐNEK:
   ✗ FAIL: "Unexpected payload type"
   → return false
   → A 2113 KÉSŐBB érkezik
   → NetworkThread megkapja
   → PAYLOAD_UNHANDLED (mert nincs case rá a switch-ben!)
```

## 7. GENERATE AssetList.txt
```
Line 1638: Symbols::GenerateBrokerAssetsFile()

→ Írja az AssetList.txt file-t: Name;SymbolId;Digits;Source
```

## 8. METADATA REQUEST (ASYNC)
```
Line 1640-1644:

SEND → 2153 (ASSET_CLASS_LIST_REQ) - asset osztályok (forex, metals, stb)
SEND → 2160 (SYMBOL_CATEGORY_REQ) - symbol kategóriák
```

## 9. SPOT PRICE SUBSCRIPTIONS (ASYNC)
```
Line 1651: Symbols::SubscribeToSpotPrices()

SEND → 2117 (SPOT_SUBSCRIBE_REQ) × 45 priority symbols
      (EURUSD, GBPUSD, USDJPY, XAUUSD, stb.)
```

## 10. START NETWORK THREAD
```
Line 1658: _beginthreadex(&NetworkThread)

NetworkThread MOST INDUL és kezdi olvasni az üzeneteket!

INNENTŐL a válaszok aszinkron érkeznek:
   ← 2122 (TRADER_RES) - 3. lépés válasza
   ← 2139 (ORDER_LIST_RES) - 4. lépés válasza
   ← 2125 (DEAL_LIST_RES) - 4. lépés válasza
   ← 2113 (ASSET_LIST_RES) - 6. lépés válasza (ha még nem érkezett meg!)
   ← 2154 (ASSET_CLASS_LIST_RES) - 8. lépés válasza
   ← 2161 (SYMBOL_CATEGORY_RES) - 8. lépés válasza
   ← 2118 (SPOT_SUBSCRIBE_RES) × 45 - 9. lépés válaszai
   ← 2121 (SPOT_EVENT) - Árak érkeznek folyamatosan!
```

## 11. REQUEST ACCOUNT INFO (ASYNC)
```
Line 1666: Account::RequestAccountInfo()

SEND → 2110 (TRADER_REQ) újra - balance, equity, margin
```

## 12. WAIT FOR QUOTES
```
Line 1671-1678: Max 5 sec várakozás

→ Várunk hogy legalább 1 quote érkezzen (G.quoteCount > 0)
✓ Quotes ready

RETURN 1 - LOGIN SUCCESS!
```

---

## A PROBLÉMA ÖSSZEFOGLALVA

### Mi történik a 6. lépésben?

**`FetchAssetList()` BLOKKOLVA VÁR** egy 2113 (ASSET_LIST_RES) válaszra, DE:

1. A 3-4-5. lépésekben küldött requestek válaszai **mind a queue-ban vannak**
2. `Network::Receive()` **AZ ELSŐ** üzenetet olvassa ki
3. **Ha NEM 2113 az első** → **FAIL** → "Asset list sync failed"
4. **Ha 2113 az első** → **SUCCESS**, de nagyon ritkán fordul elő!

### Miért látjuk a PAYLOAD_UNHANDLED üzenetet?

```
[timestamp] NET_RECV_ALL: NetworkThread received payloadType=2113 (PROTO_OA_ASSET_LIST_RES)
[timestamp] PAYLOAD_UNHANDLED: Unhandled payloadType=2113 (PROTO_OA_ASSET_LIST_RES)
```

**Mert:**
- A FetchAssetList() elszalasztotta a 2113-at (mert előbb jött egy 2122)
- A NetworkThread később megkapja a 2113-at
- A NetworkThread switch statement-ben **NINCS case a 2113-ra**
- Ezért default case → PAYLOAD_UNHANDLED

### Miért hiányzik a lotSize, swap és volume adat?

**Két ok:**

1. **Cache használat:** Ha cache létezik (5. lépés), akkor a 2114 (SYMBOLS_LIST_REQ) **NEM kerül elküldésre**
   - Cache formátum: `Name;SymbolId;Digits;Source`
   - **HIÁNYZIK:** lotSize, minVolume, maxVolume, swapLong, swapShort

2. **API nem küldi az optional mezőket:** Még ha a 2114-et elküldjük is, a cTrader API gyakran NEM küldi ezeket az optional mezőket:
   - `lotSize` (optional)
   - `minVolume` (optional)
   - `maxVolume` (optional)
   - `swapLong` (optional)
   - `swapShort` (optional)

### Mi a jelenlegi működés?

**JÓ ÉS MŰKÖDIK:**
- "EUR/USD 572 ticks read" → Árak jönnek, quote subscription működik ✓
- Symbol lookup működik ✓
- Trading működik ✓

**ROSSZ:**
- Hardcoded fallback értékek a BrokerAsset()-ben (lotSize=100000, pip=10, stb.)
- Asset metadata elvész ha cache-t használunk
- FetchAssetList() nem megbízható (race condition)

---

## KÖVETKEZŐ LÉPÉSEK

### Opció A: Fix FetchAssetList race condition
- Írjuk át FetchAssetList()-et ASYNC-ra
- Adjunk hozzá case-t a NetworkThread-hez a 2113-ra

### Opció B: Használjuk a SYMBOLS_LIST_RES adatokat
- A 2115 válasz **TARTALMAZZA** a swap/volume mezőket (ha az API küldi)
- Frissítsük a cache formátumot hogy tárolja ezeket
- Force fetch ha cache elavult

### Opció C: Használjuk a 2113 (ASSET_LIST_RES) adatokat
- Nézzük meg pontosan mit tartalmaz
- Ha tartalmaz asset-level metadatát, használjuk
- De ez valószínűleg NEM symbol-specifikus swap/volume!

### Melyiket csináljuk?
