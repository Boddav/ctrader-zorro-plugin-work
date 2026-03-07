// =================================================================
// ML-DRIVEN v6 — algo() + FCFS + TimeFrame + Smart Exit
//
// SMA ALGO (trend-following):
//   - TimeFrame from ML, SMA crossover + RSI + regime
//   - Pyramid lot (0.01→0.04), LifeTime=0
//   - Exit: opposite crossover OR trailing stop
//   - Holds overnight
//
// CH ALGO (mean-reversion):
//   - LinReg Channel + regime
//   - Fix 0.01 lot, LifeTime from ML
//   - Exit: RegLine target OR session end OR LifeTime
//   - NO overnight — closes at session end
//
// FCFS: First Come First Served — one algo active, other waits
// Server: TENSORFLOWMODEL.py serve (port 5001)
// =================================================================

#define NUM_ASSETS 7
#define MAX_LAYERS 4
#define ML_URL_PREDICT "http://127.0.0.1:5001/predict"
#define ML_URL_FILTER  "http://127.0.0.1:5001/filter"
#define ML_URL_VOTE    "http://127.0.0.1:5001/algo_vote"

// Asset config from CSV
static int assetActive[NUM_ASSETS];
static int assetSessStart[NUM_ASSETS];
static int assetSessEnd[NUM_ASSETS];
static int numActiveAssets;

// ML-predicted params per asset (12 params: 6 SMA + 6 CH)
static int mlSmaTF[NUM_ASSETS];
static int mlFastMA[NUM_ASSETS];
static int mlSlowMA[NUM_ASSETS];
static int mlSmaStop_x10[NUM_ASSETS];
static int mlAdxSMA[NUM_ASSETS];
static int mlMmiSMA[NUM_ASSETS];
static int mlN[NUM_ASSETS];
static int mlFactor_x100[NUM_ASSETS];
static int mlChStop_x10[NUM_ASSETS];
static int mlLifeTime[NUM_ASSETS];
static int mlAdxCH[NUM_ASSETS];
static int mlMmiCH[NUM_ASSETS];
static int mlReady[NUM_ASSETS];
static int lastPredictHour[NUM_ASSETS];
static int mlAlgoVote[NUM_ASSETS];
static int lastVoteHour[NUM_ASSETS];
static int lastFilterCloseHour[NUM_ASSETS];
static int lastChFilterHour[NUM_ASSETS];

// FCFS + pyramid tracking
static int layersLong[NUM_ASSETS];
static int layersShort[NUM_ASSETS];
static int lastLayerBarL[NUM_ASSETS];
static int lastLayerBarS[NUM_ASSETS];
static int smaHasOpen[NUM_ASSETS];
static int chHasOpen[NUM_ASSETS];

// FTMO tracking
static var ftmoStartBalance;
static var ftmoHighBalance;
static var ftmoDayStart;
static int ftmoLastDay;
static int ftmoStopped;
static var ftmoBestDayProfit;
static var ftmoTotalProfit;

