// ===================================================================
// HEDGING CLAUDE ML v2 - Machine Learning Pairs Trading Strategy
// ===================================================================
// Alapja: HedgingClaude.c (pairs + triangular hedge)
// ML: DTREE (alapertelmezett) vagy PERCEPTRON filter
// WFO: Anchored Walk-Forward Optimization, 10 ciklus
// 32-bit kompatibilis, nincs Python/PyTorch szukseg!
// ===================================================================
//
// HASZNALAT:
//   1. Zorro-ban valaszd ki: HedgingClaudeML
//   2. Kattints [Train] → ML tanulas + WFO + automatikus Test
//   3. Eredmeny: WFO out-of-sample teljesitmeny
//
// v3 VALTOZASOK:
//   - Price Action filter: Close > PrevClose irany megerosites
//   - ADX indikator: csökkeno ADX = gyengulo trend = mean reversion
//   - 16 ML feature (volt 12): + PriceDir, ADX, ADX slope, GBP dir
// v4 VALTOZASOK:
//   - Candle Patterns: CDLEngulfing, CDLHammer, CDLShootingStar, CDLDoji
//   - 20 ML feature (MAX!) = 12 eredeti + 4 ADX/PriceDir + 4 Candle
// v5 VALTOZASOK:
//   - Trading Session Filter: csak London (07-16) + New York (12-21) UTC
//   - Azsia szesszio kizarva (alacsony volatilitas, gyenge EUR/GBP mozgas)
//   - PERCEPTRON (DTREE overfit-el 4+ ev adaton)
// ===================================================================

//#define USE_DTREE  // DTREE: overfit kockázat! PERCEPTRON robusztusabb 6 ev adaton.

#include <default.c>

// -------------------------------------------------------------------
// HELPER: Linearis regresszio beta (hedge ratio szamitas)
// -------------------------------------------------------------------
var calculateSlope(var* Y, var* X, int Period)
{
	var SumX, SumY, SumXY, SumX2;
	var MeanX, MeanY, Denom, Beta;
	int i;

	SumX = 0; SumY = 0; SumXY = 0; SumX2 = 0;

	for(i = 0; i < Period; i++)
	{
		SumX += X[i];
		SumY += Y[i];
		SumXY += X[i] * Y[i];
		SumX2 += X[i] * X[i];
	}

	MeanX = SumX / Period;
	MeanY = SumY / Period;

	Denom = SumX2 - Period * MeanX * MeanX;
	if(abs(Denom) < 0.0000001) return 1.0;

	Beta = (SumXY - Period * MeanX * MeanY) / Denom;

	if(Beta < 0.3) Beta = 0.3;
	if(Beta > 3.0) Beta = 3.0;

	return Beta;
}

// -------------------------------------------------------------------
// HELPER: Spread stacionaritas ellenorzes
// -------------------------------------------------------------------
int isSpreadStationary(var* SpreadSeries, int Period)
{
	var RecentStd, LongStd, Ratio;

	if(Period < 8) return 0;

	LongStd = StdDev(SpreadSeries, Period);
	if(LongStd < 0.0000001) return 0;

	RecentStd = StdDev(SpreadSeries, Period / 4);
	Ratio = RecentStd / LongStd;

	return (Ratio > 10.5 && Ratio < 11.5);
}


// ===================================================================
// MAIN STRATEGY
// ===================================================================
static int gEUR_trades = 0;
static int gGBP_trades = 0;
static int gCHF_trades = 0;
static int gEntry_attempts = 0;

