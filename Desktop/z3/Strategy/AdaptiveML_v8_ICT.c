// ===================================================================
// ADAPTIVE ML v8+ICT — v8 alap + 6 ICT feature teszt
// ===================================================================
// CEL: Megvizsgalni hogy az ICT feature-ok (FVG, OTE, Sweep, BOS,
// Displacement) javitjak-e a v8 PERCEPTRON+WFO teljesitmenyet.
//
// VALTOZAS v8-hoz kepest:
//   - 10 feature → 16 feature (+ fvgBull, fvgBear, ote, disp, sweep, bos)
//   - PERCEPTRON 16 inputtal fut
//   - Minden mas AZONOS: WFO, Regime, Smart Exit, Channel logic
//
// TESZT: [Train] majd [Test] → PF osszehasonlitas v8-cal
// ===================================================================

#define NUM_ASSETS 7
#define NUM_FEATURES 16

static var phEntry[NUM_ASSETS];
static int phDir[NUM_ASSETS];
static int phBars[NUM_ASSETS];
static var phFeats[112]; // 7 × 16
static int lastDay;

function checkEquity()
{
	vars EquityCurve = series(EquityLong + EquityShort);
	vars EquityLP = series(LowPass(EquityCurve, 10));
	if(EquityLP[0] < LowPass(EquityLP, 100) && falling(EquityLP))
		setf(TradeMode, TR_PHANTOM);
	else
		resf(TradeMode, TR_PHANTOM);
}

