// ===================================================================
// HEDGING CLAUDE v21 - Daily Level MR + RSI + ADX + MMI Regime
// ===================================================================
// Kutatás-alapú stratégia (Financial Hacker + Zorro Workshop):
// 1. Daily High/Low szintek = support/resistance kontextus
// 2. RSI(4) = mean reversion jelzés (oversold/overbought)
// 3. ADX < 25 = range-bound regime filter (MR csak itt működik)
// 4. MMI falling = extra regime megerősítés
// 5. H1 ATR stop/trail + ATR position sizing
// 6. TakeProfit = fél daily range (mid-range target)
// 7. Session filter per pár
// ===================================================================

function run()
{
	set(LOGFILE|PLOTNOW);

	BarPeriod = 15;
	LookBack = 300;
	StartDate = 20251201;
	EndDate = 20260301;

	Capital = 2000;
	Leverage = 500;
	Hedge = 2;
	EndWeek = 52200;
	StopFactor = 1.5;

	if(Bar == 0 && is(TRADEMODE))
		brokerTrades(0);

	// SLIDERS - live trading-ben állítható
	var StopMulti = slider(1, 25, 10, 50, "Stop", "Stop = H1_ATR * érték/10");
	var MarginPct = slider(2, 15, 1, 50, "Margin", "Equity % margin per trade");
	var RSI_OB = 75;   // fix RSI szintek (kevesebb slider kell)
	var RSI_OS = 25;

	while(asset(loop(
		"EUR/USD", "GBP/USD", "USD/JPY", "AUD/USD",
		"USD/CAD")))
	{
		// === INDIKÁTOROK (mind BEFORE any continue) ===
		vars Close = series(priceClose());

		// RSI(4) — rövid periódus, érzékeny MR jelzés
		var rsi = RSI(Close, 4);
		if(rsi != rsi) rsi = 50; // NaN guard

		// ADX(14) — trend erősség; <25 = range-bound (MR terep)
		var adx = ADX(14);

		// H1 ATR — stabilabb stop/trail alap
		TimeFrame = 4;
		vars H1Close = series(priceClose());
		var h1atr = ATR(14);
		TimeFrame = 1;

		// MMI — regime filter; falling = trending, rising = mean-reverting
		vars MMI_Raw = series(MMI(Close, 100));
		vars MMI_Smooth = series(SMA(MMI_Raw, 50));
		int mmiRising = (MMI_Smooth[0] > MMI_Smooth[1]);

		// Tegnapi csúcs/mélypont
		var prevHigh = dayHigh(UTC, 1);
		var prevLow = dayLow(UTC, 1);
		var price = priceClose();

		// Daily range
		var dailyRange = prevHigh - prevLow;
		var minRange = 2.0 * h1atr;

		// === TRADE PARAMS ===
		Stop = h1atr * StopMulti / 10.0;
		Trail = h1atr * 1.5;
		TrailLock = 70;

		// Margin-alapú position sizing
		// Zorro a Margin-ból automatikusan számolja a Lots-ot
		Margin = Equity * MarginPct / 100.0;

		// TakeProfit: fél daily range
		var halfRange = dailyRange / 2.0;
		if(halfRange > 0)
			TakeProfit = halfRange;
		else
			TakeProfit = h1atr * 3.0;

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

		// === SKIP CONDITIONS ===
		if(!sessionOK || prevHigh <= 0 || prevLow <= 0
			|| h1atr <= 0 || dailyRange < minRange)
			continue;

		// === REGIME FILTER ===
		// ADX < 25 = range-bound piac (MR működik)
		// MMI rising = mean-reverting tendencia
		int regimeOK = (adx < 25) && mmiRising;

		// === ENTRY LOGIKA ===
		// Szint közelében + RSI jelzés + regime filter
		var threshold = h1atr * 0.5; // fél ATR közelség a szinthez

		// SHORT: ár a tegnapi csúcs közelében + RSI overbought + regime OK
		if(regimeOK && price >= prevHigh - threshold
			&& rsi > RSI_OB && !NumOpenShort)
		{
			enterShort();
		}

		// LONG: ár a tegnapi mélypont közelében + RSI oversold + regime OK
		if(regimeOK && price <= prevLow + threshold
			&& rsi < RSI_OS && !NumOpenLong)
		{
			enterLong();
		}

		// === EXIT: RSI ellentétes szélsőség ===
		if(NumOpenLong && rsi > RSI_OB)
			exitLong();
		if(NumOpenShort && rsi < RSI_OS)
			exitShort();

		// === PLOTS (EUR/USD) ===
		if(!is(LOOKBACK) && strstr(Asset, "EUR/USD"))
		{
			plot("PrevHigh", prevHigh, LINE, RED);
			plot("PrevLow", prevLow, LINE, GREEN);
			plot("MidRange", (prevHigh + prevLow) / 2.0, LINE|DOT, GREY);
			plot("RSI", rsi, NEW, BLUE);
			plot("OB", RSI_OB, LINE, GREY);
			plot("OS", RSI_OS, LINE, GREY);
			plot("ADX", adx, NEW, ORANGE);
			plot("ADX25", 25, LINE, RED);
			plot("MMI", MMI_Smooth[0], NEW, CYAN);
		}
	}

	if(is(LOOKBACK)) return;
	if(Bar % 5000 == 0)
		printf("\n[BAR %d] Open=%d Eq=%.0f ADX/MMI active", Bar, NumOpenTotal, Equity);
	if(is(EXITRUN))
		printf("\n\n=== HEDGING CLAUDE v21: DailyLevel+RSI4+ADX+MMI ===\n");
}