function run()
{
	set(LOGFILE|PLOTNOW);

	BarPeriod = 60;
	LookBack = 900;
	StartDate = 20200101;
	EndDate = 20261018;

	Capital = 10000;
	Leverage = 500;
	Hedge = 2;
	EndWeek = 52200;
	StopFactor = 1.5;

	var RiskSlider = slider(1, 10, 5, 30, "Risk%x10", "Risk % x10 (10=1.0%)") / 10.0;
	int algoMode = slider(2, 1, 1, 4, "Algo", "1=Both 2=SMA 3=CH 4=Auto");
	int ftmoMode = slider(3, 1, 1, 3, "FTMO", "1=Off 2=1-Step 3=2-Step");

	if(is(INITRUN))
	{
		// Load asset config from CSV
		numActiveAssets = 0;
		int k;
		for(k = 0; k < NUM_ASSETS; k++)
		{
			assetActive[k] = 0;
			assetSessStart[k] = 7;
			assetSessEnd[k] = 20;
		}

		string csv = file_content("Strategy\\MLDRIVEN_Assets.csv");
		if(csv)
		{
			// Known asset order — parse each from CSV
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
					// Skip Asset name → find Code comma
					char* p = strchr(line, ',');
					if(p)
					{
						// Skip Code → find Active comma
						p = strchr(p + 1, ',');
						if(p)
						{
							assetActive[k] = atoi(p + 1);
							if(assetActive[k]) numActiveAssets = numActiveAssets + 1;
							// Skip Active → find SessionStart comma
							p = strchr(p + 1, ',');
							if(p)
							{
								assetSessStart[k] = atoi(p + 1);
								// Skip SessionStart → find SessionEnd comma
								p = strchr(p + 1, ',');
								if(p)
									assetSessEnd[k] = atoi(p + 1);
							}
						}
					}
				}
			}
			printf("\n[CSV] Loaded MLDRIVEN_Assets.csv: %d active assets", numActiveAssets);
			for(k = 0; k < NUM_ASSETS; k++)
			{
				if(assetActive[k])
					printf("\n  [%d] %s session %d-%d", k, assetNames[k], assetSessStart[k], assetSessEnd[k]);
			}
		}
		else
		{
			printf("\n[CSV] MLDRIVEN_Assets.csv NOT FOUND — using defaults (3 assets)");
			assetActive[0] = 1; assetActive[1] = 1; assetActive[2] = 1;
			assetSessStart[0] = 7;  assetSessEnd[0] = 20;
			assetSessStart[1] = 7;  assetSessEnd[1] = 20;
			assetSessStart[2] = 1;  assetSessEnd[2] = 16;
			numActiveAssets = 3;
		}

		for(k = 0; k < NUM_ASSETS; k++)
		{
			mlSmaTF[k] = 4;
			mlFastMA[k] = 20;
			mlSlowMA[k] = 50;
			mlSmaStop_x10[k] = 30;
			mlAdxSMA[k] = 25;
			mlMmiSMA[k] = 75;
			mlN[k] = 60;
			mlFactor_x100[k] = 20;
			mlChStop_x10[k] = 30;
			mlLifeTime[k] = 20;
			mlAdxCH[k] = 25;
			mlMmiCH[k] = 75;
			mlReady[k] = 0;
			lastPredictHour[k] = -1;
			lastFilterCloseHour[k] = -1;
			lastChFilterHour[k] = -1;
			mlAlgoVote[k] = 1;
			lastVoteHour[k] = -1;
			layersLong[k] = 0;
			layersShort[k] = 0;
			lastLayerBarL[k] = 0;
			lastLayerBarS[k] = 0;
			smaHasOpen[k] = 0;
			chHasOpen[k] = 0;
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
	// FTMO RISK MANAGEMENT (before asset loop)
	// =========================================
	if(ftmoMode >= 2)
	{
		var equity = Equity;
		int today = day(0);

		// New day → update daily tracking
		if(today != ftmoLastDay)
		{
			// Record previous day profit for Best Day Rule
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
			if(ftmoStopped == 1) ftmoStopped = 0; // daily reset, max loss stays
		}

		// Update high water mark (trailing max loss)
		if(Balance > ftmoHighBalance)
			ftmoHighBalance = Balance;

		// 1-Step: 3% daily, 10% max trailing | 2-Step: 5% daily, 10% max
		var dailyLossLimit = ftmoStartBalance * 0.05;
		if(ftmoMode == 2) dailyLossLimit = ftmoStartBalance * 0.03;
		var maxLossLimit = ftmoStartBalance * 0.10;
		var dailyLoss = ftmoDayStart - equity;
		var totalLoss = ftmoHighBalance - equity;

		if(dailyLoss >= dailyLossLimit && !ftmoStopped)
		{
			ftmoStopped = 1;
			printf("\n[FTMO] DAILY LOSS LIMIT! loss=%.0f limit=%.0f → ALL CLOSED", dailyLoss, dailyLossLimit);
		}
		if(totalLoss >= maxLossLimit && !ftmoStopped)
		{
			ftmoStopped = 2;
			printf("\n[FTMO] MAX LOSS LIMIT! loss=%.0f limit=%.0f → ALL CLOSED", totalLoss, maxLossLimit);
		}

		// Best Day Rule (1-Step only)
		if(ftmoMode == 2 && ftmoTotalProfit > 0 && ftmoBestDayProfit > ftmoTotalProfit * 0.50)
		{
			if(Bar % 500 == 0)
				printf("\n[FTMO] BEST DAY WARNING: best=%.0f total=%.0f (%.0f%%)",
					ftmoBestDayProfit, ftmoTotalProfit, ftmoBestDayProfit / ftmoTotalProfit * 100);
		}

		// If stopped → close all and skip trading
		if(ftmoStopped)
		{
			// Close everything (all active assets)
			string allAssets[7];
			allAssets[0] = "EUR/USD"; allAssets[1] = "GBP/USD"; allAssets[2] = "USD/JPY";
			allAssets[3] = "USD/CAD"; allAssets[4] = "XAU/USD"; allAssets[5] = "AUD/USD";
			allAssets[6] = "EUR/CHF";
			int fa;
			for(fa = 0; fa < NUM_ASSETS; fa++)
			{
				if(!assetActive[fa]) continue;
				asset(allAssets[fa]);
				algo("SMA"); exitLong(); exitShort();
				algo("CH"); exitLong(); exitShort();
			}

			// Reset next day
			if(today != ftmoLastDay || dailyLoss < dailyLossLimit * 0.5)
			{
				// ftmoStopped stays until next day resets
			}
		}

		// Profit target: 1-Step=10%, 2-Step Phase1=10%, Phase2=5%
		var profitTarget = ftmoStartBalance * 0.10;
		if(equity - ftmoStartBalance >= profitTarget && !ftmoStopped)
		{
			ftmoStopped = 3;
			printf("\n[FTMO] PROFIT TARGET REACHED! profit=%.0f target=%.0f → CHALLENGE COMPLETE",
				equity - ftmoStartBalance, profitTarget);
		}

		// FTMO status log
		if(Bar % 2000 == 0)
			printf("\n[FTMO] Eq=%.0f DayLoss=%.1f/%.0f MaxLoss=%.1f/%.0f HWM=%.0f",
				equity, dailyLoss, dailyLossLimit, totalLoss, maxLossLimit, ftmoHighBalance);
	}

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
		// SHARED INDICATORS — ALL series() BEFORE any continue!
		// =========================================
		TimeFrame = 1;
		vars Close = series(priceClose());
		var price = Close[0];

		var rsi = RSI(Close, 14);
		if(rsi != rsi) rsi = 50;
		var adx = ADX(14);
		if(adx != adx) adx = 25;

		TimeFrame = 4;
		vars H4Close = series(priceClose());
		var h4atr = ATR(14);
		TimeFrame = 1;

		var hurst = Hurst(Close, 100);
		if(hurst != hurst) hurst = 0.5;

		vars MMI_Raw = series(MMI(Close, 200));
		vars MMI_Smooth = series(SMA(MMI_Raw, 50));
		var mmiVal = MMI_Smooth[0];
		if(mmiVal != mmiVal) mmiVal = 75;

		vars SMA20 = series(SMA(Close, 20));
		vars SMA50 = series(SMA(Close, 50));
		vars BarIdx = series(Bar);

		// SMA at ML-predicted TimeFrame
		int smaTF = mlSmaTF[aIdx];
		int FastMA = mlFastMA[aIdx];
		int SlowMA = mlSlowMA[aIdx];
		if(FastMA >= SlowMA) SlowMA = FastMA + 10;

		TimeFrame = smaTF;
		vars SMA_F = series(SMA(Close, FastMA));
		vars SMA_S = series(SMA(Close, SlowMA));
		TimeFrame = 1;

		// Skip inactive assets (from CSV) — AFTER series() for consistency!
		if(!assetActive[aIdx]) continue;

		// Channel params
		int N = mlN[aIdx];
		var Factor = mlFactor_x100[aIdx] / 100.0;
		var smaStop = mlSmaStop_x10[aIdx] / 10.0;
		var chStop = mlChStop_x10[aIdx] / 10.0;
		int lifeTime = mlLifeTime[aIdx];
		int adxSMA = mlAdxSMA[aIdx];
		int mmiSMA = mlMmiSMA[aIdx];
		int adxCH = mlAdxCH[aIdx];
		int mmiCH = mlMmiCH[aIdx];

		// =========================================
		// LINREG CHANNEL
		// =========================================
		var Slope = LinearRegSlope(Close, N);
		var Intercept = LinearRegIntercept(Close, N);
		if(Slope != Slope) Slope = 0;
		if(Intercept != Intercept) Intercept = Close[0];

		var HighDev = 0, LowDev = 0;
		int i;
		for(i = N; i > 0; i--)
		{
			var LinVal = Intercept + Slope * (N - i);
			HighDev = max(HighDev, Close[i] - LinVal);
			LowDev = min(LowDev, Close[i] - LinVal);
		}

		var RegLine = Intercept + Slope * N;
		var ChannelWidth = HighDev - LowDev;
		var EntryLow  = RegLine + LowDev  + Factor * (HighDev + LowDev);
		var EntryHigh = RegLine + HighDev - Factor * (HighDev + LowDev);

		// Session filter (from CSV)
		int hr = hour();
		int isRollover = (hr >= 21 && hr <= 22);
		int sessionOK = (hr >= assetSessStart[aIdx] && hr <= assetSessEnd[aIdx]);
		if(isRollover) sessionOK = 0;

		// Features for ML
		var ATR_Pct = 0;
		if(price > 0) ATR_Pct = h4atr / price * 100;
		var Range_Pct = 0;
		if(price > 0) Range_Pct = (priceHigh() - priceLow()) / price * 100;
		var Volatility = StdDev(Close, 20);
		if(Volatility != Volatility) Volatility = 0;
		var Trend_Bias = 0;
		if(SMA50[0] > 0) Trend_Bias = (SMA20[0] - SMA50[0]) / SMA50[0] * 100;
		var Trend_Quality = Correlation(Close, BarIdx, 20);
		if(Trend_Quality != Trend_Quality) Trend_Quality = 0;
		var Return_20 = 0;
		if(Close[20] > 0) Return_20 = (Close[0] - Close[20]) / Close[20] * 100;
		var BB_Upper = BBands(Close, 20, 2, 2, MAType_SMA);
		var BB_Middle = rRealMiddleBand;
		var BB_Lower = rRealLowerBand;
		var BB_Width = 0;
		if(BB_Middle > 0) BB_Width = (BB_Upper - BB_Lower) / BB_Middle * 100;
		var totalTrades = NumWinTotal + NumLossTotal;
		var WinRate = 0;
		if(totalTrades > 0) WinRate = (var)NumWinTotal / totalTrades;
		var Current_State = (Equity - Capital) / Capital * 100;

		// =========================================
		// ML PARAM PREDICTION (hourly)
		// =========================================
		int callPredict = 0;
		if(!Train && hr != lastPredictHour[aIdx])
		{
			if(is(TRADEMODE)) callPredict = 1;
			if(is(TESTMODE) && hr == 12) callPredict = 1;
		}
		if(callPredict)
		{
			lastPredictHour[aIdx] = hr;

			char postBuf[512];
			sprintf(postBuf,
				"{\"features\":[%.4f,%.4f,%.6f,%.2f,%.4f,%.4f,%.2f,%.4f,%.4f,%.4f,%.4f,%.2f]}",
				ATR_Pct, Range_Pct, Volatility, adx, Trend_Bias,
				Trend_Quality, rsi, hurst, Return_20, BB_Width,
				WinRate, Current_State);

			string resp = http_transfer(ML_URL_PREDICT, postBuf);
			if(resp)
			{
				char* p;
				p = strstr(resp, "smaTF");
				if(p) { p = strchr(p, ':'); if(p) mlSmaTF[aIdx] = clamp((int)atof(p+1), 1, 8); }
				p = strstr(resp, "FastMA");
				if(p) { p = strchr(p, ':'); if(p) mlFastMA[aIdx] = clamp((int)atof(p+1), 10, 40); }
				p = strstr(resp, "SlowMA");
				if(p) { p = strchr(p, ':'); if(p) mlSlowMA[aIdx] = clamp((int)atof(p+1), 40, 100); }
				p = strstr(resp, "smaStop_x10");
				if(p) { p = strchr(p, ':'); if(p) mlSmaStop_x10[aIdx] = clamp((int)atof(p+1), 15, 50); }
				p = strstr(resp, "adxSMA");
				if(p) { p = strchr(p, ':'); if(p) mlAdxSMA[aIdx] = clamp((int)atof(p+1), 15, 40); }
				p = strstr(resp, "mmiSMA");
				if(p) { p = strchr(p, ':'); if(p) mlMmiSMA[aIdx] = clamp((int)atof(p+1), 60, 85); }
				p = strstr(resp, "\"N\"");
				if(p) { p = strchr(p, ':'); if(p) mlN[aIdx] = clamp((int)atof(p+1), 30, 120); }
				p = strstr(resp, "Factor_x100");
				if(p) { p = strchr(p, ':'); if(p) mlFactor_x100[aIdx] = clamp((int)atof(p+1), 10, 40); }
				p = strstr(resp, "chStop_x10");
				if(p) { p = strchr(p, ':'); if(p) mlChStop_x10[aIdx] = clamp((int)atof(p+1), 15, 50); }
				p = strstr(resp, "lifeTime");
				if(p) { p = strchr(p, ':'); if(p) mlLifeTime[aIdx] = clamp((int)atof(p+1), 10, 40); }
				p = strstr(resp, "adxCH");
				if(p) { p = strchr(p, ':'); if(p) mlAdxCH[aIdx] = clamp((int)atof(p+1), 15, 40); }
				p = strstr(resp, "mmiCH");
				if(p) { p = strchr(p, ':'); if(p) mlMmiCH[aIdx] = clamp((int)atof(p+1), 60, 85); }
				mlReady[aIdx] = 1;
				if(Bar % 2000 == 0)
					printf("\n[ML] %s predict: TF=%d SMA=%d/%d N=%d F=%d adxS=%d mmiS=%d adxC=%d mmiC=%d",
						assetCode, mlSmaTF[aIdx], mlFastMA[aIdx], mlSlowMA[aIdx],
						mlN[aIdx], mlFactor_x100[aIdx],
						mlAdxSMA[aIdx], mlMmiSMA[aIdx], mlAdxCH[aIdx], mlMmiCH[aIdx]);
			}
			else
			{
				if(Bar % 2000 == 0)
					printf("\n[ML] %s predict: NO RESPONSE", assetCode);
			}
		}

		// Skip bar
		int skipBar = (price == 0 || h4atr < PIP * 5 || ChannelWidth < PIP * 3 || !sessionOK);
		if(skipBar) continue;
		if(ftmoStopped) continue;

		// =========================================
		// ALGO VOTE (Auto mode = 4)
		// =========================================
		int effectiveAlgo = algoMode;
		if(algoMode == 4 && !Train)
		{
			int callVote = 0;
			if(hr != lastVoteHour[aIdx])
			{
				if(is(TRADEMODE)) callVote = 1;
				if(is(TESTMODE)) callVote = 1;
			}
			if(callVote)
			{
				lastVoteHour[aIdx] = hr;
				char voteBuf[512];
				sprintf(voteBuf,
					"{\"features\":[%.4f,%.4f,%.6f,%.2f,%.4f,%.4f,%.2f,%.4f,%.4f,%.4f,%.4f,%.2f]}",
					ATR_Pct, Range_Pct, Volatility, adx, Trend_Bias,
					Trend_Quality, rsi, hurst, Return_20, BB_Width,
					WinRate, Current_State);
				string voteResp = http_transfer(ML_URL_VOTE, voteBuf);
				if(voteResp)
				{
					char* vp = strstr(voteResp, "algo_mode");
					if(vp) { vp = strchr(vp, ':'); if(vp) mlAlgoVote[aIdx] = clamp((int)atof(vp+1), 0, 3); }
					if(Bar % 2000 == 0)
						printf("\n[VOTE] %s algo=%d", assetCode, mlAlgoVote[aIdx]);
				}
			}
			effectiveAlgo = mlAlgoVote[aIdx];
			if(effectiveAlgo == 0) { continue; }
		}

		// =========================================
		// FCFS CHECK
		// =========================================
		algo("SMA");
		smaHasOpen[aIdx] = (NumOpenLong + NumOpenShort > 0);
		algo("CH");
		chHasOpen[aIdx] = (NumOpenLong + NumOpenShort > 0);

		int smaCanOpen = !chHasOpen[aIdx];
		int chCanOpen = !smaHasOpen[aIdx];

		// =========================================
		// SIGNALS
		// =========================================
		int smaRegime = (adx > adxSMA && mmiVal < mmiSMA);
		int smaLongSig  = crossOver(SMA_F, SMA_S);
		int smaShortSig = crossUnder(SMA_F, SMA_S);
		int smaTrendL = (SMA_F[0] > SMA_S[0]);
		int smaTrendS = (SMA_F[0] < SMA_S[0]);
		int smaEnabled = (effectiveAlgo == 1 || effectiveAlgo == 2);
		int chEnabled  = (effectiveAlgo == 1 || effectiveAlgo == 3);
		int smaOK_L = (smaEnabled && smaLongSig && rsi > 45 && rsi < 70 && smaRegime && smaCanOpen);
		int smaOK_S = (smaEnabled && smaShortSig && rsi < 55 && rsi > 30 && smaRegime && smaCanOpen);
		int addCooldown = 12;
		int smaAddL = (smaEnabled && smaTrendL && smaRegime && layersLong[aIdx] > 0
			&& layersLong[aIdx] < MAX_LAYERS && (Bar - lastLayerBarL[aIdx]) >= addCooldown);
		int smaAddS = (smaEnabled && smaTrendS && smaRegime && layersShort[aIdx] > 0
			&& layersShort[aIdx] < MAX_LAYERS && (Bar - lastLayerBarS[aIdx]) >= addCooldown);

		int chRegime = (adx > adxCH && mmiVal < mmiCH);
		// Channel extreme entry (original)
		int channelLong  = (price < EntryLow);
		int channelShort = (price > EntryHigh);
		// RegLine crossover entry (CH-REVERSE as entry)
		int regCrossUp   = (Close[1] < RegLine && Close[0] > RegLine);
		int regCrossDown = (Close[1] > RegLine && Close[0] < RegLine);
		// TEST: csak channel extreme entry, RegLine crossover kikapcs
		int chOK_L = (chEnabled && channelLong && chRegime && chCanOpen);
		int chOK_S = (chEnabled && channelShort && chRegime && chCanOpen);

		// DEBUG every 500 bars
		if(Bar % 500 == 0 && aIdx == 0)
		{
			printf("\n[DBG] smaXL=%d smaXS=%d chL=%d chS=%d smaReg=%d chReg=%d",
				smaLongSig, smaShortSig, channelLong, channelShort, smaRegime, chRegime);
			printf(" adx=%.0f adxSMA=%d adxCH=%d mmi=%.0f mmiSMA=%d mmiCH=%d rsi=%.0f",
				adx, adxSMA, adxCH, mmiVal, mmiSMA, mmiCH, rsi);
		}

		// =========================================
		// SMA EXIT: opposite crossover
		// =========================================
		algo("SMA");
		if(smaShortSig && NumOpenLong > 0)
		{
			exitLong();
			layersLong[aIdx] = 0;
		}
		if(smaLongSig && NumOpenShort > 0)
		{
			exitShort();
			layersShort[aIdx] = 0;
		}

		// =========================================
		// CH EXIT: decision tree + session end
		// =========================================
		algo("CH");
		int sessionEnd = (hr >= assetSessEnd[aIdx]);

		// CH LONG exit decision
		if(NumOpenLong > 0 && !sessionEnd)
		{
			int breakout = (price > EntryHigh);
			var distToTarget = (EntryHigh - price) / PIP;
			int atMiddle = (abs(price - RegLine) < 5 * PIP);
			int momentumOK = (Close[0] > Close[1]);

			if(breakout)
			{
				// Kitörés felfelé → tartjuk (trend lehetőség)
			}
			else if(atMiddle && !momentumOK && distToTarget <= 10)
			{
				printf("\n[CH-EXIT] %s LONG: middle+weak+no room (dist=%.0f)", assetCode, distToTarget);
				exitLong();
			}
		}

		// CH SHORT exit decision
		if(NumOpenShort > 0 && !sessionEnd)
		{
			int breakout = (price < EntryLow);
			var distToTarget = (price - EntryLow) / PIP;
			int atMiddle = (abs(price - RegLine) < 5 * PIP);
			int momentumOK = (Close[0] < Close[1]);

			if(breakout)
			{
				// Kitörés lefelé → tartjuk (trend lehetőség)
			}
			else if(atMiddle && !momentumOK && distToTarget <= 10)
			{
				printf("\n[CH-EXIT] %s SHORT: middle+weak+no room (dist=%.0f)", assetCode, distToTarget);
				exitShort();
			}
		}

		// Session end: close remaining CH trades
		if(sessionEnd)
		{
			exitLong();
			exitShort();
		}

		// =========================================
		// SMA PARTIAL CLOSE: LIFO (largest lot first)
		// =========================================
		algo("SMA");
		algo("SMA");
		var partialPip = 30;
		if(layersLong[aIdx] > 1)
		{
			for(current_trades)
			{
				var tradePips = (price - TradePriceOpen) / PIP;
				if(TradeIsLong && TradeLots >= layersLong[aIdx]
					&& tradePips > partialPip)
				{
					printf("\n[PARTIAL] SMA LONG %s close layer=%d lots=%d pips=%.1f",
						assetCode, layersLong[aIdx], (int)TradeLots, tradePips);
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
				var tradePips = (TradePriceOpen - price) / PIP;
				if(TradeIsShort && TradeLots >= layersShort[aIdx]
					&& tradePips > partialPip)
				{
					printf("\n[PARTIAL] SMA SHORT %s close layer=%d lots=%d pips=%.1f",
						assetCode, layersShort[aIdx], (int)TradeLots, tradePips);
					exitTrade(ThisTrade);
					layersShort[aIdx] = layersShort[aIdx] - 1;
					break;
				}
			}
		}

		// =========================================
		// SMA FILTER (ML)
		// =========================================
		int smaFilterL = 1;
		int smaFilterS = 1;
		if(!Train && (smaOK_L || smaOK_S) && (is(TRADEMODE) || is(TESTMODE)))
		{
			int filterDir = -1;
			if(smaOK_L) filterDir = 1;
			char filterBuf[512];
			sprintf(filterBuf,
				"{\"features\":[%.4f,%.4f,%.6f,%.2f,%.4f,%.4f,%.2f,%.4f,%.4f,%.4f,%.4f,%.2f],\"direction\":%d,\"entry_type\":\"SMA\"}",
				ATR_Pct, Range_Pct, Volatility, adx, Trend_Bias,
				Trend_Quality, rsi, hurst, Return_20, BB_Width,
				WinRate, Current_State,
				filterDir);
			string filterResp = http_transfer(ML_URL_FILTER, filterBuf);
			if(filterResp)
			{
				if(strstr(filterResp, "SKIP"))
				{
					if(smaOK_L) smaFilterL = 0;
					if(smaOK_S) smaFilterS = 0;
					printf("\n[FILTER] SMA %s SKIP dir=%d", assetCode, filterDir);
				}
				else
				{
					printf("\n[FILTER] SMA %s GO dir=%d", assetCode, filterDir);
				}
			}
			else
			{
				printf("\n[FILTER] SMA %s NO RESPONSE", assetCode);
			}
		}

		// =========================================
		// TREND-REVERSE LAYER CLOSE (simple, no server)
		// =========================================
		algo("SMA");
		if(layersLong[aIdx] > 0 && smaTrendS)
		{
			printf("\n[TREND-CLOSE] SMA LONG %s trend reversed, layers=%d", assetCode, layersLong[aIdx]);
			exitLong();
			layersLong[aIdx] = 0;
		}
		if(layersShort[aIdx] > 0 && smaTrendL)
		{
			printf("\n[TREND-CLOSE] SMA SHORT %s trend reversed, layers=%d", assetCode, layersShort[aIdx]);
			exitShort();
			layersShort[aIdx] = 0;
		}

		// =========================================
		// SMA ENTRY (pyramid, no LifeTime)
		// =========================================
		algo("SMA");
		if(NumOpenLong == 0) layersLong[aIdx] = 0;
		if(NumOpenShort == 0) layersShort[aIdx] = 0;

		var baseMargin = Equity * 0.5 / 100.0 / NUM_ASSETS;

		if(smaOK_L && smaFilterL && layersLong[aIdx] == 0
			&& (Bar - lastLayerBarL[aIdx]) >= addCooldown)
		{
			Margin = baseMargin;
			Stop = h4atr * smaStop;

			LifeTime = 0;
			enterLong();
			layersLong[aIdx] = 1;
			lastLayerBarL[aIdx] = Bar;
			printf("\n[ENTRY] SMA LONG %s @ %.5f layer=1", assetCode, price);
		}
		else if(smaAddL)
		{
			Margin = baseMargin * (layersLong[aIdx] + 1);
			Stop = h4atr * smaStop;

			LifeTime = 0;
			enterLong();
			layersLong[aIdx] = layersLong[aIdx] + 1;
			lastLayerBarL[aIdx] = Bar;
			printf("\n[ADD] SMA LONG %s @ %.5f layer=%d", assetCode, price, layersLong[aIdx]);
		}

		if(smaOK_S && smaFilterS && layersShort[aIdx] == 0
			&& (Bar - lastLayerBarS[aIdx]) >= addCooldown)
		{
			Margin = baseMargin;
			Stop = h4atr * smaStop;

			LifeTime = 0;
			enterShort();
			layersShort[aIdx] = 1;
			lastLayerBarS[aIdx] = Bar;
			printf("\n[ENTRY] SMA SHORT %s @ %.5f layer=1", assetCode, price);
		}
		else if(smaAddS)
		{
			Margin = baseMargin * (layersShort[aIdx] + 1);
			Stop = h4atr * smaStop;

			LifeTime = 0;
			enterShort();
			layersShort[aIdx] = layersShort[aIdx] + 1;
			lastLayerBarS[aIdx] = Bar;
			printf("\n[ADD] SMA SHORT %s @ %.5f layer=%d", assetCode, price, layersShort[aIdx]);
		}

		// =========================================
		// CH FILTER (ML)
		// =========================================
		int chFilterL = 1;
		int chFilterS = 1;
		if(!Train && (chOK_L || chOK_S) && (is(TRADEMODE) || is(TESTMODE))
			&& hr != lastChFilterHour[aIdx])
		{
			lastChFilterHour[aIdx] = hr;
			int chDir = -1;
			if(chOK_L) chDir = 1;
			char chFilterBuf[512];
			sprintf(chFilterBuf,
				"{\"features\":[%.4f,%.4f,%.6f,%.2f,%.4f,%.4f,%.2f,%.4f,%.4f,%.4f,%.4f,%.2f],\"direction\":%d,\"entry_type\":\"CH\"}",
				ATR_Pct, Range_Pct, Volatility, adx, Trend_Bias,
				Trend_Quality, rsi, hurst, Return_20, BB_Width,
				WinRate, Current_State,
				chDir);
			string chResp = http_transfer(ML_URL_FILTER, chFilterBuf);
			if(chResp)
			{
				if(strstr(chResp, "SKIP"))
				{
					if(chOK_L) chFilterL = 0;
					if(chOK_S) chFilterS = 0;
					printf("\n[FILTER] CH %s SKIP dir=%d", assetCode, chDir);
				}
				else
				{
					printf("\n[FILTER] CH %s GO dir=%d", assetCode, chDir);
				}
			}
			else
			{
				printf("\n[FILTER] CH %s NO RESPONSE", assetCode);
			}
		}

		// =========================================
		// CH ENTRY (fix lot, LifeTime, no overnight)
		// =========================================
		algo("CH");
		if(chOK_L && chFilterL && NumOpenLong < 2 && !sessionEnd)
		{
			Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
			Stop = h4atr * chStop;
			LifeTime = 0;
			enterLong();
			printf("\n[ENTRY] CH LONG %s @ %.5f", assetCode, price);
		}

		if(chOK_S && chFilterS && NumOpenShort < 2 && !sessionEnd)
		{
			Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
			Stop = h4atr * chStop;
			LifeTime = 0;
			enterShort();
			printf("\n[ENTRY] CH SHORT %s @ %.5f", assetCode, price);
		}

		// =========================================
		// PLOTS (EUR/USD)
		// =========================================
		if(!is(LOOKBACK) && aIdx == 0)
		{
			plot("RegLine",  RegLine,   LINE, GREY);
			plot("EntryHi",  EntryHigh, LINE|DOT, RED);
			plot("EntryLo",  EntryLow,  LINE|DOT, GREEN);
			plot("SMA_Fast", SMA_F[0],  LINE, CYAN);
			plot("SMA_Slow", SMA_S[0],  LINE, YELLOW);
		}

		if(Bar % 2000 == 0 && aIdx == 0)
		{
			printf("\n[%s] ML=%s smaTF=%d SMA=%d/%d N=%d F=%.2f",
				assetCode, ifelse(mlReady[aIdx], "ON", "OFF"),
				smaTF, FastMA, SlowMA, N, Factor);
			printf(" smaStop=%.1f chStop=%.1f Life=%d FCFS: SMA=%d CH=%d",
				smaStop, chStop, lifeTime, smaHasOpen[aIdx], chHasOpen[aIdx]);
		}

	} // end asset loop

	if(is(EXITRUN))
	{
		printf("\n\n=== ML-DRIVEN v7: algo() + FCFS + Asset CSV + Smart Exit ===");
		printf("\nAssets: %d active (from MLDRIVEN_Assets.csv)", numActiveAssets);
		printf("\nSMA: trend-following, pyramid, crossover exit, overnight OK");
		printf("\nCH:  mean-reversion, fix lot, session exit, NO overnight");
		printf("\nFCFS: one algo active, other waits");
		printf("\nML Server: %s", ML_URL_PREDICT);
	}
}
