# Lessons Learned — Tanár-Diák ML Trading

## SOHA NE CSINÁLD (bizonyítottan bukott)
1. **ML irány-predikció** regime/volatility feature-ökkel (r < 0.08) — v9.12, v9.13
2. **7 asset WFO** (84 optimize param) — "no parameters at walk N" hiba — v7
3. **ICT feature-ök** (FVG, OTE, sweep, BOS) — r < 0.05 korreláció — v8+ICT
4. **Multi-model ensemble filter** — zajt erősít, nem csökkent — v9.12
5. **Tanár újratréning** backup nélkül — PF 1.59 → 1.01 lett
6. **TREND-CLOSE tanárban** — PF 1.09 → 1.04 (diákban OK mert ML filter előszűr)
7. **Auto algo_vote (slider 4)** — filter modell nem elég pontos algo választásra

## MINDIG MŰKÖDIK
1. **ML = szűrő (GO/SKIP)**, nem irány-prediktor — PF 1.02 → 1.60
2. **Channel = irány**, ML = minőség-szűrő
3. **3 asset maximum** WFO-hoz (36 param, konvergál)
4. **Backup CSV mentés** minden tréning előtt!
5. **algo() szeparáció + FCFS** — SMA és CH nem zavarják egymást
6. **Filter threshold**: SMA=0.45, CH=0.40 (empirikus)
7. **ATR-alapú küszöbök** PIP helyett (multi-asset kompatibilis)

## WFO LIMITEK
- Max ~40 optimize param összesen (3 asset × 12 + overhead)
- 10 WFO ciklus standard
- DataSplit 80/20
- LookBack 900 (kell Hurst(100), MMI(200) -hoz)

## KÓDHIBÁK (audit 2026-03-07)
1. **Current_State eltérés**: tanár `(Balance-InitialBalance)/Capital`, diák `(Equity-Capital)/Capital` → kis distribution shift
2. **CH LifeTime**: tanár optimize(), diák hardcode 0 → /predict feleslegesen prediktálja
3. **algo_vote túl megengedő**: mindkét irány OR → tévesen engedélyez

## WORKFLOW SZABÁLYOK
1. Tervezz ELŐTT (3+ lépésnél plan mode)
2. Ha félremegy → ÁLLJ MEG, tervezd újra
3. Soha ne jelölj késznek bizonyítás nélkül
4. Minden user-javítás → lessons.md frissítés
5. Hack helyett elegáns megoldás
6. Bug report → azonnal javítsd, ne kérdezgess
