# CRITICAL BUGS FIXED - LESSON LEARNED

## 2025-10-11: T6 STRUCT DEFINITION MISMATCH (ROOT CAUSE!)

### THE REAL BUG:
Our plugin's `zorro.h` defined T6 struct **COMPLETELY WRONG**:

**OUR WRONG DEFINITION (zorro.h:32-39):**
```cpp
typedef struct T6 {
    DATE time;      // double ✓
    double fOpen;   // ❌ WRONG TYPE AND ORDER!
    double fHigh;   // ❌ WRONG TYPE!
    double fLow;    // ❌ WRONG TYPE!
    double fClose;  // ❌ WRONG TYPE!
    double fVal;    // ❌ WRONG TYPE!
} T6;
```

**ZORRO'S ACTUAL DEFINITION (trading.h:59-65):**
```cpp
#pragma pack(push,4)
typedef struct T6 {
    DATE time;           // double ✓ (8 bytes)
    float fHigh, fLow;   // ✓ FLOAT! (4 bytes each)
    float fOpen, fClose; // ✓ DIFFERENT ORDER!
    float fVal, fVol;    // ✓ 7 fields total, not 6!
} T6;
#pragma pack(pop)
```

### CRITICAL DIFFERENCES:
1. **Type**: All price/volume fields are `float` (4 bytes), NOT `double` (8 bytes)!
2. **Order**: Fields are `fHigh, fLow, fOpen, fClose` NOT `fOpen, fHigh, fLow, fClose`!
3. **Count**: 7 fields total (includes `fVol`), not 6!
4. **Packing**: Uses `#pragma pack(push,4)` for 4-byte alignment

### WHY THIS CAUSED CORRUPTION:
When we wrote doubles (8 bytes) where Zorro expected floats (4 bytes):
- Writing `tick->fOpen = 1.16130` (8 bytes) would overflow into `fHigh`'s space
- Each double write corrupted the next field
- Result: Complete field misalignment
- Symptom: `close` field contained timestamp values, `time` field contained prices!

### SYMPTOMS:
```python
# Reading T6 file showed MIXED UP fields:
Bar 0: time=45940.66736148 open=1.54131 high=45940.70069444 low=1.52867 close=45940.66458333 vol=1.16130
       ^^^^^^^^^^^^^^^^ price!    ^^^^^^^^^^^^^^^^^^^^^ timestamp!    ^^^^^^^^^^^^^^^^^ timestamp!  ^^^^^^ price!
```

Fields were completely shuffled because of size/order mismatch!

### THE FIX:
1. **Updated zorro.h** to match Zorro's actual definition:
   ```cpp
   typedef struct T6 {
       DATE time;              // double
       float fHigh, fLow;      // float - correct order!
       float fOpen, fClose;    // float
       float fVal, fVol;       // float - all 7 fields
   } T6;
   ```

2. **Updated all T6 writes** (3 locations in history.cpp) to cast to float:
   ```cpp
   tick->fHigh = static_cast<float>(bar.high);   // ✓ Correct!
   tick->fLow = static_cast<float>(bar.low);
   tick->fOpen = static_cast<float>(bar.open);
   tick->fClose = static_cast<float>(bar.close);
   tick->fVal = static_cast<float>(bar.volume);
   tick->fVol = 0.0f;
   ```

### LESSON LEARNED:
**ALWAYS verify struct definitions against the ACTUAL Zorro headers!**
- Check `C:\...\Zorro\include\trading.h` for real definitions
- Don't trust plugin stub headers - they may be wrong!
- Field order and size MUST match exactly for binary file compatibility
- The 'f' prefix means "field", not "float" (but in T6, they ARE floats!)

---

## 2025-10-11: T6 Float Cast Corruption (SUPERSEDED BY ABOVE)

### THE BUG:
In **THREE LOCATIONS** in `history.cpp`, we were casting double values to float before storing in T6 struct:

1. **Line 703-707**: Trendbar response (WebSocket delta encoding)
2. **Line 757-762**: Historical data (REST API JSON parsing)
3. **Line 877-879**: Tick data generation

```cpp
tick->fOpen = static_cast<float>(bar.open);   // ❌ WRONG!
```

**CRITICAL:** All three locations must be fixed, not just one!

### WHY IT'S WRONG:
The T6 struct (defined in `zorro.h:32-39`) uses **DOUBLE** fields, not float:
```cpp
typedef struct T6 {
    DATE time;      // double
    double fOpen;   // double!
    double fHigh;   // double!
    double fLow;    // double!
    double fClose;  // double!
    double fVal;    // double!
} T6;
```

When you cast `double → float → double`, the bit pattern gets corrupted because:
- float is 32-bit, double is 64-bit
- Intermediate float conversion loses precision
- Reinterpreting as double creates garbage values

### SYMPTOMS:
- T6 file had completely wrong data
- Bar 0: `close = 45940.66736` (timestamp value in price field!)
- Bar 1: `time = 1.54130995` (price value in timestamp field!)
- Fields were misaligned/shuffled

### THE FIX:
Remove all `static_cast<float>()` casts - assign double directly:
```cpp
tick->fOpen = bar.open;    // ✓ Correct!
tick->fHigh = bar.high;
tick->fLow = bar.low;
tick->fClose = bar.close;
tick->fVal = bar.volume;
```

### LESSON LEARNED:
**ALWAYS check zorro.h for the ACTUAL type definitions!**
- Don't assume field names like `fOpen` mean "float"
- The 'f' prefix means "field", not "float"
- Verify struct definitions before casting

### HOW TO DETECT:
If prices look corrupted in T6 files:
1. Read T6 file with Python script: `read_t6_eurusd.py`
2. Check if values are wildly wrong or mixed up
3. Look for float casts in history.cpp where we write to T6
4. Compare against zorro.h struct definition

---

## Other Critical Fixes (Same Session):

### Delta Encoding - Parentheses Matter!
**WRONG:**
```cpp
double baseLow = lowVal / scale;
record.open = baseLow + (deltaOpen / scale);  // Mathematically correct but...
```

**RIGHT:**
```cpp
record.open = (lowVal + deltaOpen) / scale;   // ✓ Follow Python formula exactly!
```

Parentheses order affects floating point precision. Always match the reference implementation (Python).

### Base Asset Digits - WRONG CONCEPT!
Initially thought deltas use different scale (baseAssetDigits). **WRONG!**

cTrader API documentation clearly states:
- ALL values (low, deltaOpen, deltaHigh, deltaClose) use **SAME scale**: `1/100000` or `10^digits`
- There is NO separate scale for deltas

**Python proof:**
```python
open = (low + deltaOpen) / 100_000  # Same scale for all!
```

---

## Testing Checklist After History Fixes:
1. ✅ Delete old T6 files
2. ✅ Rebuild DLL
3. ✅ Copy to Zorro
4. ✅ Run history download
5. ✅ Run `read_t6_eurusd.py` to verify T6 contents
6. ✅ Check prices are sensible (EURUSD ~1.16, XAUUSD ~4000)
