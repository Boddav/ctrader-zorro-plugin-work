// ===================================================================
// CHANNEL CLAUDE ML v1 - LinReg Channel + PERCEPTRON ML Filter
// ===================================================================
// Alap: Financial Hacker "Trading the Channel" (PF 2.8-7.0)
// ML: PERCEPTRON filter — channel features-ből tanulja a nyerő trade-et
// WFO: anchored walk-forward
// BEST RESULT: PF 2.57, SR 2.96, 547 trades (2 év adat)
// ===================================================================

function run()
{
	// === TWO-STEP TRAINING ===
	// Slider 3: 1=Step1 (RULES only), 2=Step2 (PARAMS only)
	// Step 1: [Train] → PERCEPTRON → _ml.c (minden asset-re)
	// Step 2: [Train] → optimize() → .par (minden asset-re)
	// [Test]: mindkettőt betölti
	int trainStep = slider(3, 1, 1, 2, "Step", "1=Rules 2=Params");
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
	StartDate = 20240101;
	EndDate = 20260301;

	Capital = 2000;
	Leverage = 500;
	Hedge = 2;
	EndWeek = 52200;
	StopFactor = 1.5;

	// WFO
	NumWFOCycles = -6;

	if(Train) Hedge = 2;
	if(!Train) MaxLong = MaxShort = 1;

	if(Bar == 0 && is(TRADEMODE))
		brokerTrades(0);

	// SLIDERS
	var MarginPct = slider(1, 15, 1, 50, "Margin", "Equity % margin per trade");
	var MLThresh  = slider(2, 0, -50, 50, "ML Thr", "ML prediction threshold");

	while(asset(loop(
		"EUR/USD", "GBP/USD", "USD/JPY", "AUD/USD",
		"USD/CAD")))
	{
		// Per-asset ML model
		if(strstr(Asset, "EUR/USD")) algo("EU");
		else if(strstr(Asset, "GBP/USD")) algo("GB");
		else if(strstr(Asset, "USD/JPY")) algo("JP");
		else if(strstr(Asset, "AUD/USD")) algo("AU");
		else if(strstr(Asset, "USD/CAD")) algo("CA");

		// === ALL SERIES/INDICATORS BEFORE ANY SKIP ===
		vars Close = series(priceClose());
		var rsi = RSI(Close, 14);
		if(rsi != rsi) rsi = 50;
		var adx = ADX(14);
		if(adx != adx) adx = 25;

		// H1 ATR
		TimeFrame = 4;
		vars H1Close = series(priceClose());
		var h1atr = ATR(14);
		TimeFrame = 1;

		// MMI
		vars MMI_Raw = series(MMI(Close, 100));
		vars MMI_Smooth = series(SMA(MMI_Raw, 50));

		// Linear regression
		int N = optimize(40, 20, 80, 5);
		var Slope = LinearRegSlope(Close, N);
		var Intercept = LinearRegIntercept(Close, N);
		if(Slope != Slope) Slope = 0;
		if(Intercept != Intercept) Intercept = Close[0];

		// Slope series
		vars SlopeS = series(Slope);

		// HH/LL
		var hh4 = HH(4);
		var ll4 = LL(4);

		var price = priceClose();

		// === TRADE PARAMS ===
		var stopMult = optimize(3.0, 1.5, 5.0, 0.5);
		Stop = h1atr * stopMult;
		LifeTime = optimize(20, 10, 40, 5);
		Margin = Equity * MarginPct / 100.0;

		// === CHANNEL SZÁMÍTÁS ===
		var Factor = optimize(0.2, 0.05, 0.5, 0.05);
		var LinVal;
		var HighDev = 0, LowDev = 0;
		int i;
		for(i = N; i > 0; i--)
		{
			LinVal = Intercept + Slope * (N - i);
			HighDev = max(HighDev, Close[i] - LinVal);
			LowDev = min(LowDev, Close[i] - LinVal);
		}

		var RegLine = Intercept + Slope * N;
		var ChannelWidth = HighDev - LowDev;
		var EntryLow = RegLine + LowDev + Factor * (HighDev + LowDev);
		var EntryHigh = RegLine + HighDev - Factor * (HighDev + LowDev);

		var chanPos = 0;
		if(ChannelWidth > 0)
			chanPos = 100.0 * (price - (RegLine + LowDev)) / ChannelWidth;

		// === SKIP FLAG — CHANNEL UTÁN (h1atr + ChannelWidth kell) ===
		int skipBar = (price == 0 || h1atr < PIP * 5 || ChannelWidth < PIP * 3);

		// === SESSION FILTER ===
		int hour = hour();
		int sessionOK = 0;
		if(strstr(Asset, "EUR/USD"))
			sessionOK = (hour >= 7 && hour <= 21);
		else if(strstr(Asset, "GBP/USD"))
			sessionOK = (hour >= 7 && hour <= 21);
		else if(strstr(Asset, "USD/JPY"))
			sessionOK = (hour >= 0 && hour <= 16);
		else if(strstr(Asset, "AUD/USD"))
			sessionOK = (hour >= 22 || hour <= 8);
		else if(strstr(Asset, "USD/CAD"))
			sessionOK = (hour >= 13 && hour <= 21);
		else
			sessionOK = 1;

		// === ML FEATURES (8 darab) ===
		var chg1 = 0, chg4 = 0, rng4 = 0;
		if(price > 0)
		{
			chg1 = (priceClose(0) - priceClose(1)) / price;
			chg4 = (priceClose(0) - priceClose(4)) / price;
			rng4 = (hh4 - ll4) / price;
		}
		if(chg1 != chg1) chg1 = 0;
		if(chg4 != chg4) chg4 = 0;
		if(rng4 != rng4) rng4 = 0;
		if(chanPos != chanPos) chanPos = 50;

		var Sigs[8];
		Sigs[0] = clamp((chanPos - 50) / 50.0, -1, 1);     // channel pozíció
		Sigs[1] = clamp(Slope / (PIP + 0.00001), -1, 1);    // slope
		Sigs[2] = clamp(ChannelWidth / (h1atr + 0.00001) / 10.0, -1, 1); // width vs ATR
		Sigs[3] = (rsi - 50) / 50.0;                         // RSI
		Sigs[4] = adx / 50.0 - 1.0;                          // ADX (-1..+1)
		Sigs[5] = clamp(chg1 * 100, -1, 1);                  // 1-bar change
		Sigs[6] = clamp(chg4 * 100, -1, 1);                  // 4-bar change
		Sigs[7] = clamp(rng4 * 100, -1, 1);                  // 4-bar range

		// === ML PREDICTION (mindig hívni — series konzisztencia) ===
		var mlLong = adviseLong(PERCEPTRON+BALANCED+RETURNS, 0, Sigs, 8);
		var mlShort = adviseShort();

		// === SKIP: most már biztonságos ===
		if(skipBar || !sessionOK) continue;

		// === CHANNEL ENTRY JELEK ===
		int channelLong = (price < EntryLow);
		int channelShort = (price > EntryHigh);

		// === DEBUG LOG ===
		if(!is(LOOKBACK) && (channelLong || channelShort))
			printf("\n[SIGNAL] %s %s price=%.5f Lo=%.5f Hi=%.5f h1atr=%.5f Stop=%.5f ML_L=%.1f ML_S=%.1f",
				Asset, ifelse(channelLong,"LONG","SHORT"), price, EntryLow, EntryHigh, h1atr, Stop, mlLong, mlShort);

		// === COMBINED ENTRY ===
		if(!Train)
		{
			if(channelLong && mlLong > MLThresh && !NumOpenLong)
				enterLong();
			if(channelShort && mlShort > MLThresh && !NumOpenShort)
				enterShort();

			// EXIT: ellentétes channel szél
			if(price > EntryHigh && NumOpenLong)
				exitLong();
			if(price < EntryLow && NumOpenShort)
				exitShort();
		}

		// Train: minden channel jelet trade-elünk
		if(Train)
		{
			if(channelLong) enterLong();
			if(channelShort) enterShort();
		}

		// === PLOTS ===
		if(!is(LOOKBACK) && strstr(Asset, "EUR/USD"))
		{
			plot("RegLine", RegLine, LINE, GREY);
			plot("EntryHi", EntryHigh, LINE|DOT, RED);
			plot("EntryLo", EntryLow, LINE|DOT, GREEN);
			plot("ChanPos", chanPos, NEW, BLUE);
			plot("ML_L", mlLong, NEW, GREEN);
			plot("ML_S", mlShort, 0, RED);
		}
	}

	if(is(LOOKBACK)) return;
	if(Bar % 5000 == 0)
		printf("\n[BAR %d] Open=%d Eq=%.0f", Bar, NumOpenTotal, Equity);
	if(is(EXITRUN))
		printf("\n\n=== CHANNEL CLAUDE ML v1: LinReg+PERCEPTRON ===\n");
}
