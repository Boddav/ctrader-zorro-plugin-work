// =================================================================
// Ehlers Ultimate Oscillator Strategy
//
// UltOsc on H4 timeframe, entry on H1 bars
// Entry: UltOsc crosses threshold (lag-free trend detection)
// Exit:  UltOsc reverses (crosses opposite threshold)
// Pyramid: up to MAX_LAYERS, cooldown between layers
// =================================================================

// ============ KONFIGURÁCIÓ ============
#define CFG_BARPERIOD     60       // H1 bar
#define CFG_STARTDATE     20200101
#define CFG_ENDDATE       20261018
#define CFG_CAPITAL       10000
#define CFG_LEVERAGE      500
#define CFG_STOPFACTOR    1.5

// Stop & Pyramid
#define STOP_ATR_MULT     3.0
#define MAX_LAYERS        4
#define COOLDOWN          12
#define PARTIAL_ATR       0.5
#define DO_HEDGE          1        // netting: egy irány aktív, hedge ki

// Ehlers Ultimate Oscillator (H4 timeframe, TF=4 on H1 bars)
#define ULT_EDGE          18       // H4-re skálázva (napi=30, H4~60%)
#define ULT_WIDTH         2
#define ULT_THRESHOLD     0.5
#define ULT_SMA_PERIOD    8        // UltOsc signal line (SMA periódus)

// FTMO
#define FTMO_DAILY_1STEP  0.03
#define FTMO_DAILY_2STEP  0.05
#define FTMO_MAX_LOSS     0.10
#define FTMO_PROFIT_TARGET 0.10
#define FTMO_BESTDAY_RULE 0.50
#define FTMO_RESUME_RATIO 0.50
// ======================================

#define NUM_ASSETS 7

// Asset config
static int assetActive[NUM_ASSETS];
static int assetSessStart[NUM_ASSETS];
static int assetSessEnd[NUM_ASSETS];
static int numActiveAssets;

// Pyramid tracking
static int layersLong[NUM_ASSETS];
static int layersShort[NUM_ASSETS];
static int lastLayerBarL[NUM_ASSETS];
static int lastLayerBarS[NUM_ASSETS];

// FTMO tracking
static var ftmoStartBalance;
static var ftmoHighBalance;
static var ftmoDayStart;
static int ftmoLastDay;
static int ftmoStopped;
static var ftmoBestDayProfit;
static var ftmoTotalProfit;

// --- Ehlers Ultimate Oscillator ---
var HighPass3(vars Data, int Period)
{
	var a1 = exp(-1.414 * PI / Period);
	var c2 = 2 * a1 * cos(1.414 * PI / Period);
	var c3 = -a1 * a1;
	var c1 = (1.0 + c2 - c3) / 4.0;
	vars HP = series(0, 3);
	return HP[0] = c1 * (Data[0] - 2 * Data[1] + Data[2])
		+ c2 * HP[1] + c3 * HP[2];
}

var UltimateOsc(vars Data, int Edge, int Width)
{
	vars Signals = series(HighPass3(Data, Width * Edge) - HighPass3(Data, Edge));
	var RMS = sqrt(SumSq(Signals, 100) / 100.0);
	return Signals[0] / fix0(RMS);
}

