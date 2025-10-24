# cTrader API Volume Conversion Problem

## Problem Statement

I'm developing a Zorro trading plugin for cTrader API. The volume conversion works correctly for forex pairs (EURUSD, etc.) but fails for gold (XAUUSD) - it sends 100x too much volume.

**Current Behavior:**
- Forex (EURUSD): Works correctly
- Gold (XAUUSD): When slider = 1, it buys 100 lots instead of 0.01 lots

## Technical Context

### cTrader API Volume System

cTrader API uses "cents" for volume, where:
- Volume is sent in "cents" (integer, not decimal)
- Each symbol has a `lotSize` field (how many cents = 1 standard lot)
- Each symbol has a `digits` field (decimal precision for prices)

**Example values from cTrader server:**

For EURUSD (forex):
```
digits = 5
lotSize = 10,000,000 cents
```

For XAUUSD (gold):
```
digits = 2 (or 3)
lotSize = 10,000 cents
```

### Zorro Internal Representation

Zorro represents volume as:
- User slider value (e.g., 2.0 for 2 lots)
- Internal value `nAmount` = slider_value × 100,000
- Example: slider = 2.0 → nAmount = 200,000

## Current Formula (NOT WORKING)

```cpp
// Current formula that fails for gold:
if (info->lotSize > 0 && info->digits > 0) {
    double digitScale = std::pow(10.0, info->digits);
    double scaleFactor = static_cast<double>(info->lotSize) / digitScale;
    volumeInCents = static_cast<long long>(static_cast<double>(absAmount) * scaleFactor);
}
```

**For EURUSD (works):**
- nAmount = 200,000 (slider = 2.0)
- digits = 5, digitScale = 100,000
- lotSize = 10,000,000
- scaleFactor = 10,000,000 / 100,000 = 100
- volumeInCents = 200,000 × 100 = 20,000,000 cents ✓

**For XAUUSD (fails):**
- nAmount = 200,000 (slider = 2.0)
- digits = 2, digitScale = 100
- lotSize = 10,000
- scaleFactor = 10,000 / 100 = 100
- volumeInCents = 200,000 × 100 = 20,000,000 cents ✗ (WAY TOO MUCH!)

## Questions

1. **What is the correct formula** to convert from Zorro's `nAmount` (slider × 100,000) to cTrader's `volumeInCents` using `lotSize` and `digits`?

2. **What should volumeInCents be** for the following scenarios?
   - EURUSD: slider = 2.0 (want 2 standard lots)
   - XAUUSD: slider = 2.0 (want 2 standard lots)
   - XAUUSD: slider = 1.0 (want 1 standard lot, currently buys 100 lots!)

3. **Is there a fundamental misunderstanding** in how `digits` relates to volume conversion? Should `digits` even be used for volume, or only for price scaling?

4. **What is the relationship** between:
   - 1 standard lot
   - lotSize in cents
   - digits value
   - volumeInCents to send in API

## Reference Implementation

The same codebase has a working "lookback" (historical data) feature that uses:
```cpp
const double scale = (digits > 0) ? std::pow(10.0, digits) : 100000.0;
```

But this is for **price scaling**, not volume conversion. Should volume use a completely different approach?

## Additional Context

- Both `lotSize` and `digits` are cached from server responses (not queried dynamically)
- The values are confirmed to be retrieved correctly from the server
- The same formula is applied consistently to both PlaceOrder and ClosePosition functions
- Multiple formula variations have been tried, all producing incorrect results for gold

## Expected Behavior

When the user sets slider to 1.0:
- Should open 1 standard lot (not 0.01, not 100)
- For both forex and gold/metals
- Using symbol-specific lotSize and digits values from server
