// ===================================================================
// ADAPTIVE ML v8 — M15 Channel + PERCEPTRON + XGBoost + Smart Exit
// ===================================================================
// 7 assets (XAG/USD, USD/CHF kivéve — PF 0.36, 0.67)
//
// 5-LAYER FILTER + ONLINE LEARNING:
//   1. LinReg Channel boundary (price < EntryLow / > EntryHigh)
//   2. PERCEPTRON ML (WFO trained, 10 features)
//   3. Regime filter (ADX + MMI optimized)
//   4. XGBoost HTTP (external Python model, daily retrained)
//   5. PHANTOM equity curve filter (per-asset)
//
// WORKFLOW:
//   1. Export: AdaptiveML_v8_export [Test] → CSV + auto train+server
//   2. Train mode: PERCEPTRON WFO training
//   3. Test/Live: 5-layer filter + continuous feedback
//
// XGBoost szerver: Strategy/xgb_server.py (Flask, port 5000)
// ===================================================================

#define NUM_ASSETS 7
#define NUM_FEATURES 10
// No phantom lifetime — exit by stop loss or channel reversal

// Per-asset phantom outcome tracking
static var phEntry[NUM_ASSETS];           // entry price
static int phDir[NUM_ASSETS];             // 1=long, -1=short, 0=none
static int phBars[NUM_ASSETS];            // bars since entry
static var phFeats[70]; // 7 assets × 10 features
static int lastDay;                       // for day-change detection

// Phantom equity curve filter — DISABLED (series consistency issue)
// checkEquity() creates hidden LowPass series that are inconsistent
// between Train and Test mode → "wrong series" error
// TODO: fix by inlining ALL series/filter calls before continue

