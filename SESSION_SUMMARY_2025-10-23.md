# cTrader Plugin - Munkamenet Összefoglaló
**Dátum:** 2025. október 23.

---

## ✅ ELKÉSZÜLT JAVÍTÁSOK

### 1. INTEGER PARSING FIX - SIKERES ✓

**Probléma:**
- A cTrader API **MINDIG** integer értékeket küld (long long), 100,000-es szorzóval
- A kód `atof()` és `ExtractJsonDouble()` függvényeket használt → **lebegőpontos pontossági hibák**
- Példa API adat: `{"low":116995,"deltaOpen":10}` → helyes eredmény: 1.16995

**Megoldás:**
Mindkét helyen (WebSocket ÉS REST API) átállítva INTEGER arithmetic-re:

#### WebSocket (src/history.cpp, sorok 477, 504-514, 537-540):
```cpp
// ELŐTTE (ROSSZ):
double low, deltaOpen, deltaHigh, deltaClose;
low = atof(lowPos + 6);
tick->fOpen = (low + deltaOpen) / CTRADER_PRICE_SCALE;

// UTÁNA (JÓ):
long long low = 0, deltaOpen = 0, deltaHigh = 0, deltaClose = 0;
low = _atoi64(lowPos + 6);
deltaOpen = _atoi64(deltaOpenPos + 12);
// INTEGER arithmetic ELŐSZÖR, UTÁNA osztás:
tick->fOpen = (double)(low + deltaOpen) / 100000.0;
tick->fHigh = (double)(low + deltaHigh) / 100000.0;
tick->fLow = (double)low / 100000.0;
tick->fClose = (double)(low + deltaClose) / 100000.0;
```

#### REST API (src/history_rest.cpp, sorok 190-195, 212-215):
```cpp
// ELŐTTE (ROSSZ):
double openVal, highVal, lowVal, closeVal;
ExtractJsonDouble(obj, "\"low\"", lowVal);

// UTÁNA (JÓ):
long long lowVal = 0;
long long deltaOpen = 0, deltaHigh = 0, deltaClose = 0;
ExtractJsonLongLong(obj, "\"low\"", lowVal);
ExtractJsonLongLong(obj, "\"deltaOpen\"", deltaOpen);
// INTEGER arithmetic:
bar.low = (double)lowVal / scale;
bar.open = (double)(lowVal + deltaOpen) / scale;
bar.high = (double)(lowVal + deltaHigh) / scale;
bar.close = (double)(lowVal + deltaClose) / scale;
```

**Ellenőrzés (wesocket.txt log, 22:01:34):**
```
✓ EURUSD: raw bid=116152 → 116152/100000 = 1.161520 (HELYES)
✓ AUDUSD: raw bid=65112 → 65112/100000 = 0.651120 (HELYES)
✓ Történeti bar: low=115875 → 115875/100000 = 1.15875 (HELYES)
```

**Státusz:** ✅ FIX MŰKÖDIK - mindkét árképzési útvonal (spot ÉS history) helyes!

---

### 2. BAR SORREND VIZSGÁLAT - NINCS SZÜKSÉG REVERSE-RE ✓

**Kérdés volt:** A cTrader API fordított sorrendben küldi-e a bar-okat (újabb→régebbi)?

**Válasz (wesocket.txt log alapján):**
```
BAR[0] timestampMinutes=29352167 (legrégebbi)
BAR[1] timestampMinutes=29352168
BAR[2] timestampMinutes=29352169 (legújabb)
```

**Következtetés:**
- WebSocket API **HELYES SORRENDBEN** küldi: régi → új (időrendi)
- Zorro is ezt várja: régi → új
- **NEM KELL std::reverse()!**

**Jules verzió megjegyzés:**
- REST API-nál (`history_rest.cpp:153-154`) Jules **használ** reverse-t
- WebSocket API-nál (`jules/history.cpp`) **NEM használ** reverse-t
- Ez megerősíti: WebSocket = helyes sorrend, REST = fordított sorrend

---

### 3. ERROR 030 MEGOLDÁS - T6 FÁJLOK TÖRÖLVE ✓

**Hiba:** "Error 030: History\EURUSD.t6 invalid dates: 2025-01-01 - 2024-10-18"

**Ok:** Régi, korrupt T6 történeti fájlok (fordított dátumok a fájlban)

**Megoldás:**
```bash
Remove-Item 'C:\Users\Administrator\Desktop\z3\History\*.t6' -Force
```

**Státusz:** ✅ Régi fájlok törölve, következő letöltés már tiszta lesz

---

## 📁 MÓDOSÍTOTT FÁJLOK

1. **C:\Users\Administrator\source\repos\zorro-plugin-windows-32-3\src\history.cpp**
   - Sorok 477: `long long` típusra váltás
   - Sorok 504-514: `_atoi64()` parsing
   - Sorok 537-540: INTEGER arithmetic (`(low + deltaOpen) / 100000.0`)
   - Sor 655: Tick price fix (`(double)absoluteTick / 100000.0`)

