# ML-DRIVEN Strategy (v7) — Meta-Learning + Dual Entry + FTMO

## Architecture
- **3 fájl pipeline**: MLDATACOLLECTION.c → TENSORFLOWMODEL.py → MLDRIVEN.c
- **Zorro mappa**: `Desktop/z3/Strategy/`
- **Python server**: port 5001, Flask + XGBoost (32-bit Python)

## Pipeline Flow
```
1. MLDATACOLLECTION [Train] → WFO optimize (10 ciklus × 3 asset × 12 param)
2. MLDATACOLLECTION [Test]  → MLTrainingData.csv + MLTradeData.csv
3. train_and_serve_meta.bat → XGBoost train + Flask server :5001
4. MLDRIVEN [Test/Trade]    → HTTP /predict (params) + /filter (trade filter)
```

## Sliders (max 4: 0-3)
| Slider | Neve | Értékek |
|---|---|---|
| 0 | Period | Zorro default |
| 1 | Risk%x10 | 5-30 (10=1.0%) |
| 2 | Algo | 1=Both 2=SMA 3=CH |
| 3 | FTMO | 1=Off 2=1-Step 3=2-Step |

## Server Endpoints (port 5001)
- `POST /predict` — 12 features → 12 params (TESTMODE: naponta 1× hr==12, TRADEMODE: óránként)
- `POST /filter` — GO/SKIP (SMA: threshold 0.45, CH: threshold 0.40)
- `POST /retrain` — retrain both models
- `GET /health` — status
- **MemoryError fix**: /predict throttle TESTMODE-ban (157k→6.5k hívás)

## Dual Entry System (v7)
- **SMA**: Crossover + RSI + Regime + ML filter | Pyramid MAX_LAYERS=4 | LifeTime=0
  - Exit: crossover + TREND-CLOSE (smaTrendS/smaTrendL) + stop
  - Partial close: pip-based LIFO (partialPip=30)
  - Cooldown: addCooldown=12 bar (első entry-re is!)
- **CH**: LinReg Channel + Regime + ML filter | Fix lot | LifeTime=0
  - Exit: CH decision tree + session end + stop
  - Decision tree: middle+weak momentum+no room → close; breakout → hold
- **FCFS**: First Come First Served — egy algo aktív, másik vár

## FTMO Risk Management (v7 — 2026-03-07)
- **ftmoStopped**: 0=aktív, 1=daily limit (resets next day), 2=max loss (permanent), 3=target reached (permanent)
- **1-Step**: 3% daily loss, 10% max trailing, Best Day Rule 50%
- **2-Step**: 5% daily loss, 10% max, no Best Day Rule
- **Profit target**: 10% → challenge complete, stop trading

## v7 Eredmények (2026-03-07)
### Legjobb Normal mód:
- PF 1.60, SR 0.91, CAGR 18.59%, Win 55%, 969 trade
- Monte Carlo Median AR 145%

### FTMO 1-Step:
- PF 1.59, SR 0.66, Win 3992$, DD 1092$, 245 trade
- Profit target reached ✅

### FTMO 2-Step:
- PF 1.55, SR 0.60, Win 3615$, DD 1017$, 242 trade
- Profit target reached ✅

## Tesztelt és ELVETETT fejlesztések (v7)
1. **Trailing stop SMA** (Trail=2×ATR) → PF 1.60→1.49 RONT
2. **CH RegLine target** (close at middle) → PF 1.60→1.51 RONT
3. **FILTER-CLOSE** (ML-alapú layer zárás) → PF RONT, túl sok szerver hívás
4. **CH-REVERSE exit** (Close[1]/[0] RegLine cross) → semleges, belépésnek jobb
5. **TREND-CLOSE tanárban** → PF 1.09→1.04 RONT (tanárban nem kell, phantom-ban igen)

## Mi MŰKÖDÖTT (v7):
1. **SMA /filter bekapcsolás** → PF 1.02→1.60 (LEGJOBB javulás!)
2. **CH /filter** (threshold 0.40) → phantom trade-ek szűrése
3. **TREND-CLOSE diákban** → SMA layer zárás trend forduláskor
4. **/predict throttle** → TESTMODE naponta 1×, szerver stabilitás
5. **FTMO risk management** → PF javul, DD 3× kisebb

## Tanár-Diák szinkron (v7)
| Feature | Tanár | Diák |
|---|---|---|
| SMA crossover exit | ✅ | ✅ |
| SMA TREND-CLOSE (real trades) | ❌ (rontja) | ✅ |
| SMA TREND-CLOSE (phantom) | ✅ | n/a |
| CH exit | LifeTime + session end | decision tree + session end |
| CH LifeTime | optimize() | 0 |
| ML filter | n/a | ✅ (SMA 0.45, CH 0.40) |
| ML predict | n/a | ✅ (hourly/daily) |
| FTMO | n/a | ✅ (slider) |

## 12 Market Features (shared)
ATR_Pct, Range_Pct, Volatility, ADX, Trend_Bias, Trend_Quality, RSI, Hurst, Return_20, BB_Width, WinRate, Current_State

## 12 Predicted Params
smaTF, FastMA, SlowMA, smaStop_x10, adxSMA, mmiSMA, N, Factor_x100, chStop_x10, lifeTime, adxCH, mmiCH

## Tanár verzió történet (2026-03-07)
- **MLDATACOLLECTION v6** (JELENLEGI, JÓ): 12 param (6 SMA + 6 CH), algo("SMA")+algo("CH"), FCFS, H4 ATR
  - Eredeti tréning CSV-k → diák PF 1.59, 1213 trade, CAGR 13.54% (6 év)
  - **Backup CSV-k**: `Strategy/c backup/MLTrainingData.csv` + `MLTradeData.csv` (3007+2657 sor)
  - Újratréning PF 1.01 (ROSSZ) — NE tréningezd újra! Backup CSV = biztos alap
- **MLDATACOLLECTION v4** (.bak): 8 közös param, nincs algo szeparáció — PF 0.86 BUKÁS
- **MLDATACOLLECTION v7** (7 asset): 7×12=84 optimize param → WFO nem bírja! PF 0.73 tanár, PF 0.84 diák — BUKÁS
  - "no EUR/USD:SMA parameters at walk 9" — WFO konvergencia hiba
- **Tanulság**: v6 tanár jó, de CSAK az eredeti CSV-kkel! Újratréning kockázatos (WFO instabil)
- **Tanulság**: 3 asset az optimum! 7 asset = túl sok optimize param (84) a WFO-nak

## Known Issues & Fixes
- series() MUST be before `continue`
- lite-C: no `bool`, no ternary, no string concat with macros
- lite-C: array size MUST be literal number
- TradeProfit garbage in for(current_trades) → use pip calc instead
- Python 32-bit server: MemoryError if too many requests → throttle
- Filter CH threshold: 0.45=94% SKIP (too strict), 0.25=OK, 0.40=best

## Assets
EUR/USD, GBP/USD, USD/JPY (3 assets)

## Bat File
`train_and_serve_meta.bat` — cd to Strategy/, runs TENSORFLOWMODEL.py