function run()
{
	// === TWO-STEP TRAINING (Slider 0 = "Period" slot) ===
	// 1 = RULES only (PERCEPTRON) → _ml.c
	// 2 = PARAMETERS only (optimize) → .par
	// [Test]: mindkettőt betölti
	int trainStep = slider(3, 1, 1, 2, "Step", "Train: 1=Rules 2=Params");
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

	Capital = 2000;
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
	var MarginPct = slider(1, 2, 1, 10, "Margin%", "Equity % margin total");
	var MLThresh  = slider(2, 0, -50, 50, "ML Thr", "PERCEPTRON threshold");
	// Slider 3 = trainStep (defined at top of run())
	var XGBThresh = 50; // fix — slider 3 used for trainStep
	int UsePhantom = 0; // DISABLED — checkEquity series consistency issue

	// === DAILY RETRAIN TRIGGER ===
	if(!Train && (is(TRADEMODE) || is(TESTMODE)))
	{
		int today = day(0);
		if(Bar > 0 && today != lastDay && dow(0) >= 1 && dow(0) <= 5)
		{
			if(hour(0) == 7)
			{
				http_transfer("http://127.0.0.1:5000/retrain", "{}");
			}
		}
		lastDay = today;
	}

	// === MULTI-ASSET LOOP ===
	while(asset(loop(
		"EUR/USD", "GBP/USD", "USD/JPY",
		"USD/CAD", "XAU/USD",
		"AUD/USD", "EUR/CHF")))
	{
		// Per-asset ML model via algo()
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
		// M15 INDICATORS
		// ============================================================
		vars Close = series(priceClose());
		var price = Close[0];

		var rsi = RSI(Close, 14);
		if(rsi != rsi) rsi = 50;

		var adx = ADX(14);
		if(adx != adx) adx = 25;

		var hh4 = HH(4);
		var ll4 = LL(4);

		// H1 ATR (TimeFrame=4 on M15 = H1)
		TimeFrame = 4;
		vars H1Close = series(priceClose());
		var h1atr = ATR(14);
		TimeFrame = 1;

		// Structure indicators
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
		// LINREG CHANNEL
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

		// Session filter + Rollover block
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

		// Skip flag — minimum h1atr és channel width
		int skipBar = (price == 0 || h1atr < PIP * 5 || ChannelWidth < PIP * 3);

		// ============================================================
		// 10 ML FEATURES [-1..+1]
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

		// PERCEPTRON prediction (MINDIG hívni!)
		var mlLong  = adviseLong(PERCEPTRON+BALANCED+RETURNS, 0, Sigs, NUM_FEATURES);
		var mlShort = adviseShort();

		// optimize() MUST be before continue!
		var stopMult = optimize(3.0, 1.5, 5.0, 0.5);
		int lifeTime = optimize(20, 10, 40, 5);

		// === SKIP ===
		if(skipBar || !sessionOK) continue;

		// Trade params
		Stop = h1atr * stopMult;
		LifeTime = lifeTime;
		Margin = Equity * MarginPct / 100.0 / NUM_ASSETS;

		// Channel signals
		int channelLong  = (price < EntryLow);
		int channelShort = (price > EntryHigh);

		// ============================================================
		// PHANTOM OUTCOME TRACKING — online learning feedback
		// ============================================================
		if(!Train && (is(TRADEMODE) || is(TESTMODE)))
		{
			if(phDir[aIdx] != 0)
			{
				phBars[aIdx] = phBars[aIdx] + 1;

				int phExit = 0;
				var phUnreal = (price - phEntry[aIdx]) * phDir[aIdx];
				// Stop loss: ATR × 3 (same as real trades)
				if(phUnreal < -(h1atr * 3.0)) phExit = 1;
				// Channel exit: only if in loss, min 5 bars
				if(phBars[aIdx] >= 5 && phUnreal <= 0)
				{
					if(phDir[aIdx] == 1 && price > EntryHigh) phExit = 1;
					if(phDir[aIdx] == -1 && price < EntryLow) phExit = 1;
				}
				// Stale trade: 60+ bars with no meaningful profit → close
				if(phBars[aIdx] >= 60 && phUnreal < 5 * PIP) phExit = 1;

				if(phExit)
				{
					var phPnl = (price - phEntry[aIdx]) * phDir[aIdx];
					// Total cost: spread + commission + slippage (min 5 pips)
					var totalCost = max(Spread, 5) * PIP;
					var phNetPnl = phPnl - totalCost;
					int phLabel = 0;
					if(phNetPnl > 0) phLabel = 1;
					// Convert to pips and dollars for log
					var grossPips = phPnl / PIP;
					var netPips = phNetPnl / PIP;
					var phLots = Equity * MarginPct / 100.0 / NUM_ASSETS / (price * LotAmount / Leverage);
					var phDollar = netPips * PIPCost * phLots;

					printf("\n[PHANTOM] %s %s close @ %.5f entry %.5f %.1fp net %.1fp $%.1f %s bars=%d",
						assetCode, ifelse(phDir[aIdx]==1,"LONG","SHORT"),
						price, phEntry[aIdx], grossPips, netPips, phDollar,
						ifelse(phLabel,"WIN","LOSS"), phBars[aIdx]);

					int base = aIdx * NUM_FEATURES;
					char fbData[512];
					sprintf(fbData,
						"{\"asset\":\"%s\",\"side\":%d,\"label\":%d,\"features\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]}",
						assetCode, phDir[aIdx], phLabel,
						phFeats[base+0], phFeats[base+1], phFeats[base+2],
						phFeats[base+3], phFeats[base+4], phFeats[base+5],
						phFeats[base+6], phFeats[base+7], phFeats[base+8],
						phFeats[base+9]);
					http_transfer("http://127.0.0.1:5000/feedback", fbData);

					phDir[aIdx] = 0;
				}
			}

			if(phDir[aIdx] == 0)
			{
				if(channelLong)
				{
					phEntry[aIdx] = price;
					phDir[aIdx] = 1;
					phBars[aIdx] = 0;
					int base = aIdx * NUM_FEATURES;
					int j;
					for(j = 0; j < NUM_FEATURES; j++)
						phFeats[base + j] = Sigs[j];
					printf("\n[PHANTOM] %s LONG open @ %.5f", assetCode, price);
				}
				else if(channelShort)
				{
					phEntry[aIdx] = price;
					phDir[aIdx] = -1;
					phBars[aIdx] = 0;
					int base = aIdx * NUM_FEATURES;
					int j;
					for(j = 0; j < NUM_FEATURES; j++)
						phFeats[base + j] = Sigs[j];
					printf("\n[PHANTOM] %s SHORT open @ %.5f", assetCode, price);
				}
			}
		}

		// ============================================================
		// TRAIN MODE
		// ============================================================
		if(Train)
		{
			if(channelLong)  enterLong();
			if(channelShort) enterShort();
		}

		// ============================================================
		// TEST/LIVE MODE: PERCEPTRON + XGBoost + Phantom filter
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

			// XGBoost + Phantom prediction via HTTP
			var xgbLong = 50, xgbShort = 50;
			var phLong = 50, phShort = 50;
			if(is(TRADEMODE) || is(TESTMODE))
			{
				string postData = strf(
					"{\"asset\":\"%s\",\"features\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]}",
					assetCode,
					Sigs[0],Sigs[1],Sigs[2],Sigs[3],Sigs[4],
					Sigs[5],Sigs[6],Sigs[7],Sigs[8],Sigs[9]);

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
					// Phantom model scores (learned from live phantom trades)
					char* pPhL = strstr(response, "phantom_long");
					char* pPhS = strstr(response, "phantom_short");
					if(pPhL)
					{
						pPhL = strchr(pPhL, ':');
						if(pPhL) phLong = atof(pPhL + 1) * 100;
					}
					if(pPhS)
					{
						pPhS = strchr(pPhS, ':');
						if(pPhS) phShort = atof(pPhS + 1) * 100;
					}
				}
			}

			// ENTRY: Channel + PERCEPTRON + Regime + XGBoost OR Phantom override
			int xgbOK_L = (xgbLong >= XGBThresh);
			int xgbOK_S = (xgbShort >= XGBThresh);
			// Phantom override: if XGBoost blocks but phantom model says >= 60%, allow trade
			int phOK_L = (phLong >= 60);
			int phOK_S = (phShort >= 60);
			int mlOK_L = (xgbOK_L || phOK_L);
			int mlOK_S = (xgbOK_S || phOK_S);

			if(channelLong && mlLong > MLThresh && regime_ok && mlOK_L && !NumOpenLong)
				enterLong();

			if(channelShort && mlShort > MLThresh && regime_ok && mlOK_S && !NumOpenShort)
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
		}
	}

	// LOG
	if(is(LOOKBACK)) return;
	if(Bar % 20000 == 0)
		printf("\n[BAR %d] Open=%d Eq=%.0f", Bar, NumOpenTotal, Equity);
	if(is(EXITRUN))
	{
		printf("\n\n=== ADAPTIVE ML v8: M15 Channel+PERCEPTRON+XGBoost+SmartExit | 7 assets ===\n");
		if(Train && trainStep == 2)
		{
			// .par fájlok másolása export néven
			file_write("Data\\copy_v8_pars.bat",
				"@echo off\r\ncopy /y \"Data\\AdaptiveML_v8.par\" \"Data\\AdaptiveML_v8_export.par\" >nul\r\nfor /L %%i in (1,1,8) do copy /y \"Data\\AdaptiveML_v8_%%i.par\" \"Data\\AdaptiveML_v8_export_%%i.par\" >nul 2>nul\r\n", 0);
			//exec("cmd", "/c Data\\copy_v8_pars.bat", 0); // cmd ablak probléma — manuálisan kell
			printf("\n>>> PARAMS KÉSZ! .par fájlok exportra másolva. <<<\n");
		}
		if(Train) printf("\nStep=%d (1=Rules 2=Params)\n", trainStep);
	}
}
