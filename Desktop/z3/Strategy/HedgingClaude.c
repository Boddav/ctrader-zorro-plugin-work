// ===================================================================
// HEDGING CLAUDE - Multi-Layer Hedge Strategy
// 3-retegu rendszer: Pairs + Triangular + Regime Filter
// ===================================================================
//
// RETEG 1: Pairs Mean Reversion (EUR/USD vs GBP/USD)
//   - Spread Z-Score belepes/kilepes
//   - Dinamikus hedge ratio (linearis regresszio)
//   - Stacionaritas ellenorzes
//
// RETEG 2: Triangular Hedge (+ USD/CHF)
//   - Harmadik lab az USD kitettseget csokkenti
//   - Kulon Z-Score megerosites
//
// RETEG 3: Regime Filter (Hurst + Stacionaritas)
//   - Hurst < 0.5 = mean reverting → kereskedunk
//   - Hurst >= 0.5 = trending → nem kereskedunk
// ===================================================================

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
	if(abs(Denom) < 0.0000001) return 1.0;  // fallback

	Beta = (SumXY - Period * MeanX * MeanY) / Denom;

	// Clamp to realistic range
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

	// Stacioner ha a recent/long volatilitas arany 0.5-1.5 kozott
	return (Ratio > 0.5 && Ratio < 1.5);
}


// ===================================================================
// MAIN STRATEGY
// ===================================================================
// Global trade counters (a teszt vegen osszesites)
static int gEUR_trades = 0;
static int gGBP_trades = 0;
static int gCHF_trades = 0;
static int gEntry_attempts = 0;