function run()
{
	set(LOGFILE|PLOTNOW);

	BarPeriod = CFG_BARPERIOD;
	LookBack = 900;
	StartDate = CFG_STARTDATE;
	EndDate = CFG_ENDDATE;

	Capital = CFG_CAPITAL;
	Leverage = CFG_LEVERAGE;
	Hedge = 2;
	EndWeek = 52200;
	StopFactor = CFG_STOPFACTOR;

	var RiskSlider = slider(1, 10, 5, 30, "Risk%x10", "Risk % x10 (10=1.0%)") / 10.0;
	int ftmoMode = slider(2, 1, 1, 3, "FTMO", "1=Off 2=1-Step 3=2-Step");

	if(is(INITRUN))
	{
		numActiveAssets = 0;
		int k;
		for(k = 0; k < NUM_ASSETS; k++)
		{
			assetActive[k] = 0;
			assetSessStart[k] = 7;
			assetSessEnd[k] = 20;
		}

		string csv = file_content("Strategy/MLDRIVEN_Assets.csv");
		if(!csv) csv = file_content("MLDRIVEN_Assets.csv");
		if(csv)
		{
			string assetNames[7];
			assetNames[0] = "EUR/USD";
			assetNames[1] = "GBP/USD";
			assetNames[2] = "USD/JPY";
			assetNames[3] = "USD/CAD";
			assetNames[4] = "XAU/USD";
			assetNames[5] = "AUD/USD";
			assetNames[6] = "EUR/CHF";

			for(k = 0; k < NUM_ASSETS; k++)
			{
				char* line = strstr(csv, assetNames[k]);
				if(line)
				{
					char* p = strchr(line, ',');
					if(p)
					{
						p = strchr(p + 1, ',');
						if(p)
						{
							assetActive[k] = atoi(p + 1);
							if(assetActive[k]) numActiveAssets = numActiveAssets + 1;
							p = strchr(p + 1, ',');
							if(p)
							{
								assetSessStart[k] = atoi(p + 1);
								p = strchr(p + 1, ',');
								if(p)
									assetSessEnd[k] = atoi(p + 1);
							}
						}
					}
				}
			}
			printf("\n[CSV] Loaded %d active assets", numActiveAssets);
		}
		else
		{
			printf("\n[CSV] NOT FOUND — defaults (3 assets)");
			assetActive[0] = 1; assetActive[1] = 1; assetActive[2] = 1;
			assetSessStart[0] = 7;  assetSessEnd[0] = 20;
			assetSessStart[1] = 7;  assetSessEnd[1] = 20;
			assetSessStart[2] = 1;  assetSessEnd[2] = 16;
			numActiveAssets = 3;
		}

		for(k = 0; k < NUM_ASSETS; k++)
		{
			layersLong[k] = 0;
			layersShort[k] = 0;
			lastLayerBarL[k] = 0;
			lastLayerBarS[k] = 0;
		}
		ftmoStartBalance = Capital;
		ftmoHighBalance = Capital;
		ftmoDayStart = Capital;
		ftmoLastDay = 0;
		ftmoStopped = 0;
		ftmoBestDayProfit = 0;
		ftmoTotalProfit = 0;
	}

	// =========================================
	// FTMO RISK MANAGEMENT
	// =========================================
	if(ftmoMode >= 2)
	{
		var equity = Equity;
		int today = day(0);

		if(today != ftmoLastDay)
		{
			if(ftmoLastDay > 0)
			{
				var dayProfit = equity - ftmoDayStart;
				if(dayProfit > 0)
				{
					ftmoTotalProfit = ftmoTotalProfit + dayProfit;
					if(dayProfit > ftmoBestDayProfit)
						ftmoBestDayProfit = dayProfit;
				}
			}
			ftmoDayStart = equity;
			ftmoLastDay = today;
			// Új nap: daily és max loss reset (backtest folytatáshoz)
			if(ftmoStopped == 1 || ftmoStopped == 2) ftmoStopped = 0;
			// Profit target: reset + új baseline
			if(ftmoStopped == 3)
			{
				ftmoStopped = 0;
				ftmoStartBalance = equity;
				ftmoHighBalance = equity;
				ftmoBestDayProfit = 0;
				ftmoTotalProfit = 0;
			}
		}

		if(Balance > ftmoHighBalance)
			ftmoHighBalance = Balance;

		var dailyLossLimit = ftmoStartBalance * FTMO_DAILY_2STEP;
		if(ftmoMode == 2) dailyLossLimit = ftmoStartBalance * FTMO_DAILY_1STEP;
		var maxLossLimit = ftmoStartBalance * FTMO_MAX_LOSS;
		var dailyLoss = ftmoDayStart - equity;
		var totalLoss = ftmoHighBalance - equity;

		if(dailyLoss >= dailyLossLimit && !ftmoStopped)
		{
			ftmoStopped = 1;
			printf("\n[FTMO] DAILY LOSS LIMIT! loss=%.0f limit=%.0f", dailyLoss, dailyLossLimit);
		}
		if(totalLoss >= maxLossLimit && !ftmoStopped)
		{
			ftmoStopped = 2;
			printf("\n[FTMO] MAX LOSS LIMIT! loss=%.0f limit=%.0f", totalLoss, maxLossLimit);
		}

		if(ftmoMode == 2 && ftmoTotalProfit > 0 && ftmoBestDayProfit > ftmoTotalProfit * FTMO_BESTDAY_RULE)
		{
			if(Bar % 500 == 0)
				printf("\n[FTMO] BEST DAY WARNING: best=%.0f total=%.0f (%.0f%%)",
					ftmoBestDayProfit, ftmoTotalProfit, ftmoBestDayProfit / ftmoTotalProfit * 100);
		}

		if(ftmoStopped)
		{
			string allAssets[7];
			allAssets[0] = "EUR/USD"; allAssets[1] = "GBP/USD"; allAssets[2] = "USD/JPY";
			allAssets[3] = "USD/CAD"; allAssets[4] = "XAU/USD"; allAssets[5] = "AUD/USD";
			allAssets[6] = "EUR/CHF";
			int fa;
			for(fa = 0; fa < NUM_ASSETS; fa++)
			{
				if(!assetActive[fa]) continue;
				asset(allAssets[fa]);
				exitLong(); exitShort();
			}

			if(today != ftmoLastDay || dailyLoss < dailyLossLimit * FTMO_RESUME_RATIO)
			{
			}
		}

		var profitTarget = ftmoStartBalance * FTMO_PROFIT_TARGET;
		if(equity - ftmoStartBalance >= profitTarget && !ftmoStopped)
		{
			ftmoStopped = 3;
			printf("\n[FTMO] PROFIT TARGET REACHED! profit=%.0f target=%.0f",
				equity - ftmoStartBalance, profitTarget);
		}
	}

	// =========================================
	// ASSET LOOP
	// =========================================
	while(asset(loop("EUR/USD", "GBP/USD", "USD/JPY",
		"USD/CAD", "XAU/USD", "AUD/USD", "EUR/CHF")))
	{
		int aIdx = 0;
		string assetCode = "EURUSD";
		if(strstr(Asset, "EUR/USD"))      { aIdx = 0; assetCode = "EURUSD"; }
		else if(strstr(Asset, "GBP/USD")) { aIdx = 1; assetCode = "GBPUSD"; }
		else if(strstr(Asset, "USD/JPY")) { aIdx = 2; assetCode = "USDJPY"; }
		else if(strstr(Asset, "USD/CAD")) { aIdx = 3; assetCode = "USDCAD"; }
		else if(strstr(Asset, "XAU/USD")) { aIdx = 4; assetCode = "XAUUSD"; }
		else if(strstr(Asset, "AUD/USD")) { aIdx = 5; assetCode = "AUDUSD"; }
		else if(strstr(Asset, "EUR/CHF")) { aIdx = 6; assetCode = "EURCHF"; }

		// =========================================
		// INDICATORS — ALL series() BEFORE continue!
		// =========================================
		TimeFrame = 1;
		vars Close = series(priceClose());
		var price = Close[0];

		var rsi = RSI(Close, 14);
		if(rsi != rsi) rsi = 50;

		// H4 timeframe for ATR and UltOsc
		TimeFrame = 4;
		vars H4Close = series(priceClose());
		var atr14 = ATR(14);
		if(atr14 != atr14) atr14 = PIP * 50;

		// Ehlers Ultimate Oscillator on H4
		var ultOsc = UltimateOsc(H4Close, ULT_EDGE, ULT_WIDTH);
		if(ultOsc != ultOsc) ultOsc = 0;
		vars UltSeries = series(ultOsc);
		// Signal line: UltOsc EMA
		vars UltEMA = series(EMA(UltSeries, ULT_SMA_PERIOD));
		// UltOsc RSI
		var ultRSI = RSI(UltSeries, 14);
		if(ultRSI != ultRSI) ultRSI = 50;
		TimeFrame = 1;

		// Skip inactive assets — AFTER series()!
		if(!assetActive[aIdx]) continue;

		// Session filter
		int hr = hour();
		int isRollover = (hr >= 21 && hr <= 22);
		int sessionOK = (hr >= assetSessStart[aIdx] && hr <= assetSessEnd[aIdx]);
		if(isRollover) sessionOK = 0;

		// Skip bar
		if(price == 0 || atr14 < PIP * 5 || !sessionOK) continue;
		if(ftmoStopped) continue;

		// =========================================
		// SIGNALS — UltOsc × Signal Line crossover
		// =========================================
		// Crossover: UltOsc crosses ABOVE its SMA = bullish
		// Crossover: UltOsc crosses BELOW its SMA = bearish
		int ultLongSig  = (UltSeries[1] <= UltEMA[1] && UltSeries[0] > UltEMA[0]);
		int ultShortSig = (UltSeries[1] >= UltEMA[1] && UltSeries[0] < UltEMA[0]);
		// Trend: UltOsc above/below its EMA (for pyramid adds)
		int ultTrendL = (ultOsc > UltEMA[0]);
		int ultTrendS = (ultOsc < UltEMA[0]);

		int entryOK_L = (ultLongSig && rsi > 45 && rsi < 70);
		int entryOK_S = (ultShortSig && rsi < 55 && rsi > 30);

		int addL = (ultTrendL && layersLong[aIdx] > 0
			&& layersLong[aIdx] < MAX_LAYERS && (Bar - lastLayerBarL[aIdx]) >= COOLDOWN);
		int addS = (ultTrendS && layersShort[aIdx] > 0
			&& layersShort[aIdx] < MAX_LAYERS && (Bar - lastLayerBarS[aIdx]) >= COOLDOWN);

		// =========================================
		// EXIT: stop-loss only (no signal exit — hedge mode)
		// Zorro StopFactor handles stop-loss automatically
		// =========================================

		// =========================================
		// PARTIAL CLOSE: LIFO
		// =========================================
		var partialThresh = PARTIAL_ATR * atr14;
		if(layersLong[aIdx] > 1)
		{
			for(current_trades)
			{
				var tradeProfit = price - TradePriceOpen;
				if(TradeIsLong && TradeLots >= layersLong[aIdx]
					&& tradeProfit > partialThresh)
				{
					exitTrade(ThisTrade);
					layersLong[aIdx] = layersLong[aIdx] - 1;
					break;
				}
			}
		}
		if(layersShort[aIdx] > 1)
		{
			for(current_trades)
			{
				var tradeProfit = TradePriceOpen - price;
				if(TradeIsShort && TradeLots >= layersShort[aIdx]
					&& tradeProfit > partialThresh)
				{
					exitTrade(ThisTrade);
					layersShort[aIdx] = layersShort[aIdx] - 1;
					break;
				}
			}
		}

		// =========================================
		// LOT SIZING & MARGIN SAFETY
		// =========================================
		int baseLots = max(1, (int)(Equity * (RiskSlider / 3.0) / 100.0 / numActiveAssets));

		if(NumOpenLong == 0) layersLong[aIdx] = 0;
		if(NumOpenShort == 0) layersShort[aIdx] = 0;

		var usedMargin = MarginTotal;
		var freeMargin = Equity - usedMargin;
		var marginLevel = 0;
		if(usedMargin > 0) marginLevel = Equity / usedMargin * 100;
		int safeToOpen = 1;
		if(usedMargin > 0 && marginLevel < 200) safeToOpen = 0;
		if(freeMargin < Equity * 0.1) safeToOpen = 0;

		// =========================================
		// ENTRY (pyramid)
		// =========================================
		int hedgeOK_L = DO_HEDGE || (NumOpenShort == 0);
		int hedgeOK_S = DO_HEDGE || (NumOpenLong == 0);

		if(safeToOpen && entryOK_L && hedgeOK_L && layersLong[aIdx] == 0
			&& (Bar - lastLayerBarL[aIdx]) >= COOLDOWN)
		{
			Lots = baseLots;
			Stop = atr14 * STOP_ATR_MULT;
			LifeTime = 0;
			enterLong();
			layersLong[aIdx] = 1;
			lastLayerBarL[aIdx] = Bar;
			printf("\n[ENTRY] LONG %s @ %.5f lots=%d ult=%.2f", assetCode, price, baseLots, ultOsc);
		}
		else if(safeToOpen && addL)
		{
			Lots = baseLots + layersLong[aIdx];
			Stop = atr14 * STOP_ATR_MULT;
			LifeTime = 0;
			enterLong();
			layersLong[aIdx] = layersLong[aIdx] + 1;
			lastLayerBarL[aIdx] = Bar;
		}

		if(safeToOpen && entryOK_S && hedgeOK_S && layersShort[aIdx] == 0
			&& (Bar - lastLayerBarS[aIdx]) >= COOLDOWN)
		{
			Lots = baseLots;
			Stop = atr14 * STOP_ATR_MULT;
			LifeTime = 0;
			enterShort();
			layersShort[aIdx] = 1;
			lastLayerBarS[aIdx] = Bar;
			printf("\n[ENTRY] SHORT %s @ %.5f lots=%d ult=%.2f", assetCode, price, baseLots, ultOsc);
		}
		else if(safeToOpen && addS)
		{
			Lots = baseLots + layersShort[aIdx];
			Stop = atr14 * STOP_ATR_MULT;
			LifeTime = 0;
			enterShort();
			layersShort[aIdx] = layersShort[aIdx] + 1;
			lastLayerBarS[aIdx] = Bar;
		}

		// =========================================
		// PLOTS
		// =========================================
		if(!is(LOOKBACK) && aIdx == 0)
		{
			plot("UltOsc", ultOsc, NEW|LINE, RED);
			plot("EMA", UltEMA[0], LINE, BLUE);
			plot("Thresh+", ULT_THRESHOLD, LINE|DOT, GREEN);
			plot("Thresh-", -ULT_THRESHOLD, LINE|DOT, GREEN);
			plot("Zero", 0, LINE|DOT, GREY);
			plot("UltRSI", (ultRSI - 50) / 16.67, LINE, ORANGE);
		}

		if(Bar % 2000 == 0)
			printf("\n[%s] ult=%.2f rsi=%.0f layers L=%d S=%d",
				assetCode, ultOsc, rsi, layersLong[aIdx], layersShort[aIdx]);

	} // end asset loop

	if(is(EXITRUN))
	{
		printf("\n\n=== Ehlers Ultimate Oscillator Strategy (H4 signal, H1 entry) ===");
		printf("\nAssets: %d active", numActiveAssets);
		printf("\nUltOsc: Edge=%d Width=%d Threshold=%.1f (on H4 TF)", ULT_EDGE, ULT_WIDTH, ULT_THRESHOLD);
		printf("\nStop=%.1f ATR, Pyramid: max %d layers, cooldown %d bars",
			STOP_ATR_MULT, MAX_LAYERS, COOLDOWN);
	}
}