function run()
{
	// === TWO-STEP TRAINING (like v8) ===
	int trainStep = slider(0, 1, 1, 2, "Step", "Train: 1=Rules 2=Params");
	if(Train)
	{
		if(trainStep == 1)
			set(LOGFILE|PLOTNOW|RULES);
		else
			set(LOGFILE|PLOTNOW|PARAMETERS);
	}
	else
		set(LOGFILE|PLOTNOW|RULES|PARAMETERS);

	BarPeriod = 15;
	LookBack = 300;
	StartDate = 20200102;
	EndDate = 20260301;

	Capital = 10000;
	Leverage = 500;
	Hedge = 2;
	EndWeek = 52200;
	StopFactor = 1.5;

	// WFO — 10 cycles like MLDATACOLLECTION v6
	NumWFOCycles = 10;
	DataSplit = 80;

	if(Train) Hedge = 2;
	if(!Train) MaxLong = MaxShort = 1;

	if(Bar == 0 && is(TRADEMODE))
		brokerTrades(0);

	// === SLIDERS (0-3 only!) ===
	// Slider 0 = trainStep (defined at top)
	var MarginPct  = slider(1, 2, 1, 10, "Margin%", "Equity % margin total");
	var MLThresh   = slider(2, 0, -50, 50, "ML Thr", "PERCEPTRON threshold");
	var XGBThresh  = slider(3, 0, 0, 80, "XGB%", "XGBoost min score (test only)");

	// === MULTI-ASSET LOOP ===
	while(asset(loop(
		"EUR/USD", "GBP/USD", "USD/JPY",
		"USD/CAD", "XAU/USD",
		"AUD/USD", "EUR/CHF")))
	{
		string assetCode = "UNKNOWN";
		int aIdx = 0;
		if(strstr(Asset, "EUR/USD"))      { algo("EU"); assetCode = "EURUSD"; aIdx = 0; }
		else if(strstr(Asset, "GBP/USD")) { algo("GB"); assetCode = "GBPUSD"; aIdx = 1; }
		else if(strstr(Asset, "USD/JPY")) { algo("JP"); assetCode = "USDJPY"; aIdx = 2; }
		else if(strstr(Asset, "USD/CAD")) { algo("CA"); assetCode = "USDCAD"; aIdx = 3; }
		else if(strstr(Asset, "XAU/USD")) { algo("XU"); assetCode = "XAUUSD"; aIdx = 4; }
		else if(strstr(Asset, "AUD/USD")) { algo("AU"); assetCode = "AUDUSD"; aIdx = 5; }
		else if(strstr(Asset, "EUR/CHF")) { algo("EC"); assetCode = "EURCHF"; aIdx = 6; }

		// ============================================================
		// OHLC + INDICATORS
		// ============================================================
		vars Close = series(priceClose());
		vars Open  = series(priceOpen());
		vars High  = series(priceHigh());
		vars Low   = series(priceLow());
		var price = Close[0];

		var rsi = RSI(Close, 14);
		if(rsi != rsi) rsi = 50;

		var adx = ADX(14);
		if(adx != adx) adx = 25;

		var hh4 = HH(4);
		var ll4 = LL(4);

		// H1 ATR
		TimeFrame = 4;
		vars H1Close = series(priceClose());
		var h1atr = ATR(14);
		TimeFrame = 1;

		// Structure
		var hurst = Hurst(Close, 100);
		if(hurst != hurst) hurst = 0.5;
		var fracDim = FractalDimension(Close, 100);
		if(fracDim != fracDim) fracDim = 1.5;
		var spearman = Spearman(Close, 20);
		if(spearman != spearman) spearman = 0;

		// Regime
		vars MMI_Raw = series(MMI(Close, 200));
		vars MMI_Smooth = series(SMA(MMI_Raw, 50));
		var mmiVal = MMI_Smooth[0];
		if(mmiVal != mmiVal) mmiVal = 75;

		var adxThresh = optimize(25, 15, 40, 1);
		var mmiThresh = optimize(75, 60, 85, 1);
		int regime_ok = (adx > adxThresh && mmiVal < mmiThresh);

		// ============================================================
		// LINREG CHANNEL (same as v8)
		// ============================================================
		int N = 60; // HARDCODED — feature consistency for two-step training
		var Slope = LinearRegSlope(Close, N);
		var Intercept = LinearRegIntercept(Close, N);
		if(Slope != Slope) Slope = 0;
		if(Intercept != Intercept) Intercept = Close[0];

		vars SlopeS = series(Slope);

		var HighDev = 0, LowDev = 0;
		int i;
		var LinVal;
		for(i = N; i > 0; i--)
		{
			LinVal = Intercept + Slope * (N - i);
			HighDev = max(HighDev, Close[i] - LinVal);
			LowDev = min(LowDev, Close[i] - LinVal);
		}

		var Factor = 0.20; // HARDCODED — feature consistency for two-step training
		var RegLine = Intercept + Slope * N;
		var ChannelWidth = HighDev - LowDev;
		var EntryLow  = RegLine + LowDev  + Factor * (HighDev + LowDev);
		var EntryHigh = RegLine + HighDev - Factor * (HighDev + LowDev);

		var chanPos = 50;
		if(ChannelWidth > 0)
			chanPos = 100.0 * (price - (RegLine + LowDev)) / ChannelWidth;

		// Session filter
		int hr = hour();
		int isRollover = (hr >= 21 && hr <= 22);
		int sessionOK = 0;
		if(strstr(Asset, "EUR/USD"))      sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "GBP/USD")) sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "USD/JPY")) sessionOK = (hr >= 1 && hr <= 16);
		else if(strstr(Asset, "USD/CAD")) sessionOK = (hr >= 12 && hr <= 20);
		else if(strstr(Asset, "XAU/USD")) sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "AUD/USD")) sessionOK = (hr >= 1 && hr <= 16);
		else if(strstr(Asset, "EUR/CHF")) sessionOK = (hr >= 7 && hr <= 20);
		else sessionOK = 1;
		if(isRollover) sessionOK = 0;

		int skipBar = (price == 0 || h1atr <= 0 || ChannelWidth <= 0);

		// ============================================================
		// 16 FEATURES — v8 eredeti 10 + 6 ICT
		// ============================================================
		var chg1 = 0, chg4 = 0;
		if(price > 0)
		{
			chg1 = (priceClose(0) - priceClose(1)) / price;
			chg4 = (priceClose(0) - priceClose(4)) / price;
		}
		if(chg1 != chg1) chg1 = 0;
		if(chg4 != chg4) chg4 = 0;
		if(chanPos != chanPos) chanPos = 50;

		var Sigs[NUM_FEATURES];
		int j, k;

		// --- v8 original 10 features ---
		Sigs[0] = clamp((chanPos - 50) / 50.0, -1, 1);
		Sigs[1] = clamp(Slope / (PIP + 0.00001), -1, 1);
		Sigs[2] = clamp(ChannelWidth / (h1atr + 0.00001) / 10.0, -1, 1);
		Sigs[3] = (rsi - 50) / 50.0;
		Sigs[4] = adx / 50.0 - 1.0;
		Sigs[5] = clamp(chg1 * 100, -1, 1);
		Sigs[6] = clamp(chg4 * 100, -1, 1);
		Sigs[7] = clamp((hurst - 0.5) * 4.0, -1, 1);
		Sigs[8] = clamp((fracDim - 1.5) * 4.0, -1, 1);
		Sigs[9] = clamp(spearman, -1, 1);

		// --- 6 ICT features ---

		// [10] FVG BULL — legközelebbi bullish FVG távolsága
		var nearBullFVG = 5.0;
		for(k = 1; k <= 15; k++)
		{
			if(High[k+2] < Low[k])
			{
				var gapMid = (Low[k] + High[k+2]) / 2.0;
				var d = abs(price - gapMid) / (h1atr + PIP);
				if(d < nearBullFVG) nearBullFVG = d;
			}
		}
		Sigs[10] = clamp(1.0 - nearBullFVG, -1, 1);

		// [11] FVG BEAR — legközelebbi bearish FVG távolsága
		var nearBearFVG = 5.0;
		for(k = 1; k <= 15; k++)
		{
			if(Low[k+2] > High[k])
			{
				var gapMid = (High[k] + Low[k+2]) / 2.0;
				var d = abs(price - gapMid) / (h1atr + PIP);
				if(d < nearBearFVG) nearBearFVG = d;
			}
		}
		Sigs[11] = clamp(1.0 - nearBearFVG, -1, 1);

		// [12] OTE — Optimal Trade Entry (Fib 0.618-0.786 zóna)
		var swHigh = HH(20);
		var swLow = LL(20);
		var swRange = swHigh - swLow;
		var slope20 = LinearRegSlope(Close, 20);
		if(slope20 != slope20) slope20 = 0;

		Sigs[12] = 0;
		if(swRange > PIP * 5)
		{
			if(slope20 > 0)
			{
				var fib618 = swHigh - 0.618 * swRange;
				var fib786 = swHigh - 0.786 * swRange;
				if(price >= fib786 && price <= fib618) Sigs[12] = 1.0;
			}
			if(slope20 < 0)
			{
				var fib618 = swLow + 0.618 * swRange;
				var fib786 = swLow + 0.786 * swRange;
				if(price >= fib618 && price <= fib786) Sigs[12] = -1.0;
			}
		}

		// [13] DISPLACEMENT — erős gyertatest + méret
		var cRange = High[0] - Low[0];
		if(cRange <= 0) cRange = PIP;
		var cBody = abs(Close[0] - Open[0]);
		var bodyRatio = cBody / cRange;

		Sigs[13] = 0;
		if(bodyRatio > 0.7 && cRange > 0.3 * h1atr)
		{
			var strength = cRange / (h1atr + PIP);
			if(Close[0] > Open[0])
				Sigs[13] = clamp(strength, 0, 1);
			else
				Sigs[13] = clamp(-strength, -1, 0);
		}

		// [14] SWEEP — liquidity grab (wick alá/fölé megy majd visszajön)
		var prevHH = High[1];
		var prevLL = Low[1];
		for(k = 2; k <= 15; k++)
		{
			if(High[k] > prevHH) prevHH = High[k];
			if(Low[k] < prevLL) prevLL = Low[k];
		}
		Sigs[14] = 0;
		if(Low[0] < prevLL && Close[0] > prevLL)
			Sigs[14] = clamp((prevLL - Low[0]) / (h1atr + PIP), 0, 1);
		if(High[0] > prevHH && Close[0] < prevHH)
			Sigs[14] = clamp(-(High[0] - prevHH) / (h1atr + PIP), -1, 0);

		// [15] BOS — Break of Structure
		var olderHH = High[10];
		var olderLL = Low[10];
		for(k = 11; k <= 20; k++)
		{
			if(High[k] > olderHH) olderHH = High[k];
			if(Low[k] < olderLL) olderLL = Low[k];
		}
		var recentHH = HH(5);
		var recentLL = LL(5);
		int bosBull = (recentHH > olderHH);
		int bosBear = (recentLL < olderLL);
		Sigs[15] = 0;
		if(bosBull && !bosBear) Sigs[15] = 1;
		if(bosBear && !bosBull) Sigs[15] = -1;

		// ============================================================
		// PERCEPTRON — 16 features (v8 had 10)
		// ============================================================
		var mlLong  = adviseLong(PERCEPTRON+BALANCED+RETURNS, 0, Sigs, NUM_FEATURES);
		var mlShort = adviseShort();

		// Trade params — optimize() MUST be before continue for consistent param order!
		var stopMult = optimize(3.0, 1.0, 5.0, 0.5);
		int lifeTime = optimize(20, 10, 40, 5);

		// === SKIP ===
		if(skipBar || !sessionOK) continue;

		Stop = h1atr * stopMult;
		LifeTime = lifeTime;
		Margin = Equity * MarginPct / 100.0 / NUM_ASSETS;

		// Channel signals
		int channelLong  = (price < EntryLow);
		int channelShort = (price > EntryHigh);

		// ============================================================
		// TRAIN MODE — channel + regime (WFO optimizes thresholds)
		// ============================================================
		if(Train)
		{
			if(channelLong && regime_ok)  enterLong();
			if(channelShort && regime_ok) enterShort();
		}

		// ============================================================
		// TEST/LIVE MODE: Channel + PERCEPTRON + Regime + XGBoost
		// ============================================================
		if(!Train)
		{
			// Session end (from MLDRIVEN)
			int sessionEnd = 0;
			if(strstr(Asset, "EUR/USD"))      sessionEnd = (hr >= 20);
			else if(strstr(Asset, "GBP/USD")) sessionEnd = (hr >= 20);
			else if(strstr(Asset, "USD/JPY")) sessionEnd = (hr >= 16);
			else if(strstr(Asset, "USD/CAD")) sessionEnd = (hr >= 20);
			else if(strstr(Asset, "XAU/USD")) sessionEnd = (hr >= 20);
			else if(strstr(Asset, "AUD/USD")) sessionEnd = (hr >= 16);
			else if(strstr(Asset, "EUR/CHF")) sessionEnd = (hr >= 20);

			// XGBoost prediction (port 5000, 16 features)
			var xgbLong = 50, xgbShort = 50;
			if(is(TRADEMODE) || is(TESTMODE))
			{
				string postData = strf(
					"{\"asset\":\"%s\",\"features\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]}",
					assetCode,
					Sigs[0],Sigs[1],Sigs[2],Sigs[3],Sigs[4],
					Sigs[5],Sigs[6],Sigs[7],Sigs[8],Sigs[9],
					Sigs[10],Sigs[11],Sigs[12],Sigs[13],Sigs[14],Sigs[15]);

				string response = http_transfer("http://127.0.0.1:5000/predict", postData);
				if(response)
				{
					char* pLong = strstr(response, "long_score");
					char* pShort = strstr(response, "short_score");
					if(pLong)
					{
						pLong = strchr(pLong, ':');
						if(pLong) xgbLong = atof(pLong + 1) * 100;
					}
					if(pShort)
					{
						pShort = strchr(pShort, ':');
						if(pShort) xgbShort = atof(pShort + 1) * 100;
					}
				}
			}

			int xgbOK_L = (xgbLong >= XGBThresh);
			int xgbOK_S = (xgbShort >= XGBThresh);

			// ENTRY: Channel + PERCEPTRON + Regime + XGBoost
			if(channelLong && mlLong > MLThresh && regime_ok && xgbOK_L && !NumOpenLong)
				enterLong();

			if(channelShort && mlShort > MLThresh && regime_ok && xgbOK_S && !NumOpenShort)
				enterShort();

			// CH EXIT: decision tree (ATR-based thresholds)
			// LONG exit
			if(NumOpenLong > 0 && !sessionEnd)
			{
				int breakout = (price > EntryHigh);
				var distToTarget = EntryHigh - price;
				int atMiddle = (abs(price - RegLine) < 0.1 * h1atr);
				int momentumOK = (Close[0] > Close[1]);

				if(breakout)
				{
					// Breakout up → hold (trend potential)
				}
				else if(atMiddle && !momentumOK && distToTarget < 0.2 * h1atr)
				{
					printf("\n[CH-EXIT] %s LONG: middle+weak+no room", assetCode);
					exitLong();
				}
			}

			// SHORT exit
			if(NumOpenShort > 0 && !sessionEnd)
			{
				int breakout = (price < EntryLow);
				var distToTarget = price - EntryLow;
				int atMiddle = (abs(price - RegLine) < 0.1 * h1atr);
				int momentumOK = (Close[0] < Close[1]);

				if(breakout)
				{
					// Breakout down → hold (trend potential)
				}
				else if(atMiddle && !momentumOK && distToTarget < 0.2 * h1atr)
				{
					printf("\n[CH-EXIT] %s SHORT: middle+weak+no room", assetCode);
					exitShort();
				}
			}

			// Session end → close remaining
			if(sessionEnd)
			{
				exitLong();
				exitShort();
			}
		}

		// PLOTS (EUR/USD only)
		if(!is(LOOKBACK) && strstr(Asset, "EUR/USD"))
		{
			plot("RegLine",  RegLine,   LINE, GREY);
			plot("EntryHi",  EntryHigh, LINE|DOT, RED);
			plot("EntryLo",  EntryLow,  LINE|DOT, GREEN);
			plot("ChanPos",  chanPos,   NEW, BLUE);
			plot("ML_Long",  mlLong,    NEW, GREEN);
			plot("ML_Short", mlShort,   0, RED);
			plot("ADX",      adx,       NEW, ORANGE);
			plot("MMI",      mmiVal,    0, MAGENTA);
			plot("FVG_B",    Sigs[10]*50+50, NEW, CYAN);
			plot("FVG_S",    Sigs[11]*50+50, 0, MAGENTA);
			plot("OTE",      Sigs[12]*50+50, NEW, YELLOW);
			plot("Sweep",    Sigs[14]*50+50, 0, WHITE);
			plot("BOS",      Sigs[15]*50+50, 0, ORANGE);
		}
	}

	// LOG
	if(is(LOOKBACK)) return;
	if(Bar % 20000 == 0)
		printf("\n[BAR %d] Open=%d Eq=%.0f", Bar, NumOpenTotal, Equity);
	if(is(EXITRUN))
	{
		printf("\n\n=== ADAPTIVE ML v8+ICT FIXED: 16 features, two-step, 6yr data ===");
		printf("\nWFO=%d DataSplit=%d", NumWFOCycles, DataSplit);
		if(Train) printf("\nStep=%d (1=Rules 2=Params)", trainStep);
		printf("\nPERCEPTRON+BALANCED+RETURNS, 16 features, N=60 Factor=0.20 hardcoded");
		int totalTrades = NumWinTotal + NumLossTotal;
		printf("\nTrades=%d Win=%d Loss=%d WR=%.1f%%",
			totalTrades, NumWinTotal, NumLossTotal,
			ifelse(totalTrades > 0, NumWinTotal * 100.0 / totalTrades, 0));
		printf("\nProfit=%.0f Equity=%.0f\n",
			WinTotal + LossTotal, Equity);
	}
}
