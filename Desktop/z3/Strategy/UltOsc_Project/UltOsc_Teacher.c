// =================================================================
// UltOsc Teacher — WFO optimize + CSV export
//
// Tanár: optimize() → WFO → CSV (features + params + trades)
// Diák:  SMA_Ultimate.c → ML server → runtime params + GO/SKIP filter
//
// Pipeline: [Train] → WFO → [Test] → 2x CSV → server trains
// =================================================================

#define NUM_ASSETS 3
#define MAX_PHANTOM 20

static var InitialBalance;
static int lastLogDay[NUM_ASSETS];
static int csvReady;
static int layersLong[NUM_ASSETS];
static int layersShort[NUM_ASSETS];
static int lastLayerBarL[NUM_ASSETS];
static int lastLayerBarS[NUM_ASSETS];

// Phantom trade tracking (GO/SKIP filter training)
static var phEntry[MAX_PHANTOM];
static int phDir[MAX_PHANTOM];
static int phBars[MAX_PHANTOM];
static int phAsset[MAX_PHANTOM];
static var phATR[MAX_PHANTOM];
static var phStop[MAX_PHANTOM];
static var phFeats[120]; // MAX_PHANTOM * 6 features

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
	BarPeriod = 60;
	LookBack = 900;
	StartDate = 20200102;
	EndDate = 20261018;

	set(LOGFILE|PARAMETERS);
	Capital = 10000;
	Leverage = 500;
	Hedge = 2;
	StopFactor = 1.5;
	EndWeek = 52200;

	NumWFOCycles = 10;
	DataSplit = 80;

	if(is(INITRUN))
	{
		InitialBalance = Balance;
		csvReady = 0;
		int k;
		for(k = 0; k < NUM_ASSETS; k++)
		{
			lastLogDay[k] = 0;
			layersLong[k] = 0;
			layersShort[k] = 0;
			lastLayerBarL[k] = 0;
			lastLayerBarS[k] = 0;
		}
		for(k = 0; k < MAX_PHANTOM; k++)
			phDir[k] = 0;

		string header1 = "Date,Asset,ATR_Pct,Range_Pct,Volatility,RSI,Hurst,Current_State,Edge,Threshold_x10,Stop_x10,Cooldown,MaxLayers\n";
		file_write("UltOsc_TrainingData.csv", header1, 0);

		string header2 = "Asset,Direction,ATR_Pct,Range_Pct,Volatility,RSI,Hurst,Current_State,PnlPips,Result\n";
		file_write("UltOsc_TradeData.csv", header2, 0);

		csvReady = 1;
	}

	// === MULTI-ASSET LOOP ===
	while(asset(loop("EUR/USD", "GBP/USD", "USD/JPY")))
	{
		int aIdx = 0;
		string assetCode = "EURUSD";
		if(strstr(Asset, "EUR/USD"))      { aIdx = 0; assetCode = "EURUSD"; }
		else if(strstr(Asset, "GBP/USD")) { aIdx = 1; assetCode = "GBPUSD"; }
		else if(strstr(Asset, "USD/JPY")) { aIdx = 2; assetCode = "USDJPY"; }

		// =========================================
		// OPTIMIZE PARAMS (5 params per asset = 15 total)
		// =========================================
		int Edge = optimize(18, 10, 30, 2);
		int Threshold_x10 = optimize(5, 2, 15, 1);
		var Threshold = Threshold_x10 / 10.0;
		int Stop_x10 = optimize(30, 15, 50, 5);
		var StopMult = Stop_x10 / 10.0;
		int Cooldown = optimize(12, 6, 24, 3);
		int MaxLayers = optimize(4, 1, 6, 1);

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

		var ultOsc = UltimateOsc(H4Close, Edge, 2);
		if(ultOsc != ultOsc) ultOsc = 0;
		vars UltSeries = series(ultOsc);
		TimeFrame = 1;

		var hurst = Hurst(Close, 100);
		if(hurst != hurst) hurst = 0.5;

		// =========================================
		// MARKET FEATURES (6)
		// =========================================
		var ATR_Pct = 0;
		if(price > 0) ATR_Pct = atr14 / price * 100;
		var Range_Pct = 0;
		if(price > 0) Range_Pct = (priceHigh() - priceLow()) / price * 100;
		var Volatility = StdDev(Close, 20);
		if(Volatility != Volatility) Volatility = 0;
		var totalTrades = NumWinTotal + NumLossTotal;
		var WinRate = 0;
		if(totalTrades > 0) WinRate = (var)NumWinTotal / totalTrades;
		var Current_State = 0;
		if(Capital > 0) Current_State = (Balance - InitialBalance) / Capital * 100;

		var feats[6];
		feats[0] = ATR_Pct;    feats[1] = Range_Pct;
		feats[2] = Volatility; feats[3] = rsi;
		feats[4] = hurst;      feats[5] = Current_State;

		// Session filter
		int hr = hour();
		int isRollover = (hr >= 21 && hr <= 22);
		int sessionOK = 0;
		if(strstr(Asset, "EUR/USD"))      sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "GBP/USD")) sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "USD/JPY")) sessionOK = (hr >= 1 && hr <= 16);
		else sessionOK = 1;
		if(isRollover) sessionOK = 0;

		// =========================================
		// PHANTOM TRADE TRACKING — check exits
		// =========================================
		if(!is(TRAINMODE) && csvReady)
		{
			int p;
			for(p = 0; p < MAX_PHANTOM; p++)
			{
				if(phDir[p] == 0) continue;
				if(phAsset[p] != aIdx) continue;

				phBars[p] = phBars[p] + 1;
				var phPnl = (price - phEntry[p]) * phDir[p];

				int doExit = 0;
				// Stop hit
				if(phPnl < -(phATR[p] * phStop[p])) doExit = 1;
				// UltOsc reversal
				if(phDir[p] == 1 && ultOsc < -Threshold) doExit = 1;
				if(phDir[p] == -1 && ultOsc > Threshold) doExit = 1;
				// Stale limit
				if(phBars[p] >= 80) doExit = 1;

				if(doExit)
				{
					var costPips = max(Spread, 5);
					var netPnl = phPnl / PIP - costPips;
					int result = 0;
					if(netPnl > 0) result = 1;

					int base = p * 6;
					string line = strf("%s,%d,%.4f,%.4f,%.6f,%.2f,%.4f,%.2f,%.1f,%d\n",
						assetCode,
						phDir[p],
						phFeats[base+0], phFeats[base+1], phFeats[base+2],
						phFeats[base+3], phFeats[base+4], phFeats[base+5],
						netPnl, result);

					file_append("UltOsc_TradeData.csv", line);
					phDir[p] = 0;
				}
			}
		}

		int skipBar = (price == 0 || atr14 < PIP * 5 || !sessionOK);
		if(skipBar) continue;

		// =========================================
		// LOG DAILY — param CSV (6 features + 5 params)
		// =========================================
		if(!is(TRAINMODE) && Bar > LookBack && csvReady)
		{
			int today = day(0);
			if(today != lastLogDay[aIdx])
			{
				lastLogDay[aIdx] = today;
				char dateBuf[16];
				sprintf(dateBuf, "%04d-%02d-%02d", year(0), month(0), day(0));

				string logLine = strf("%s,%s,%.4f,%.4f,%.6f,%.2f,%.4f,%.2f,%d,%d,%d,%d,%d\n",
					dateBuf, Asset,
					ATR_Pct, Range_Pct, Volatility, rsi, hurst, Current_State,
					Edge, Threshold_x10, Stop_x10, Cooldown, MaxLayers);

				file_append("UltOsc_TrainingData.csv", logLine);
			}
		}

		// =========================================
		// SIGNALS
		// =========================================
		int ultLongSig  = (UltSeries[1] <= Threshold && UltSeries[0] > Threshold);
		int ultShortSig = (UltSeries[1] >= -Threshold && UltSeries[0] < -Threshold);
		int ultTrendL = (ultOsc > Threshold);
		int ultTrendS = (ultOsc < -Threshold);

		int entryOK_L = (ultLongSig && rsi > 45 && rsi < 70);
		int entryOK_S = (ultShortSig && rsi < 55 && rsi > 30);

		int addL = (ultTrendL && layersLong[aIdx] > 0
			&& layersLong[aIdx] < MaxLayers && (Bar - lastLayerBarL[aIdx]) >= Cooldown);
		int addS = (ultTrendS && layersShort[aIdx] > 0
			&& layersShort[aIdx] < MaxLayers && (Bar - lastLayerBarS[aIdx]) >= Cooldown);

		// =========================================
		// ENTRY (pyramid, hedge OK)
		// =========================================
		if(NumOpenLong == 0) layersLong[aIdx] = 0;
		if(NumOpenShort == 0) layersShort[aIdx] = 0;

		if(entryOK_L && layersLong[aIdx] == 0
			&& (Bar - lastLayerBarL[aIdx]) >= Cooldown)
		{
			Stop = atr14 * StopMult;
			Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
			LifeTime = 0;
			enterLong();
			layersLong[aIdx] = 1;
			lastLayerBarL[aIdx] = Bar;

			// Phantom tracking
			if(!is(TRAINMODE) && csvReady)
			{
				int slot = -1;
				int s;
				for(s = 0; s < MAX_PHANTOM; s++)
					if(phDir[s] == 0) { slot = s; break; }
				if(slot >= 0)
				{
					phEntry[slot] = price;
					phDir[slot] = 1;
					phBars[slot] = 0;
					phAsset[slot] = aIdx;
					phATR[slot] = atr14;
					phStop[slot] = StopMult;
					int base = slot * 6;
					int f;
					for(f = 0; f < 6; f++)
						phFeats[base + f] = feats[f];
				}
			}
		}
		else if(addL)
		{
			Stop = atr14 * StopMult;
			Margin = Equity * 0.5 / 100.0 / NUM_ASSETS * (layersLong[aIdx] + 1);
			LifeTime = 0;
			enterLong();
			layersLong[aIdx] = layersLong[aIdx] + 1;
			lastLayerBarL[aIdx] = Bar;
		}

		if(entryOK_S && layersShort[aIdx] == 0
			&& (Bar - lastLayerBarS[aIdx]) >= Cooldown)
		{
			Stop = atr14 * StopMult;
			Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
			LifeTime = 0;
			enterShort();
			layersShort[aIdx] = 1;
			lastLayerBarS[aIdx] = Bar;

			// Phantom tracking
			if(!is(TRAINMODE) && csvReady)
			{
				int slot = -1;
				int s;
				for(s = 0; s < MAX_PHANTOM; s++)
					if(phDir[s] == 0) { slot = s; break; }
				if(slot >= 0)
				{
					phEntry[slot] = price;
					phDir[slot] = -1;
					phBars[slot] = 0;
					phAsset[slot] = aIdx;
					phATR[slot] = atr14;
					phStop[slot] = StopMult;
					int base = slot * 6;
					int f;
					for(f = 0; f < 6; f++)
						phFeats[base + f] = feats[f];
				}
			}
		}
		else if(addS)
		{
			Stop = atr14 * StopMult;
			Margin = Equity * 0.5 / 100.0 / NUM_ASSETS * (layersShort[aIdx] + 1);
			LifeTime = 0;
			enterShort();
			layersShort[aIdx] = layersShort[aIdx] + 1;
			lastLayerBarS[aIdx] = Bar;
		}

	} // end asset loop

	if(is(EXITRUN))
	{
		printf("\n\n=== UltOsc Teacher — WFO + CSV ===");
		printf("\n%d optimize params per asset × %d assets = %d total",
			5, NUM_ASSETS, 5 * NUM_ASSETS);
		printf("\nParam CSV:  UltOsc_TrainingData.csv (6 features + 5 params)");
		printf("\nTrade CSV:  UltOsc_TradeData.csv (GO/SKIP filter)");
	}
}