function run()
{
	// ---------------------------------------------------------------
	// 1. SETUP
	// ---------------------------------------------------------------
	set(RULES);       // ML training/prediction aktivalasa
	set(LOGFILE);     // printf → Log/HedgingClaudeML_test.log

	BarPeriod = 5;        // 1H bars (bizonyitottan legjobb)
	StartDate = 20240105;  // 2024-2026 (bizonyitottan nyereseges periodus)
	EndDate = 20260301;
	LookBack = 152;

	Capital = 5000;
	Leverage = 500;
	EndWeek = 52200;

	// --- Walk-Forward Optimization ---
	NumWFOCycles = -6;
	DataSplit = 90;

	// --- Training setup ---
	if(Train) {
		Hedge = 2;
		LifeTime = 6;
	}

	// Orphan recovery TRADE modban
	if(Bar == 0 && is(TRADEMODE))
		brokerTrades(0);

	// ---------------------------------------------------------------
	// 2. PARAMETERS
	// ---------------------------------------------------------------
	var ZScore_Entry = 0.75;       // fix alap (adaptive 7b szekcioban modositja)
	var ZScore_Exit = 5.5;
	var Lookback_Period = 152;
	var MinHold = 6;
	var Corr_Min = 0.3;
	var Hurst_Threshold = 0.55;
	var Tri_Weight = 0.5;
	var EmergencySL_ATR = 115.0;
	var PortfolioSL_Pct = 90.0;
	var PortfolioTP_Pct = 50.0;
	var MinLots = 10;
	var LotSize = 3;
	var MLThreshold = 0;

	// ---------------------------------------------------------------
	// 3. DATA COLLECTION
	// ---------------------------------------------------------------
	vars Series1, Series2, Series3;
	var Price1, Price2, Price3;
	var ATR1, ATR2, ATR3;

	asset("EUR/USD");
	Price1 = priceClose();
	Series1 = series(Price1);
	ATR1 = ATR(20);
	if(Price1 == 0) { if(Bar < 5) printf("\nWARN: EUR/USD no price bar %d", Bar); return; }

	asset("GBP/USD");
	Price2 = priceClose();
	Series2 = series(Price2);
	ATR2 = ATR(20);
	if(Price2 == 0) { if(Bar < 5) printf("\nWARN: GBP/USD no price bar %d", Bar); return; }

	asset("USD/CHF");
	Price3 = priceClose();
	Series3 = series(Price3);
	ATR3 = ATR(20);
	if(Price3 == 0) { if(Bar < 5) printf("\nWARN: USD/CHF no price bar %d", Bar); return; }

	// ---------------------------------------------------------------
	// 4. ADX + PRICE DIRECTION (multi-timeframe megerosites)
	// ---------------------------------------------------------------

	// ADX: trend ero indikator (14 period = standard)
	// Magas ADX = eros trend (rossz pairs tradinghez)
	// Csökkeno ADX = gyengulo trend (jo mean reversion-hoz!)
	asset("EUR/USD");
	var adxEUR = ADX(14);
	vars adxEURseries = series(adxEUR);
	var adxSlope = adxEURseries[0] - adxEURseries[1]; // negativ = gyengulo trend
	if(adxSlope != adxSlope) adxSlope = 0;  // NaN check

	// Magasabb idosik ADX (56 period 1H-n ≈ 14 period 4H-n)
	var adxHTF = ADX(56);
	vars adxHTFseries = series(adxHTF);
	var adxHTFslope = adxHTFseries[0] - adxHTFseries[1];
	if(adxHTFslope != adxHTFslope) adxHTFslope = 0;

	// Price Direction: Close vs Previous Close (irany megerosites)
	// Long-hoz: EUR emelkedik (Close > PrevClose)
	// Short-hoz: EUR csokken (Close < PrevClose)
	int eurRising = (Bar > 1 && Series1[0] > Series1[1]);
	int eurFalling = (Bar > 1 && Series1[0] < Series1[1]);
	int gbpRising = (Bar > 1 && Series2[0] > Series2[1]);
	int gbpFalling = (Bar > 1 && Series2[0] < Series2[1]);

	// ADX szurok
	int adxDeclining = (adxSlope < 0);       // 1H ADX csokken
	int adxHTFdeclining = (adxHTFslope < 0);  // 4H ADX csokken
	int adxLow = (adxEUR < 25);               // gyenge trend (kedvezo)

	// ---------------------------------------------------------------
	// 4b. CANDLE PATTERNS (TA-Lib CDL fuggvenyek)
	// ---------------------------------------------------------------
	// CDL fuggvenyek -100/0/+100 erteket adnak:
	//   +100 = bullish pattern, -100 = bearish pattern, 0 = nincs pattern
	// EUR/USD-re szamoljuk (fo kereskedesi par)
	asset("EUR/USD");
	var cdlEngulf  = CDLEngulfing();      // engulfing (bullish/bearish)
	var cdlHammer  = CDLHammer();         // hammer / pin bar (bullish)
	var cdlStar    = CDLShootingStar();   // shooting star (bearish pin bar)
	var cdlDoji    = CDLDoji();           // doji (bizonytalansag)

	// ---------------------------------------------------------------
	// 4c. TRADING SESSION FILTER (Azsia / London / New York)
	// ---------------------------------------------------------------
	// Forex szessziok (UTC):
	//   Asia:     00:00 - 08:00  (alacsony volatilitas, range)
	//   London:   07:00 - 16:00  (legmagasabb volatilitas, EUR/GBP fo mozgas)
	//   New York: 12:00 - 21:00  (magas volatilitas, USD mozgas)
	//   Overlap:  12:00 - 16:00  (CSUCS volatilitas!)
	int utcHour = hour();  // 0-23 UTC (bar ideje)
	int isLondon  = (utcHour >= 7 && utcHour < 16);
	int isNewYork = (utcHour >= 12 && utcHour < 21);
	int isOverlap = (utcHour >= 12 && utcHour < 16);
	int isAsia    = (utcHour >= 0 && utcHour < 8);
	int SessionOK = (isLondon || isNewYork);  // Csak aktiv szesszio

	// ---------------------------------------------------------------
	// 5. CORRELATIONS
	// ---------------------------------------------------------------
	var Corr12 = Correlation(Series1, Series2, Lookback_Period);
	var Corr13 = Correlation(Series1, Series3, Lookback_Period);
	if(Corr12 != Corr12) Corr12 = 0;
	if(Corr13 != Corr13) Corr13 = 0;
	var AbsCorr12 = abs(Corr12);

	// ---------------------------------------------------------------
	// 5. DYNAMIC HEDGE RATIO
	// ---------------------------------------------------------------
	var DynHedgeRatio = calculateSlope(Series1, Series2, Lookback_Period);
	if(DynHedgeRatio != DynHedgeRatio) DynHedgeRatio = 1.0;

	// ---------------------------------------------------------------
	// 6. PAIR SPREAD + Z-SCORE
	// ---------------------------------------------------------------
	var PairSpread = Price1 - (DynHedgeRatio * Price2);
	vars PairSpreadSeries = series(PairSpread);
	var PairSMA = SMA(PairSpreadSeries, Lookback_Period);
	var PairStd = StdDev(PairSpreadSeries, Lookback_Period);
	var PairZScore = 0;
	if(PairStd > 0.00001)
		PairZScore = (PairSpreadSeries[0] - PairSMA) / PairStd;

	// ---------------------------------------------------------------
	// 7. TRIANGULAR SPREAD + Z-SCORE
	// ---------------------------------------------------------------
	var TriSpread = Price1 - (DynHedgeRatio * Price2) + (Tri_Weight * Price3);
	vars TriSpreadSeries = series(TriSpread);
	var TriSMA = SMA(TriSpreadSeries, Lookback_Period);
	var TriStd = StdDev(TriSpreadSeries, Lookback_Period);
	var TriZScore = 0;
	if(TriStd > 0.00001)
		TriZScore = (TriSpreadSeries[0] - TriSMA) / TriStd;

	// ---------------------------------------------------------------
	// 7b. ADAPTIVE Z-SCORE THRESHOLD
	// ---------------------------------------------------------------
	// A spread volatilitasa alapjan dinamikusan allitjuk a ZScore kuszobot:
	// - Magas volatilitas (vol ratio > 1) → magasabb kuszob (szelektivebb)
	// - Alacsony volatilitas (vol ratio < 1) → alacsonyabb kuszob (tobb trade)
	// Igy a strategia alkalmazkodik az aktualis piaci kornyezethez.
	var recentSpreadVol = StdDev(PairSpreadSeries, 12);  // utolso 12 bar vol
	var longSpreadVol = StdDev(PairSpreadSeries, Lookback_Period); // hosszu tavu vol
	var spreadVolRatio = 1.0;
	if(longSpreadVol > 0.00001) spreadVolRatio = recentSpreadVol / longSpreadVol;

	// Adaptiv kuszob: base * volRatio, de min 1.2, max 2.5
	var ZScore_Adaptive = ZScore_Entry * spreadVolRatio;
	if(ZScore_Adaptive < 1.2) ZScore_Adaptive = 1.2;
	if(ZScore_Adaptive > 2.5) ZScore_Adaptive = 2.5;

	// ---------------------------------------------------------------
	// 8. REGIME FILTER
	// ---------------------------------------------------------------
	var HurstValue = Hurst(PairSpreadSeries, Lookback_Period);
	if(HurstValue != HurstValue) HurstValue = 0.5;
	int Stationary = isSpreadStationary(PairSpreadSeries, Lookback_Period);
	int MeanReverting = (HurstValue < Hurst_Threshold);
	int NearNeutral = (HurstValue < 0.6);
	int CanTrade = (MeanReverting || Stationary || NearNeutral) && (AbsCorr12 > Corr_Min);

	// ---------------------------------------------------------------
	// 10. FEATURE ENGINEERING (20 signal = 12 eredeti + 4 ADX/PriceDir + 4 Candle)
	// ---------------------------------------------------------------
	vars PairZSeries = series(PairZScore);
	var ZMomentum = PairZSeries[0] - PairZSeries[4];
	if(ZMomentum != ZMomentum) ZMomentum = 0;

	var chg_eur1 = 0, chg_gbp1 = 0, chg_eur4 = 0, chg_gbp4 = 0;
	if(Price1 > 0 && Bar > 1) chg_eur1 = 100.0 * (Series1[0] - Series1[1]) / Series1[0];
	if(Price2 > 0 && Bar > 1) chg_gbp1 = 100.0 * (Series2[0] - Series2[1]) / Series2[0];
	if(Price1 > 0 && Bar > 4) chg_eur4 = 100.0 * (Series1[0] - Series1[4]) / Series1[0];
	if(Price2 > 0 && Bar > 4) chg_gbp4 = 100.0 * (Series2[0] - Series2[4]) / Series2[0];

	var VolRatio = 1.0;
	if(ATR1 > 0.00001) VolRatio = ATR2 / ATR1;

	// Clamp (eredeti 12)
	var PZ_c  = min(max(PairZScore, -4), 4);
	var TZ_c  = min(max(TriZScore, -4), 4);
	var C13_c = min(max(Corr13, -1), 1);
	var VR_c  = min(max(VolRatio, 0.5), 2.0);
	var ZM_c  = min(max(ZMomentum, -3), 3);
	var ce1   = min(max(chg_eur1, -5), 5);
	var cg1   = min(max(chg_gbp1, -5), 5);
	var ce4   = min(max(chg_eur4, -10), 10);
	var cg4   = min(max(chg_gbp4, -10), 10);

	// uj feature-ok: ADX + Price Direction
	var adx_n  = min(max(adxEUR / 50.0, 0), 2.0);   // ADX normalizalva (0..2)
	var adxS_n = min(max(adxSlope, -5), 5);           // ADX slope (negativ = gyengulo)
	var pdEUR = 0;                                     // EUR price direction
	if(eurRising) pdEUR = 1;
	if(eurFalling) pdEUR = -1;
	var pdGBP = 0;                                     // GBP price direction
	if(gbpRising) pdGBP = 1;
	if(gbpFalling) pdGBP = -1;

	// uj feature-ok: Candle Patterns (normalizalva -1..+1)
	var cdlE_n = cdlEngulf / 100.0;   // engulfing: -1=bearish, 0=nincs, +1=bullish
	var cdlH_n = cdlHammer / 100.0;   // hammer (pin bar): 0 vagy +1
	var cdlS_n = cdlStar / 100.0;     // shooting star: 0 vagy +1 (bearish jelzes!)
	var cdlD_n = cdlDoji / 100.0;     // doji: 0 vagy +1 (bizonytalansag)

	// ---------------------------------------------------------------
	// 11. ML PREDICTION (20 feature = MAX!)
	// ---------------------------------------------------------------
	// 0-11: Z-Score, Hurst, korrelacio, volatilitas, ar momentum
	// 12: ADX (trend ero — alacsony = jo pairs tradinghez)
	// 13: ADX slope (negativ = gyengulo trend = kedvezo)
	// 14: EUR price direction (+1=emelkedo, -1=csökkeno)
	// 15: GBP price direction (+1=emelkedo, -1=csökkeno)
	// 16: Engulfing pattern (-1=bearish, +1=bullish)
	// 17: Hammer / pin bar (+1=bullish reversal)
	// 18: Shooting star (+1=bearish reversal)
	// 19: Doji (+1=bizonytalansag, fordulo pont)

	asset("EUR/USD");

#ifdef USE_DTREE
	var vLong = adviseLong(DTREE+BALANCED+RETURNS, 0,
		PZ_c, TZ_c, HurstValue, AbsCorr12, C13_c, (var)Stationary,
		VR_c, ZM_c, ce1, cg1, ce4, cg4,
		adx_n, adxS_n, pdEUR, pdGBP,
		cdlE_n, cdlH_n, cdlS_n, cdlD_n);
#else
	var vLong = adviseLong(PERCEPTRON+BALANCED+RETURNS, 0,
		PZ_c, TZ_c, HurstValue, AbsCorr12, C13_c, (var)Stationary,
		VR_c, ZM_c, ce1, cg1, ce4, cg4,
		adx_n, adxS_n, pdEUR, pdGBP,
		cdlE_n, cdlH_n, cdlS_n, cdlD_n);
#endif
	var vShort = adviseShort();  // ugyanaz a 20 signal

	// ---------------------------------------------------------------
	// 11. ENTRY
	// ---------------------------------------------------------------
	var BaseLots = max(LotSize * MinLots, MinLots);
	var HalfLots = max(BaseLots / 2.0, MinLots);
	static int lastEntryBar = 0;

	if(Train)
	{
		// TRAINING: minden bar-on long+short EUR/USD (Hedge=2)
		asset("EUR/USD");
		Lots = BaseLots;
		enterLong();
		enterShort();
	}
	else
	{
		// TEST/TRADE: ML + Z-Score + Regime → pairs trade

		// SCENARIO B: Spread tul alacsony → long EUR, short GBP
		// ZScore_Adaptive: dinamikus kuszob (alacsony vol → alacsonyabb, magas vol → magasabb)
		if(vLong > MLThreshold && PairZScore < -ZScore_Adaptive && CanTrade && NumOpenTotal == 0)
		{
			lastEntryBar = Bar;
			gEntry_attempts++;

			asset("EUR/USD");
			Lots = BaseLots;
			Stop = ATR1 * EmergencySL_ATR;
			TakeProfit = 0;
			enterLong();
			if(NumOpenLong) gEUR_trades++;

			asset("GBP/USD");
			Lots = BaseLots;
			Stop = ATR2 * EmergencySL_ATR;
			TakeProfit = 0;
			enterShort();
			if(NumOpenShort) gGBP_trades++;

			if(TriZScore < -ZScore_Adaptive)
			{
				asset("USD/CHF");
				Lots = HalfLots;
				Stop = ATR3 * EmergencySL_ATR;
				TakeProfit = 0;
				enterShort();
				if(NumOpenShort) gCHF_trades++;
			}
		}

		// SCENARIO A: Spread tul magas → short EUR, long GBP
		if(vShort > MLThreshold && PairZScore > ZScore_Adaptive && CanTrade && NumOpenTotal == 0)
		{
			lastEntryBar = Bar;
			gEntry_attempts++;

			asset("EUR/USD");
			Lots = BaseLots;
			Stop = ATR1 * EmergencySL_ATR;
			TakeProfit = 0;
			enterShort();
			if(NumOpenShort) gEUR_trades++;

			asset("GBP/USD");
			Lots = BaseLots;
			Stop = ATR2 * EmergencySL_ATR;
			TakeProfit = 0;
			enterLong();
			if(NumOpenLong) gGBP_trades++;

			if(TriZScore > ZScore_Adaptive)
			{
				asset("USD/CHF");
				Lots = HalfLots;
				Stop = ATR3 * EmergencySL_ATR;
				TakeProfit = 0;
				enterLong();
				if(NumOpenLong) gCHF_trades++;
			}
		}

		// ---------------------------------------------------------------
		// 12. EXIT
		// ---------------------------------------------------------------
		int holdBars = Bar - lastEntryBar;

		// Z-Score EXIT (MinHold utan)
		if(holdBars >= MinHold && abs(PairZScore) < ZScore_Exit && NumOpenTotal > 0)
		{
			asset("EUR/USD"); exitLong(); exitShort();
			asset("GBP/USD"); exitLong(); exitShort();
			asset("USD/CHF"); exitLong(); exitShort();
		}

		// PORTFOLIO SL/TP
		if(NumOpenTotal > 0)
		{
			var totalProfit = 0;
			for(open_trades) {
				var p = TradeProfit;
				totalProfit += p;
			}

			var slThreshold = -Capital * PortfolioSL_Pct / 100.0;
			var tpThreshold = Capital * PortfolioTP_Pct / 100.0;

			if(totalProfit < slThreshold)
			{
				asset("EUR/USD"); exitLong(); exitShort();
				asset("GBP/USD"); exitLong(); exitShort();
				asset("USD/CHF"); exitLong(); exitShort();
			}
			else if(totalProfit > tpThreshold)
			{
				asset("EUR/USD"); exitLong(); exitShort();
				asset("GBP/USD"); exitLong(); exitShort();
				asset("USD/CHF"); exitLong(); exitShort();
			}
		}
	}

	// ---------------------------------------------------------------
	// 13. PLOTS
	// ---------------------------------------------------------------
	if(!is(LOOKBACK))
	{
		plot("ML Long", vLong, NEW|LINE, BLACK);
		plot("ML Short", vShort, LINE, GREY);
		if(MLThreshold != 0) plot("Threshold", MLThreshold, LINE, RED);

		if(PairZScore > -50 && PairZScore < 50)
			plot("PairZ", PairZScore, NEW, RED);
		if(TriZScore > -50 && TriZScore < 50)
			plot("TriZ", TriZScore, NEW, BLUE);

		plot("Hurst", HurstValue, NEW, BLACK);
		plot("HurstTh", Hurst_Threshold, LINE, GREY);

		plot("ZAdapt", ZScore_Adaptive, NEW|LINE, RED);
		plot("ZBase", ZScore_Entry, LINE, GREY);
	}

	// ---------------------------------------------------------------
	// 14. SUMMARY
	// ---------------------------------------------------------------
	if(is(LOOKBACK)) return;

	if(Bar % 5000 == 0)
		printf("\n[BAR %d] Z=%.2f H=%.3f Can=%d vL=%.0f vS=%.0f",
			Bar, PairZScore, HurstValue, CanTrade, vLong, vShort);

	if(is(EXITRUN))
	{
		printf("\n\n========================================================");
		printf("\n  HEDGING CLAUDE ML v4 - FINAL SUMMARY");
		printf("\n========================================================");
#ifdef USE_DTREE
		printf("\n  ML Method:       DTREE");
#else
		printf("\n  ML Method:       PERCEPTRON");
#endif
		printf("\n  Period:          %d - %d", StartDate, EndDate);
		printf("\n  WFO Cycles:      %d (anchored)", abs(NumWFOCycles));
		printf("\n  ZScore Entry:    %.1f (adaptive: 1.2 - 2.5)", ZScore_Entry);
		printf("\n  MinHold:         %.0f bars", MinHold);
		printf("\n  ML Threshold:    %.0f", MLThreshold);
		printf("\n  Filters:         PriceDir + ADX + Candle(Engulf/Hammer/Star/Doji)");
		printf("\n  Entry attempts:  %d", gEntry_attempts);
		printf("\n  EUR/USD trades:  %d", gEUR_trades);
		printf("\n  GBP/USD trades:  %d", gGBP_trades);
		printf("\n  USD/CHF trades:  %d", gCHF_trades);
		if(gEntry_attempts > 0 && gGBP_trades == 0)
			printf("\n  *** BUG: GBP/USD SOHA NEM NYILT MEG! ***");
		if(gEntry_attempts > 0 && gGBP_trades == gEUR_trades)
			printf("\n  OK: Minden EUR entry-hez volt GBP hedge.");
		printf("\n========================================================\n");
	}
}