2. **C:\Users\Administrator\source\repos\zorro-plugin-windows-32-3\src\history_rest.cpp**
   - Sorok 190-191: `long long` típusra váltás
   - Sorok 192-195: `ExtractJsonLongLong()` használata
   - Sorok 212-215: INTEGER arithmetic

3. **Telepített DLL-ek (frissítve 2025-10-23 21:54:53):**
   - `C:\Users\Administrator\Desktop\z3\Plugin\cTrader.dll`
   - `C:\Users\Administrator\Desktop\z3\cTrader.dll`

---

## 🔍 FONTOS FELFEDEZÉSEK

### Python Referencia Implementáció
A helyes megoldás forrása:
```python
# download_history.py:229-232
low = bar['low'] / 100000.0
open_price = (bar['low'] + bar['deltaOpen']) / 100000.0

# download_ticks.py:323
price = abs_tick / math.pow(10, self.symbol_digits)
```

### cTrader API Scaling
- **MINDIG 100,000× szorzó**, függetlenül a digits értéktől!
- Spot árak: integer × 100,000
- Delta encoding: `low` (abszolút), `deltaOpen/High/Close` (relatív)
- Formula: `open = (low + deltaOpen) / 100000.0`

---

## 📋 KÖVETKEZŐ LÉPÉSEK (HOLNAPRA)

### 1. TESZTELÉS ✓
- [ ] Indítsd el a Zorro-t
- [ ] Töltsd le az EURUSD történeti adatokat
- [ ] Ellenőrizd a `C:\Users\Administrator\Desktop\z3\History\EURUSD_2025.t6` fájlt
- [ ] Nézd meg a `TradeTest_diag.txt` log-ot
- [ ] Győződj meg arról, hogy **NEM** jelenik meg Error 030

### 2. ÁRAK ELLENŐRZÉSE
Nézd meg, hogy az árak helyesek:
```
EURUSD: ~1.16xxx tartomány (NEM 1.89 vagy más furcsa érték)
AUDUSD: ~0.65xxx tartomány
```

### 3. HA TOVÁBBRA IS PROBLÉMA VAN

#### Ha még mindig Error 030 látszik:
1. Ellenőrizd a log-ban az első és utolsó bar időbélyegét
2. Keress rá: `HISTORY_FIRST_TICK` és `ORDER_CHECK` bejegyzésekre
3. Ha fordított sorrend, akkor **MÉGIS** kell reverse

#### Ha rossz árak:
1. Ellenőrizd a `wesocket.txt` log-ban: `HISTORY_FIRST_BAR_RAW`
2. Nézd meg a raw integer értékeket
3. Számold ki manuálisan: `low / 100000`
4. Ha nem egyezik → DLL nem frissült

---

## 🛠️ BUILD PARANCS (HA KELL)

```bash
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
"C:\Users\Administrator\source\repos\zorro-plugin-windows-32-3\cTrader.vcxproj" ^
-p:Configuration=Release -p:Platform=Win32 -t:Rebuild
```

Utána másold át:
```bash
copy "C:\Users\Administrator\source\repos\zorro-plugin-windows-32-3\Release\cTrader.dll" ^
     "C:\Users\Administrator\Desktop\z3\Plugin\cTrader.dll"
copy "C:\Users\Administrator\source\repos\zorro-plugin-windows-32-3\Release\cTrader.dll" ^
     "C:\Users\Administrator\Desktop\z3\cTrader.dll"
```

---

## 📝 DEBUG LOG HELYEK

- **WebSocket események:** `C:\Users\Administrator\Desktop\z3\Plugin\wesocket.txt`
- **Zorro Trade log:** `C:\Users\Administrator\Desktop\z3\Log\TradeTest_diag.txt`
- **Download log:** `C:\Users\Administrator\Desktop\z3\Log\Download_EURUSD_diag.txt`
- **History fájlok:** `C:\Users\Administrator\Desktop\z3\History\*.t6`

---

## ✅ ÖSSZEFOGLALÁS

**ÁR PARSING:** ✅ MŰKÖDIK
- Integer arithmetic használva
- WebSocket és REST API is javítva
- Log megerősíti a helyes árakat (1.16152, 0.65112)

**BAR SORREND:** ✅ HELYES
- WebSocket API időrendben küldi
- NEM kell reverse
- Jules verzió is ezt erősíti meg

**T6 FÁJLOK:** ✅ TISZTÍTVA
- Régi korrupt fájlok törölve
- Következő letöltés friss lesz

**KÖVETKEZŐ:** Tesztelés Zorro-val, történeti letöltés ellenőrzése

---

**Készítette:** Claude Code Assistant
**Dátum:** 2025-10-23 22:15