function run()
{
	// ---------------------------------------------------------------
	// 1. SETUP
	// ---------------------------------------------------------------
	// set(PARAMETERS);  // KIKAPCSOLVA: per-asset .par file hiba okozza a multi-asset bugot!

	set(LOGFILE);  // printf() output -> Log/HedgingClaude_log.csv

	BarPeriod = 60;       // 1H bars (15 perces tul zajos pairs tradinghez)
	StartDate = 20240105;
	EndDate = 20260301;

	Capital = 5000;
	Leverage = 2;

	EndWeek = 52200;

	// Orphan recovery on restart
	if(Bar == 0 && is(TRADEMODE))
		brokerTrades(0);

	// ---------------------------------------------------------------
	// 2. PARAMETERS
	// ---------------------------------------------------------------

	var ZScore_Entry = optimize(2.0, 1.0, 3.0, 0.25, -3);   // magasabb = kevesebb, biztosabb jel
	var ZScore_Exit = optimize(0.5, 0.0, 1.0, 0.1, -3);    // 0.5 = kozelebb a meanhez varunk
	var Lookback_Period = optimize(48, 20, 96, 4, -3);      // 48H = 2 nap (1H bar-okkal)
	var MinHold = optimize(8, 4, 24, 4, -3);                // min 8 bar (8 ora) tartasi ido

	// LookBack automatikusan: max(Period*3, 80) — ATR(20) minimum ~61 bar kell
	LookBack = max(Lookback_Period * 3, 80);
	var Corr_Min = optimize(0.4, 0.2, 0.8, 0.05, -3);
	var Hurst_Threshold = optimize(0.55, 0.35, 0.65, 0.05, -3);
	var Tri_Weight = optimize(0.5, 0.1, 1.0, 0.1, -3);

	// Portfolio szintu SL/TP (Capital %-ban)
	var PortfolioSL_Pct = optimize(5.0, 2.0, 10.0, 1.0, -3);   // -5% = portfolio stop (tobb ido a hedge-nek)
	var PortfolioTP_Pct = optimize(3.0, 1.0, 8.0, 0.5, -3);    // +3% = portfolio take profit
	var EmergencySL_ATR = 15.0;  // egyedi veszhelyzeti SL (15x ATR - NAGYON messze, csak katasztrofa)

	// cTrader minVolume = 1000 cents = 0.01 lot
	// LotAmount=1000(!) -> Lots=1 -> Amount=1000 -> Volume=1000 cents = minVolume OK
	// FIGYELEM: LotAmount=1000, NEM 100! (Multiplier=1000 az AssetsFix.csv-ben)
	var MinLots = 1;      // minimum 1 lot (= 0.01 cTrader lot = minVolume)
	var LotSize = optimize(3, 1, 8, 1, -3);  // pozicio meret szorzo (Lots = LotSize * MinLots)

	// ---------------------------------------------------------------
	// 3. DATA COLLECTION — 3 asset series
	// ---------------------------------------------------------------

	vars Series1;   // EUR/USD
	vars Series2;   // GBP/USD
	vars Series3;   // USD/CHF

	var Price1, Price2, Price3;
	var ATR1, ATR2, ATR3;

	// --- EUR/USD ---
	asset("EUR/USD");
	Price1 = priceClose();
	Series1 = series(Price1);
	ATR1 = ATR(20);
	if(Price1 == 0) { if(Bar < 5) printf("\nWARN: EUR/USD no price bar %d", Bar); return; }

	// --- GBP/USD ---
	asset("GBP/USD");
	Price2 = priceClose();
	Series2 = series(Price2);
	ATR2 = ATR(20);
	if(Price2 == 0) { if(Bar < 5) printf("\nWARN: GBP/USD no price bar %d", Bar); return; }

	// --- USD/CHF ---
	asset("USD/CHF");
	Price3 = priceClose();
	Series3 = series(Price3);
	ATR3 = ATR(20);
	if(Price3 == 0) { if(Bar < 5) printf("\nWARN: USD/CHF no price bar %d", Bar); return; }

	// Diagnostic: elso par bar-on ellenorzes
	if(Bar < 3)
		printf("\n[DATA] Bar=%d EUR=%.5f GBP=%.5f CHF=%.5f ATR1=%.6f ATR2=%.6f ATR3=%.6f",
			Bar, Price1, Price2, Price3, ATR1, ATR2, ATR3);

	// ---------------------------------------------------------------
	// 4. CORRELATIONS
	// ---------------------------------------------------------------

	var Corr12, Corr13, Corr23;

	Corr12 = Correlation(Series1, Series2, Lookback_Period);
	Corr13 = Correlation(Series1, Series3, Lookback_Period);
	Corr23 = Correlation(Series2, Series3, Lookback_Period);

	// NaN check
	if(Corr12 != Corr12) Corr12 = 0;
	if(Corr13 != Corr13) Corr13 = 0;
	if(Corr23 != Corr23) Corr23 = 0;

	var AbsCorr12 = abs(Corr12);

	// ---------------------------------------------------------------
	// 5. DYNAMIC HEDGE RATIO (linearis regresszio)
	// ---------------------------------------------------------------

	var DynHedgeRatio = calculateSlope(Series1, Series2, Lookback_Period);

	// NaN check
	if(DynHedgeRatio != DynHedgeRatio) DynHedgeRatio = 1.0;

	// ---------------------------------------------------------------
	// 6. PAIR SPREAD + Z-SCORE (EUR/USD vs GBP/USD)
	// ---------------------------------------------------------------

	var PairSpread = Price1 - (DynHedgeRatio * Price2);
	vars PairSpreadSeries = series(PairSpread);

	var PairSMA = SMA(PairSpreadSeries, Lookback_Period);
	var PairStd = StdDev(PairSpreadSeries, Lookback_Period);
	var PairZScore = 0;

	if(PairStd > 0.00001)
		PairZScore = (PairSpreadSeries[0] - PairSMA) / PairStd;

	// ---------------------------------------------------------------
	// 7. TRIANGULAR SPREAD + Z-SCORE (+ USD/CHF)
	// ---------------------------------------------------------------

	var TriSpread = Price1 - (DynHedgeRatio * Price2) + (Tri_Weight * Price3);
	vars TriSpreadSeries = series(TriSpread);

	var TriSMA = SMA(TriSpreadSeries, Lookback_Period);
	var TriStd = StdDev(TriSpreadSeries, Lookback_Period);
	var TriZScore = 0;

	if(TriStd > 0.00001)
		TriZScore = (TriSpreadSeries[0] - TriSMA) / TriStd;

	// ---------------------------------------------------------------
	// 8. REGIME FILTER (Hurst + Stacionaritas)
	// ---------------------------------------------------------------

	var HurstValue = Hurst(PairSpreadSeries, Lookback_Period);

	// NaN check
	if(HurstValue != HurstValue) HurstValue = 0.5;

	int Stationary = isSpreadStationary(PairSpreadSeries, Lookback_Period);

	// REGIME FILTER — mean reverting piacon kereskedunk, trending-en NEM
	int MeanReverting = (HurstValue < Hurst_Threshold);
	int NearNeutral = (HurstValue < 0.6);
	int CanTrade = (MeanReverting || Stationary || NearNeutral) && (AbsCorr12 > Corr_Min);

	// ---------------------------------------------------------------
	// 9. ENTRY — Pairs + Triangular
	// ---------------------------------------------------------------
	// NINCS egyedi TakeProfit! Pairs tradingben a portfolio exit dont.
	// Csak veszhelyzeti SL (10x ATR) a katasztrofa ellen.

	var BaseLots = max(LotSize * MinLots, MinLots);
	var HalfLots = max(BaseLots / 2.0, MinLots);

	static int nEntryDiag = 0;  // diagnostic counter
	static int lastEntryBar = 0;  // utolso belepes bar-ja (MinHold szamitashoz)

	if(CanTrade)
	{
		// === SCENARIO A: Pair Spread TOO HIGH ===
		// Short EUR/USD, Long GBP/USD (spread visszahuzodik)
		if(PairZScore > ZScore_Entry && NumOpenTotal == 0)
		{
			lastEntryBar = Bar;
			gEntry_attempts++;
			if(nEntryDiag < 10 && !is(LOOKBACK))
				printf("\n[ENTRY-A] Bar=%d Z=%.3f Entry=%.3f BaseLots=%.0f", Bar, PairZScore, ZScore_Entry, BaseLots);

			asset("EUR/USD");
			if(!NumOpenShort)
			{
				int bS = NumOpenShort;
				Lots = BaseLots;
				Stop = ATR1 * EmergencySL_ATR;
				TakeProfit = 0;
				enterShort();
				if(NumOpenShort > bS) gEUR_trades++;
				if(nEntryDiag < 10)
					printf("\n  EUR/USD Short: %d->%d LotAmt=%.0f Stop=%.5f", bS, NumOpenShort, LotAmount, Stop);
			}

			asset("GBP/USD");
			if(!NumOpenLong)
			{
				int bL = NumOpenLong;
				Lots = BaseLots;
				Stop = ATR2 * EmergencySL_ATR;
				TakeProfit = 0;
				enterLong();
				if(NumOpenLong > bL) gGBP_trades++;
				if(nEntryDiag < 10)
					printf("\n  GBP/USD Long: %d->%d LotAmt=%.0f Stop=%.5f", bL, NumOpenLong, LotAmount, Stop);
			}
			else if(nEntryDiag < 10 && !is(LOOKBACK))
				printf("\n  GBP/USD SKIP: NumOpenLong=%d", NumOpenLong);

			// Triangular lab: USD/CHF (ha TriZScore megerositi)
			if(TriZScore > ZScore_Entry)
			{
				asset("USD/CHF");
				if(!NumOpenShort)
				{
					Lots = HalfLots;
					Stop = ATR3 * EmergencySL_ATR;
					TakeProfit = 0;
					enterShort();
					if(NumOpenShort) gCHF_trades++;
					if(nEntryDiag < 10)
						printf("\n  USD/CHF Short: NumOpenS=%d LotAmt=%.0f", NumOpenShort, LotAmount);
				}
			}
			if(!is(LOOKBACK)) nEntryDiag++;
		}

		// === SCENARIO B: Pair Spread TOO LOW ===
		// Long EUR/USD, Short GBP/USD (spread visszahuzodik)
		if(PairZScore < -ZScore_Entry && NumOpenTotal == 0)
		{
			lastEntryBar = Bar;
			gEntry_attempts++;
			if(nEntryDiag < 10 && !is(LOOKBACK))
				printf("\n[ENTRY-B] Bar=%d Z=%.3f Entry=-%.3f", Bar, PairZScore, ZScore_Entry);

			asset("EUR/USD");
			if(!NumOpenLong)
			{
				int bL = NumOpenLong;
				Lots = BaseLots;
				Stop = ATR1 * EmergencySL_ATR;
				TakeProfit = 0;
				enterLong();
				if(NumOpenLong > bL) gEUR_trades++;
				if(nEntryDiag < 10)
					printf("\n  EUR/USD Long: %d->%d LotAmt=%.0f Stop=%.5f", bL, NumOpenLong, LotAmount, Stop);
			}

			asset("GBP/USD");
			if(!NumOpenShort)
			{
				int bS = NumOpenShort;
				Lots = BaseLots;
				Stop = ATR2 * EmergencySL_ATR;
				TakeProfit = 0;
				enterShort();
				if(NumOpenShort > bS) gGBP_trades++;
				if(nEntryDiag < 10)
					printf("\n  GBP/USD Short: %d->%d LotAmt=%.0f Stop=%.5f", bS, NumOpenShort, LotAmount, Stop);
			}
			else if(nEntryDiag < 10 && !is(LOOKBACK))
				printf("\n  GBP/USD SKIP: NumOpenShort=%d", NumOpenShort);

			// Triangular lab: USD/CHF (ha TriZScore megerositi)
			if(TriZScore < -ZScore_Entry)
			{
				asset("USD/CHF");
				if(!NumOpenLong)
				{
					Lots = HalfLots;
					Stop = ATR3 * EmergencySL_ATR;
					TakeProfit = 0;
					enterLong();
					if(NumOpenLong) gCHF_trades++;
					if(nEntryDiag < 10)
						printf("\n  USD/CHF Long: NumOpenL=%d LotAmt=%.0f", NumOpenLong, LotAmount);
				}
			}
			if(!is(LOOKBACK)) nEntryDiag++;
		}
	}

	// ---------------------------------------------------------------
	// 10. EXIT — 3 szintu dontes (MinHold ellenorzes!)
	// ---------------------------------------------------------------

	int holdBars = Bar - lastEntryBar;

	// --- 10A. Z-SCORE EXIT: spread visszatert az atlaghoz ---
	// CSAK ha a minimum tartasi ido letelt!
	if(holdBars >= MinHold && abs(PairZScore) < ZScore_Exit && NumOpenTotal > 0)
	{
		printf("\n[EXIT-Z] Bar=%d Z=%.3f Hold=%d/%d", Bar, PairZScore, holdBars, (int)MinHold);
		asset("EUR/USD"); exitLong(); exitShort();
		asset("GBP/USD"); exitLong(); exitShort();
		asset("USD/CHF"); exitLong(); exitShort();
	}

	// --- 10B. PORTFOLIO SL/TP: osszesitett P&L alapu exit ---
	// Portfolio SL/TP MINDIG aktiv, MinHold NEM vonatkozik ra (vedelem!)
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
			printf("\n*** PORTFOLIO STOP: P&L=%.2f < SL=%.2f Hold=%d ***", totalProfit, slThreshold, holdBars);
			asset("EUR/USD"); exitLong(); exitShort();
			asset("GBP/USD"); exitLong(); exitShort();
			asset("USD/CHF"); exitLong(); exitShort();
		}
		else if(totalProfit > tpThreshold)
		{
			printf("\n*** PORTFOLIO TP: P&L=%.2f > TP=%.2f Hold=%d ***", totalProfit, tpThreshold, holdBars);
			asset("EUR/USD"); exitLong(); exitShort();
			asset("GBP/USD"); exitLong(); exitShort();
			asset("USD/CHF"); exitLong(); exitShort();
		}
	}

	// ---------------------------------------------------------------
	// 11. PLOTS
	// ---------------------------------------------------------------

	if(!is(LOOKBACK) && PairStd > 0.00001)
	{
		if(PairZScore > -50 && PairZScore < 50)
			plot("PairZScore", PairZScore, NEW, RED);
		if(TriZScore > -50 && TriZScore < 50)
			plot("TriZScore", TriZScore, NEW, BLUE);
		plot("Entry+", ZScore_Entry, LINE, GREEN);
		plot("Entry-", -ZScore_Entry, LINE, GREEN);
		plot("Exit+", ZScore_Exit, LINE, GREY);
		plot("Exit-", -ZScore_Exit, LINE, GREY);
		plot("Correlation", AbsCorr12, NEW, ORANGE);
		plot("Hurst", HurstValue, NEW, BLACK);
		plot("HurstThresh", Hurst_Threshold, LINE, GREY);
	}

	// ---------------------------------------------------------------
	// 12. MONITORING (minden 2000. bar-on + EXITRUN osszesites)
	// ---------------------------------------------------------------

	if(is(LOOKBACK)) return;

	// Rovid status minden 2000. bar-on
	if(Bar % 2000 == 0) {
		printf("\n[BAR %d] Z=%.2f H=%.3f Can=%d EUR:L%dS%d GBP:L%dS%d Attempts=%d",
			Bar, PairZScore, HurstValue, CanTrade,
			NumOpenLong, NumOpenShort, 0, 0, gEntry_attempts);
		// Reset asset for NumOpen display
		asset("EUR/USD"); int eL=NumOpenLong, eS=NumOpenShort;
		asset("GBP/USD"); int gL=NumOpenLong, gS=NumOpenShort;
		printf(" | EUR:L%dS%d GBP:L%dS%d", eL, eS, gL, gS);
	}

	// === TESZT VEGI OSSZESITES ===
	if(is(EXITRUN))
	{
		printf("\n\n========================================================");
		printf("\n  HEDGING CLAUDE - FINAL SUMMARY");
		printf("\n========================================================");
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
