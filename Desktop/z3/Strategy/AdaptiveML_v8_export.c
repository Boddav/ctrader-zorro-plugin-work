// ===================================================================
// AdaptiveML v8 DATA EXPORT — CSV export for XGBoost training
// ===================================================================
// 10 features (SAME as PERCEPTRON ML in AdaptiveML_v8.c)
//
// HASZNÁLAT:
//   1. Zorro: válaszd ki "AdaptiveML_v8_export" → kattints [Test]
//   2. Eredmény: Data/xgb_EURUSD.csv, xgb_GBPUSD.csv, stb.
//   3. Utána: python train_xgb_v8.py
// ===================================================================

#define NUM_ASSETS 7

function run()
{
	set(PLOTNOW|PARAMETERS);  // PARAMETERS: betölti a v8-ból másolt .par fájlokat

	BarPeriod = 15;
	LookBack = 300;
	StartDate = 20230102;
	EndDate = 20260301;

	Capital = 72000;
	Leverage = 500;
	Hedge = 2;

	// WFO — MUST match AdaptiveML_v8.c!
	NumWFOCycles = -8;
	DataSplit = 80;

	// === CSV HEADER (egyszer, Bar==0) ===
	if(Bar == 0)
	{
		string codes[7];
		codes[0]="EURUSD"; codes[1]="GBPUSD"; codes[2]="USDJPY";
		codes[3]="USDCAD"; codes[4]="XAUUSD"; codes[5]="AUDUSD";
		codes[6]="EURCHF";
		int a;
		for(a = 0; a < 7; a++)
		{
			string fp = strf("Data\\xgb_%s.csv", codes[a]);
			file_delete(fp);
			file_append(fp, "chanPos,slope,widthATR,rsi,adx,chg1,chg4,hurst,fracDim,spearman,price,signal\n");
		}
		printf("\n[EXPORT] CSV files initialized (7 assets, 10 features)");
	}

	// === MULTI-ASSET ===
	while(asset(loop(
		"EUR/USD", "GBP/USD", "USD/JPY",
		"USD/CAD", "XAU/USD",
		"AUD/USD", "EUR/CHF")))
	{
		// Per-asset algo() — MUST match v8 for per-asset .par loading
		string assetCode = "UNKNOWN";
		if(strstr(Asset, "EUR/USD"))      { algo("EU"); assetCode = "EURUSD"; }
		else if(strstr(Asset, "GBP/USD")) { algo("GB"); assetCode = "GBPUSD"; }
		else if(strstr(Asset, "USD/JPY")) { algo("JP"); assetCode = "USDJPY"; }
		else if(strstr(Asset, "USD/CAD")) { algo("CA"); assetCode = "USDCAD"; }
		else if(strstr(Asset, "XAU/USD")) { algo("XU"); assetCode = "XAUUSD"; }
		else if(strstr(Asset, "AUD/USD")) { algo("AU"); assetCode = "AUDUSD"; }
		else if(strstr(Asset, "EUR/CHF")) { algo("EC"); assetCode = "EURCHF"; }

		// M15 indicators
		vars Close = series(priceClose());
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

		// MMI
		vars MMI_Raw = series(MMI(Close, 200));
		vars MMI_Smooth = series(SMA(MMI_Raw, 50));
		var mmiVal = MMI_Smooth[0];
		if(mmiVal != mmiVal) mmiVal = 75;

		// === optimize() — SAME ORDER as AdaptiveML_v8.c (.par compatibility) ===
		var adxThresh = optimize(25, 15, 40, 1);   // #1 (unused in export)
		var mmiThresh = optimize(75, 60, 85, 1);   // #2 (unused in export)

		// LinReg Channel
		int N = optimize(40, 20, 80, 5);            // #3
		var Slope = LinearRegSlope(Close, N);
		var Intercept = LinearRegIntercept(Close, N);
		if(Slope != Slope) Slope = 0;
		if(Intercept != Intercept) Intercept = Close[0];

		var HighDev = 0, LowDev = 0;
		int i;
		var LinVal;
		for(i = N; i > 0; i--)
		{
			LinVal = Intercept + Slope * (N - i);
			HighDev = max(HighDev, Close[i] - LinVal);
			LowDev = min(LowDev, Close[i] - LinVal);
		}

		var Factor = optimize(0.2, 0.05, 0.5, 0.05);   // #4
		var RegLine = Intercept + Slope * N;
		var ChannelWidth = HighDev - LowDev;
		var EntryLow  = RegLine + LowDev  + Factor * (HighDev + LowDev);
		var EntryHigh = RegLine + HighDev - Factor * (HighDev + LowDev);

		var chanPos = 50;
		if(ChannelWidth > 0)
			chanPos = 100.0 * (price - (RegLine + LowDev)) / ChannelWidth;

		// Session filter
		int hr = hour();
		int sessionOK = 0;
		if(strstr(Asset, "EUR/USD"))      sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "GBP/USD")) sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "USD/JPY")) sessionOK = (hr >= 1 && hr <= 16);
		else if(strstr(Asset, "USD/CAD")) sessionOK = (hr >= 12 && hr <= 20);
		else if(strstr(Asset, "XAU/USD")) sessionOK = (hr >= 7 && hr <= 20);
		else if(strstr(Asset, "AUD/USD")) sessionOK = (hr >= 1 && hr <= 16);
		else if(strstr(Asset, "EUR/CHF")) sessionOK = (hr >= 7 && hr <= 20);
		else sessionOK = 1;

		// optimize() #5, #6 — MUST match v8's param order (not used in export)
		var stopMult = optimize(3.0, 1.5, 5.0, 0.5); // #5
		int lifeTime = optimize(20, 10, 40, 5);       // #6

		// Skip
		if(price == 0 || h1atr < PIP * 5 || ChannelWidth < PIP * 3 || !sessionOK)
			continue;

		// Features
		var chg1 = 0, chg4 = 0;
		if(price > 0)
		{
			chg1 = (priceClose(0) - priceClose(1)) / price;
			chg4 = (priceClose(0) - priceClose(4)) / price;
		}
		if(chg1 != chg1) chg1 = 0;
		if(chg4 != chg4) chg4 = 0;
		if(chanPos != chanPos) chanPos = 50;

		var s0 = clamp((chanPos - 50) / 50.0, -1, 1);
		var s1 = clamp(Slope / (PIP + 0.00001), -1, 1);
		var s2 = clamp(ChannelWidth / (h1atr + 0.00001) / 10.0, -1, 1);
		var s3 = (rsi - 50) / 50.0;
		var s4 = adx / 50.0 - 1.0;
		var s5 = clamp(chg1 * 100, -1, 1);
		var s6 = clamp(chg4 * 100, -1, 1);
		var s7 = clamp((hurst - 0.5) * 4.0, -1, 1);
		var s8 = clamp((fracDim - 1.5) * 4.0, -1, 1);
		var s9 = clamp(spearman, -1, 1);

		// Channel signals
		int channelLong  = (price < EntryLow);
		int channelShort = (price > EntryHigh);

		// === CSV EXPORT — channel signal barokon ===
		if(channelLong || channelShort)
		{
			int sig = -1;
			if(channelLong) sig = 1;

			// strf EGYSZER — path literal (strf single buffer safe)
			string csvLine = strf("%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.6f,%d\n",
				s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, price, sig);

			if(strstr(assetCode, "EURUSD"))      file_append("Data\\xgb_EURUSD.csv", csvLine);
			else if(strstr(assetCode, "GBPUSD")) file_append("Data\\xgb_GBPUSD.csv", csvLine);
			else if(strstr(assetCode, "USDJPY")) file_append("Data\\xgb_USDJPY.csv", csvLine);
			else if(strstr(assetCode, "USDCAD")) file_append("Data\\xgb_USDCAD.csv", csvLine);
			else if(strstr(assetCode, "XAUUSD")) file_append("Data\\xgb_XAUUSD.csv", csvLine);
			else if(strstr(assetCode, "AUDUSD")) file_append("Data\\xgb_AUDUSD.csv", csvLine);
			else if(strstr(assetCode, "EURCHF")) file_append("Data\\xgb_EURCHF.csv", csvLine);
		}
	}

	// Progress
	if(is(LOOKBACK)) return;
	if(Bar % 50000 == 0)
		printf("\n[EXPORT] Bar %d", Bar);
	if(is(EXITRUN))
	{
		printf("\n\n[EXPORT] CSV export kész!\n");
		printf("\n[AUTO] XGBoost train + server indítás...\n");
		exec("cmd", "/c Strategy\\train_and_serve.bat", 1);
		printf("\n[AUTO] Batch elindítva — nézd a cmd ablakot!\n");
	}
}
