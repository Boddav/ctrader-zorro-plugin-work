# Claude Memory - cTrader Zorro Projects

## Language & User Preferences
- **Hungarian** communication preferred
- Deploy: PowerShell `Copy-Item -Force` (bash cp fails on locked DLLs)
- Zorro instances: z3, z7.2.7 (both Desktop/)
- cBot deploy: 3 locations under `Documents/cAlgo/Sources/Robots/`

## Active Project: Zorro-cTrader Bridge (v2.1)
- **Repo**: `C:\Users\Administrator\source\repos\zorro-ctrader-bridge\`
- **Architecture**: Zorro DLL (C++ x86) ←TCP:5555 JSON→ cBot (C#) in cTrader
- See [zorro_ctrader_bridge.md](zorro_ctrader_bridge.md) for full details

## WebSocket Plugin (v4.7.0) — Saját/Developer verzió
- **Dir**: `C:\Users\Administrator\source\repos\zorro-plugin-windows-32-4\`
- **Status**: M1-M11 complete
- See [websocket_plugin_history.md](websocket_plugin_history.md)

## Zorro Broker Plugin API Reference
- See [zorro_api_research.md](zorro_api_research.md) for full details

## Zorro Training Mode (KRITIKUS)
- `set(RULES)` → advise() PERCEPTRON → _ml.c
- `set(PARAMETERS)` → optimize() → .par
- optimize() MINDIG top-level, SOHA nem if/else-ben!
- Zorro slider limit: max 4 db (0-3)
- optimize() MUST be BEFORE continue
- Ternary (`? :`) NEM LÉTEZIK lite-C-ben!

## Zorro Hedge & Trade Loop
- `Hedge = 2`, `for(open_trades)`, `for(current_trades)`, `exitTrade(ThisTrade)`

---

## ML-DRIVEN Strategy (AKTÍV, LEGJOBB)
- **Fájlok**: `Desktop/z3/Strategy/MLDRIVEN.c` (diák) + `MLDATACOLLECTION.c` (tanár v6)
- **Szerver**: `TENSORFLOWMODEL.py` (XGBoost) vagy `ml_server_experiment.py` (multi-model)
- **Indítás**: `start_ml_xgb.bat` vagy `start_ml_lgbm.bat`
- See [mldriven_strategy.md](mldriven_strategy.md) for full details

### ML Model Összehasonlítás (2026-03-07, 6 év backtest, slider 1=Both)
| Model | PF | Trades | CAGR | SR | Win$ | DD |
|---|---|---|---|---|---|---|
| **XGBoost** | **1.59** | 1213 | 13.54% | 0.81 | 11888$ | **2076$** |
| **LightGBM** | 1.45 | 1240 | **21.44%** | **0.87** | **23151$** | 6645$ |
| Ensemble | 1.38 | 1197 | 18.52% | 0.77 | 18529$ | 5778$ |
| GBR | 1.22 | 1009 | 9.81% | 0.51 | 7809$ | 4282$ |
| Hybrid(lgbm+xgb) | 1.33 | 1132 | 13.73% | 0.66 | 12112$ | 4634$ |
- **XGBoost** = stabil, legjobb PF, legkisebb DD → FTMO-ra jobb
- **LightGBM** = legtöbb profit, legjobb CAGR → agresszívebb

### ML Validation Scores (param prediction)
| Model | MAE | R2 |
|---|---|---|
| **LightGBM** | **1.369** | **0.689** |
| XGBoost | 1.544 | 0.632 |
| GBR | 1.574 | 0.620 |
| RF | 1.580 | 0.593 |
| MLP | 2.395 | 0.149 |
| Ridge | 2.610 | 0.264 |

### Filter Accuracy (GO/SKIP szűrő)
- SMA: XGBoost **78%** > GBR 76.7% > LightGBM 76.1%
- CH: GBR **61.4%** > RF 58.4% > LightGBM 55.5% > XGBoost 55.2%

### Slider 4=Auto (algo_vote) — KÍSÉRLETI, NEM AJÁNLOTT
- Auto+FTMO Off: PF 1.24, 1020 trade — rosszabb mint Both
- Auto+FTMO: csak 64 trade — kettős szűrés túl agresszív
- **Tanulság**: filter modellek ~55% acc (CH) nem elég jó algo kapcsolgatásra

### Tanár (MLDATACOLLECTION) Verziók
- **v6 (JELENLEGI, JÓ)**: 12 param, algo("SMA")+algo("CH"), FCFS
- **v4 (.bak)**: 8 param — PF 0.86 BUKÁS
- **Backup CSV**: `Strategy/c backup/` (3007+2657 sor) — KRITIKUS, NE TÖRÖLD!
- **Tanulság**: v6 újratréning PF 1.01 → NE tréningezd újra! Backup CSV = biztos alap

---

## BUKOTT Fejlesztések (NE ISMÉTELD!)

### AdaptiveML v8+ICT — PF 0.78 FAILED
- v8 + 6 ICT feature (FVG, OTE, Displacement, Sweep, BOS) → 16 PERCEPTRON input
- ICT features r<0.05 korreláció → csak zaj, rontja az eredményt
- **Training probléma is**: 16 feature PERCEPTRON tréning instabil, WFO ciklusok nem konvergálnak
- v8 base (10 feature) tréning működött, ICT bővítés elrontotta

### AdaptiveML v8 base — Training instabilitás
- PERCEPTRON ~50% accuracy = random, de szűrőként mégis segít (pass-through)
- XGBoost OOS accuracy szintén ~50% irányra, DE FILTER-ként PF javít
- **Phantom BUG**: 500+ bar phantom trade-ek → szemét feedback → XGBoost rossz adatból tanult
- **Tanulság**: training adat minősége KRITIKUS — phantom/stale trade-ek torzítják

### AdaptiveML v9.12 — PF 0.81 FAILED
- Loss Filter + 4-model Direction ensemble → overfitting

### AdaptiveML v9.13 — ValAcc 50% FAILED
- ML=irány predikció, channel=timing → coinflip (feature r<0.08)
- **TANULSÁG**: ML NEM tudja az irányt prediktálni ezekkel a feature-ökkel!
- Channel=irány, ML=szűrő az EGYETLEN működő architektúra

### AdaptiveML Evolution
- v8: Channel+PERCEPTRON+XGBoost → PF 0.93 (WFO nélkül)
- v91: PERCEPTRON törölve → rosszabb
- v912: ML irány + ensemble → PF 0.81 (overfit)
- v913: ML=irány → random (50%)

### MLDATACOLLECTION v6 újratréning — PF 1.01 FAILED
- 12 optimize × 3 asset = 36 param → WFO overfitting
- Újratréning ROSSZABB mint az eredeti → backup CSV-ket használd!

### Slider 4=Auto algo_vote — NEM JAVÍT
- Filter model pontossága (~55% CH) nem elég algo kapcsolgatásra
- Kettős szűrés (vote+filter) túl agresszív → kevés trade

### Hybrid (LightGBM param + XGBoost filter) — NEM JAVÍT
- PF 1.33 — rosszabb mint bármelyik önállóan
- Modellek nem erősítik, gyengítik egymást

### Általános ML tanulságok
- ML=irány predikció SOHA nem működött (r<0.08)
- ML=szűrő (GO/SKIP) MŰKÖDIK (PF 1.02→1.60)
- ML=param predikció MŰKÖDIK (WFO params adaptálása)
- ICT/price action features nem javítanak (r<0.05)
- Több model ≠ jobb eredmény (ensemble/hybrid gyengébb)
- WFO újratréning instabil — backup CSV-k a biztos alap

---

## Egyéb Stratégiák
- **ChannelClaudeML**: LinReg+PERCEPTRON, 5 assets, PF 1.18, SR 1.19
- **HedgingClaude v11**: PF 2.03, SR 1.01, CAGR 24.91%
- **MartingaleHedge v3**: PF 3.89, Win 57.1%, CAGR 7.92%
  - See [hedgingclaude_optimization.md](hedgingclaude_optimization.md)

## Server
- **DAVID**: Windows Server 2022, Rackhost VPS, 16GB RAM, 2 core

## FTMO Challenge Rules
- See [ftmo_rules.md](ftmo_rules.md)

## API Reference
- See [ctrader_api_reference.md](ctrader_api_reference.md)
- See [zorro_api_research.md](zorro_api_research.md)
