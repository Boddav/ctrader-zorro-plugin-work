# cTrader Volume Konverzió Helyes Megértése

## A probléma gyökere:

A `lotSize` mező **NEM** szorzó faktor! A `lotSize` azt mondja meg, hogy **hány cent = 1 standard lot**.

## Helyes értelmezés:

### EURUSD:
- lotSize = 10,000,000 cents = 1 standard lot
- Ha 2 lotot akarunk: 2 × 10,000,000 = 20,000,000 cents ✓

### XAUUSD (arany):
- lotSize = 10,000 cents = 1 standard lot
- Ha 2 lotot akarunk: 2 × 10,000 = 20,000 cents ✓

## A helyes formula:

```cpp
// Zorro nAmount = slider_value × 100,000
// Pl. slider = 2.0 → nAmount = 200,000

// Konvertáljuk Zorro "lots"-ra:
double zorroLots = static_cast<double>(nAmount) / 100000.0;
// slider = 2.0 → zorroLots = 2.0

// Konvertáljuk cTrader cents-re:
long long volumeInCents = static_cast<long long>(zorroLots * info->lotSize);
```

## Példa számítások:

### EURUSD esetén:
- slider = 2.0
- nAmount = 200,000
- zorroLots = 200,000 / 100,000 = 2.0
- volumeInCents = 2.0 × 10,000,000 = 20,000,000 cents ✓

### XAUUSD esetén:
- slider = 2.0
- nAmount = 200,000
- zorroLots = 200,000 / 100,000 = 2.0
- volumeInCents = 2.0 × 10,000 = 20,000 cents ✓

### XAUUSD, slider = 1.0:
- slider = 1.0
- nAmount = 100,000
- zorroLots = 100,000 / 100,000 = 1.0
- volumeInCents = 1.0 × 10,000 = 10,000 cents ✓ (1 standard lot)

## Miért volt rossz a régi formula?

```cpp
// ROSSZ FORMULA:
volumeInCents = nAmount × (lotSize / pow(10.0, digits))

// EURUSD:
// 200,000 × (10,000,000 / 100,000) = 200,000 × 100 = 20,000,000 ✓

// XAUUSD:
// 200,000 × (10,000 / 100) = 200,000 × 100 = 20,000,000 ✗
// Ez 2000 lotot jelent (20,000,000 / 10,000 = 2000), nem 2-t!
```

A `digits` mezőnek **SEMMI KÖZE a volume konverzióhoz**! A digits csak az ár megjelenítésére szolgál.

## Az EGYSZERŰ, HELYES megoldás:

```cpp
// 1. Zorro lots → standard lots
double standardLots = static_cast<double>(absAmount) / 100000.0;

// 2. Standard lots → cTrader cents
long long volumeInCents = static_cast<long long>(standardLots * info->lotSize);
```

ENNYI! Nincs szükség digits-re, pow()-ra, vagy bonyolult számításokra.
