#include "../historical/historicaldata.h"
#include "../historical/marketreplay.h"
#include "../backtest/strategy.h"
#include "../backtest/backtestengine.h"
#include "../broker/broker.h"
#include "../execution/executionservice.h"
#include "../portfolio/portfolio.h"
#include "../market/market.h"
#include "../risk/riskmanager.h"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using historical::HistoricalBar;

static int g_checks = 0;
static int g_failures = 0;

static void check(bool ok, const std::string &what, int line)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::cout << "  FAIL (line " << line << "): " << what << "\n";
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

#define CHECK_THROWS(expr, ExType)                              \
    do                                                          \
    {                                                           \
        bool threw = false;                                     \
        try                                                     \
        {                                                       \
            expr;                                               \
        }                                                       \
        catch (const ExType &)                                  \
        {                                                       \
            threw = true;                                       \
        }                                                       \
        catch (...)                                             \
        {                                                       \
        }                                                       \
        check(threw, #expr " should throw " #ExType, __LINE__); \
    } while (0)

static bool near(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) <= eps;
}

static HistoricalBar bar(const std::string &symbol,
                         std::time_t timestamp,
                         double open,
                         double high,
                         double low,
                         double close,
                         long long volume)
{
    return HistoricalBar{symbol, timestamp, open, high, low, close, volume};
}

static historical::HistoricalDataSet makeDataset()
{
    historical::HistoricalDataSet data;

    historical::HistoricalSeries nova("NOVA");
    nova.addBar(bar("NOVA", 1000, 100.0, 102.0, 99.0, 101.0, 1000));
    nova.addBar(bar("NOVA", 2000, 101.0, 111.0, 100.0, 110.0, 1200));
    nova.addBar(bar("NOVA", 3000, 110.0, 112.0, 104.0, 105.0, 1400));
    nova.addBar(bar("NOVA", 4000, 105.0, 108.0, 103.0, 107.0, 1600));

    historical::HistoricalSeries qbit("QBIT");
    qbit.addBar(bar("QBIT", 1000, 200.0, 202.0, 198.0, 201.0, 2000));
    qbit.addBar(bar("QBIT", 2000, 201.0, 206.0, 199.0, 205.0, 2200));
    qbit.addBar(bar("QBIT", 3000, 205.0, 208.0, 203.0, 207.0, 2400));
    qbit.addBar(bar("QBIT", 4000, 207.0, 210.0, 205.0, 209.0, 2600));

    data.addSeries(nova);
    data.addSeries(qbit);
    return data;
}

static void testHistoricalSeriesValidation()
{
    std::cout << "[1] HistoricalSeries validation and indexed access\n";

    historical::HistoricalSeries s("TEST");
    s.addBar(bar("TEST", 1000, 10.0, 11.0, 9.0, 10.5, 100));

    CHECK(s.size() == 1);
    CHECK(!s.empty());
    CHECK(s.symbol() == "TEST");
    CHECK(near(s.at(0).close, 10.5));
    CHECK(s.at(0).volume == 100);

    CHECK_THROWS(
        s.addBar(bar("OTHER", 2000, 10.5, 11.0, 10.0, 10.8, 100)),
        historical::HistoricalDataError);

    CHECK_THROWS(
        s.addBar(bar("TEST", 2000, 10.5, 9.0, 10.0, 10.5, 100)),
        historical::HistoricalDataError);

    CHECK_THROWS(
        s.addBar(bar("TEST", 2000, 10.5, 11.0, 10.0, -1.0, 100)),
        historical::HistoricalDataError);

    CHECK_THROWS(
        s.addBar(bar("TEST", 1000, 10.5, 11.0, 10.0, 10.5, 100)),
        historical::HistoricalDataError);

    CHECK_THROWS(s.at(5), historical::HistoricalDataError);
}

static void testHistoricalDataSet()
{
    std::cout << "[2] HistoricalDataSet multi-symbol and step alignment\n";

    auto data = makeDataset();

    CHECK(data.stepCount() == 4);
    CHECK(data.hasSymbol("NOVA"));
    CHECK(data.hasSymbol("QBIT"));
    CHECK(!data.hasSymbol("NOPE"));

    const auto symbols = data.symbols();
    CHECK(symbols.size() == 2);
    CHECK(data.barAt("NOVA", 1).close == 110.0);
    CHECK(data.barAt("QBIT", 3).close == 209.0);

    CHECK_THROWS(data.series("NOPE"), historical::HistoricalDataError);
    CHECK_THROWS(data.barAt("NOVA", 99), historical::HistoricalDataError);

    historical::HistoricalDataSet bad;
    historical::HistoricalSeries shortSeries("SHORT");
    shortSeries.addBar(bar("SHORT", 1000, 10, 11, 9, 10, 100));
    shortSeries.addBar(bar("SHORT", 2000, 10, 12, 9, 11, 100));
    bad.addSeries(shortSeries);

    historical::HistoricalSeries wrongLength("OTHER");
    wrongLength.addBar(bar("OTHER", 1000, 20, 21, 19, 20, 100));

    CHECK_THROWS(bad.addSeries(wrongLength), historical::HistoricalDataError);
}

static void testCsvPersistence()
{
    std::cout << "[3] Historical CSV save/load round trip\n";

    historical::HistoricalSeries original("CSVTEST");
    original.addBar(bar("CSVTEST", 1000, 10, 12, 9, 11, 500));
    original.addBar(bar("CSVTEST", 2000, 11, 13, 10, 12, 700));

    const std::string path = "phase4_test_history.csv";
    historical::saveHistoricalSeriesCsv(original, path);

    auto loaded = historical::loadHistoricalSeriesCsv("CSVTEST", path);

    CHECK(loaded.symbol() == original.symbol());
    CHECK(loaded.size() == original.size());
    CHECK(loaded.at(0).timestamp == 1000);
    CHECK(near(loaded.at(1).open, 11.0));
    CHECK(near(loaded.at(1).high, 13.0));
    CHECK(near(loaded.at(1).low, 10.0));
    CHECK(near(loaded.at(1).close, 12.0));
    CHECK(loaded.at(1).volume == 700);

    std::remove(path.c_str());

    CHECK_THROWS(
        historical::loadHistoricalSeriesCsv("CSVTEST", "does_not_exist_phase4.csv"),
        historical::HistoricalDataError);
}

static void testSyntheticGeneration()
{
    std::cout << "[4] Deterministic synthetic historical generation\n";

    auto a = historical::generateSyntheticSeries("SYN", 100.0, 8, 1000, 60, 0.01, 42);
    auto b = historical::generateSyntheticSeries("SYN", 100.0, 8, 1000, 60, 0.01, 42);

    CHECK(a.size() == 8);
    CHECK(b.size() == 8);
    CHECK(a.at(0).timestamp == 1000);
    CHECK(a.at(7).timestamp == 1420);

    for (std::size_t i = 0; i < a.size(); ++i)
    {
        CHECK(a.at(i).timestamp == b.at(i).timestamp);
        CHECK(near(a.at(i).close, b.at(i).close));
        CHECK(a.at(i).volume == b.at(i).volume);
        CHECK(a.at(i).high >= a.at(i).low);
    }

    CHECK_THROWS(
        historical::generateSyntheticSeries("BAD", 100.0, 0, 1000),
        historical::HistoricalDataError);
}

static void testMarketHistoricalBar()
{
    std::cout << "[5] Market::applyHistoricalBar exact OHLCV replay\n";

    market::Market market;
    market.addStock(market::Stock("NOVA", "Nova", market::Sector::Technology, 100.0));

    market.applyHistoricalBar("NOVA", 100.0, 105.0, 99.0, 103.0, 5000, 1000);

    const auto &q = market.getQuote("NOVA");
    const auto &h = market.priceHistory("NOVA");

    CHECK(near(q.price(), 103.0));
    CHECK(near(q.previousPrice(), 100.0));
    CHECK(near(q.change(), 3.0));
    CHECK(near(q.percentChange(), 3.0));
    CHECK(q.volume() == 5000);
    CHECK(near(q.turnover(), 103.0 * 5000.0));
    CHECK(q.timestamp() == 1000);

    CHECK(!h.empty());
    const auto &last = h.back();
    CHECK(near(last.open, 100.0));
    CHECK(near(last.high, 105.0));
    CHECK(near(last.low, 99.0));
    CHECK(near(last.close, 103.0));
    CHECK(last.volume == 5000);
    CHECK(last.timestamp == 1000);

    CHECK_THROWS(
        market.applyHistoricalBar("NOPE", 1, 2, 0, 1, 100, 1000),
        std::out_of_range);

    CHECK_THROWS(
        market.applyHistoricalBar("NOVA", 1, 2, 0, -1, 100, 1000),
        std::invalid_argument);
}

static void testMarketReplay()
{
    std::cout << "[6] MarketReplay step progression and historical prices\n";

    auto data = makeDataset();

    market::Market market;
    market.addStock(market::Stock("NOVA", "Nova", market::Sector::Technology, 100.0));
    market.addStock(market::Stock("QBIT", "Qbit", market::Sector::Technology, 200.0));

    portfolio::Portfolio portfolio(100000.0);
    execution::ExecutionService service(market, portfolio);
    broker::Broker broker(service);

    historical::MarketReplay replay(market, broker, data);

    CHECK(replay.stepCount() == 4);
    CHECK(replay.stepsApplied() == 0);
    CHECK(replay.hasNext());

    auto first = replay.step();
    (void)first;
    CHECK(replay.stepsApplied() == 1);
    CHECK(near(market.getQuote("NOVA").price(), 101.0));
    CHECK(near(market.getQuote("QBIT").price(), 201.0));
    CHECK(market.getQuote("NOVA").timestamp() == 1000);

    replay.step();
    CHECK(replay.stepsApplied() == 2);
    CHECK(near(market.getQuote("NOVA").price(), 110.0));
    CHECK(near(market.getQuote("QBIT").price(), 205.0));

    auto remaining = replay.stepMultiple(10);
    CHECK(remaining.size() == 2);
    CHECK(replay.stepsApplied() == 4);
    CHECK(!replay.hasNext());
    CHECK(near(market.getQuote("NOVA").price(), 107.0));

    CHECK_THROWS(replay.step(), std::out_of_range);
}

static void testMarketViewNoLookahead()
{
    std::cout << "[7] MarketView history is bounded to the current step\n";

    auto data = makeDataset();

    market::Market market;
    market.addStock(market::Stock("NOVA", "Nova", market::Sector::Technology, 100.0));
    portfolio::Portfolio portfolio(100000.0);

    backtest::MarketView view(market, portfolio, data, 1);

    auto history = view.history("NOVA");

    CHECK(view.stepIndex() == 1);
    CHECK(history.size() == 2);
    CHECK(history[0].timestamp == 1000);
    CHECK(history[1].timestamp == 2000);
    CHECK(near(history.back().close, 110.0));

    CHECK(view.history("NOPE").empty());

    // Explicit look-ahead check: the third bar exists in the dataset but
    // cannot be observed from step index 1.
    CHECK(history.size() < data.series("NOVA").size());
    CHECK(history.back().timestamp < data.barAt("NOVA", 2).timestamp);
}

class BuyThenSellStrategy final : public backtest::Strategy
{
public:
    std::string name() const override
    {
        return "Phase4 Buy Then Sell";
    }

    std::vector<backtest::OrderIntent> onStep(const backtest::MarketView &view) override
    {
        if (view.stepIndex() == 0)
        {
            return {{"NOVA", execution::OrderSide::Buy, 10, 0.0}};
        }

        if (view.stepIndex() == 1)
        {
            return {{"NOVA", execution::OrderSide::Sell, 10, 0.0}};
        }

        return {};
    }
};

class BuyOnceStrategy final : public backtest::Strategy
{
public:
    std::string name() const override
    {
        return "Phase4 Buy Once";
    }

    std::vector<backtest::OrderIntent> onStep(const backtest::MarketView &view) override
    {
        if (view.stepIndex() == 0)
        {
            return {{"NOVA", execution::OrderSide::Buy, 10, 0.0}};
        }

        return {};
    }
};

static void testStrategyImplementations()
{
    std::cout << "[8] Built-in Strategy implementations\n";

    auto data = makeDataset();

    market::Market market;
    market.addStock(market::Stock("NOVA", "Nova", market::Sector::Technology, 100.0));
    portfolio::Portfolio portfolio(100000.0);

    backtest::MarketView firstView(market, portfolio, data, 0);
    backtest::BuyAndHoldStrategy buyHold("NOVA", 10);

    CHECK(buyHold.name().find("Buy & Hold") != std::string::npos);

    auto firstIntent = buyHold.onStep(firstView);
    CHECK(firstIntent.size() == 1);
    CHECK(firstIntent[0].symbol == "NOVA");
    CHECK(firstIntent[0].side == execution::OrderSide::Buy);
    CHECK(firstIntent[0].quantity == 10);

    portfolio.applyBuyFill("NOVA", 10, 100.0);
    backtest::MarketView heldView(market, portfolio, data, 1);
    CHECK(buyHold.onStep(heldView).empty());

    backtest::MovingAverageCrossStrategy ma("NOVA", 2, 3, 10);
    CHECK(ma.name().find("MA Cross") != std::string::npos);

    // At step 1 there is not enough history for a 3-bar long MA.
    CHECK(ma.onStep(heldView).empty());

    backtest::MarketView step2View(market, portfolio, data, 2);
    auto maIntent = ma.onStep(step2View);
    CHECK(maIntent.empty() || maIntent[0].symbol == "NOVA");
}

static void testBuyAndHoldBacktest()
{
    std::cout << "[9] BacktestEngine + BuyAndHold + benchmark + equity curve\n";

    auto data = makeDataset();

    backtest::BacktestConfig config;
    config.startingCash = 100000.0;
    config.benchmarkSymbol = "NOVA";

    backtest::BacktestEngine engine(data, config);
    backtest::BuyAndHoldStrategy strategy("NOVA", 10);

    auto result = engine.run(strategy);

    CHECK(result.strategyName == "Buy & Hold (NOVA)");
    CHECK(near(result.startingEquity, 100000.0));
    CHECK(result.equityCurve.size() == data.stepCount());
    CHECK(result.equityCurve.size() == 4);
    CHECK(result.equityCurve[0].stepIndex == 0);
    CHECK(result.equityCurve[3].stepIndex == 3);
    CHECK(result.equityCurve[0].timestamp == 1000);
    CHECK(result.equityCurve[3].timestamp == 4000);

    const double expectedBenchmark = (107.0 - 101.0) / 101.0 * 100.0;
    CHECK(near(result.benchmarkReturnPercent, expectedBenchmark));

    CHECK(result.numberOfTrades == 1);
    CHECK(result.trades.size() == 1);
    CHECK(result.trades[0].symbol == "NOVA");
    CHECK(result.trades[0].side == portfolio::TransactionSide::Buy);
    CHECK(result.trades[0].quantity == 10);
    CHECK(result.winningTrades == 0);
    CHECK(result.losingTrades == 0);
    CHECK(near(result.winRate, 0.0));
    CHECK(result.totalTransactionCosts == 0.0);
    CHECK(result.maxDrawdownPercent >= 0.0);
}

static void testFullBacktestMetrics()
{
    std::cout << "[10] Backtest trade execution, P&L, commission and metrics\n";

    auto data = makeDataset();

    backtest::BacktestConfig config;
    config.startingCash = 100000.0;
    config.commissionPerOrder = 5.0;
    config.slippageBps = 0.0;
    config.benchmarkSymbol = "NOVA";

    backtest::BacktestEngine engine(data, config);
    BuyThenSellStrategy strategy;

    auto result = engine.run(strategy);

    CHECK(result.strategyName == "Phase4 Buy Then Sell");
    CHECK(result.equityCurve.size() == 4);
    CHECK(result.numberOfTrades == 2);
    CHECK(result.trades.size() == 2);

    CHECK(result.trades[0].side == portfolio::TransactionSide::Buy);
    CHECK(result.trades[1].side == portfolio::TransactionSide::Sell);
    CHECK(result.trades[0].quantity == 10);
    CHECK(result.trades[1].quantity == 10);

    CHECK(result.winningTrades == 1);
    CHECK(result.losingTrades == 0);
    CHECK(near(result.winRate, 100.0));
    CHECK(result.trades[1].realizedPnL > 0.0);

    CHECK(near(result.totalTransactionCosts, 10.0));
    CHECK(result.endingEquity > result.startingEquity);
    CHECK(result.totalReturnPercent > 0.0);
    CHECK(result.maxDrawdownPercent >= 0.0);
}

static void testBacktestRejectionDoesNotStopRun()
{
    std::cout << "[11] Backtest continues after a rejected order\n";

    auto data = makeDataset();

    backtest::BacktestConfig config;
    config.startingCash = 10.0;

    backtest::BacktestEngine engine(data, config);
    BuyOnceStrategy strategy;

    auto result = engine.run(strategy);

    // The order is rejected for insufficient buying power, but all replay
    // steps still complete and the backtest returns a result.
    CHECK(result.equityCurve.size() == data.stepCount());
    CHECK(result.numberOfTrades == 0);
    CHECK(result.trades.empty());
    CHECK(near(result.startingEquity, 10.0));
    CHECK(near(result.endingEquity, 10.0));
}

static void testBacktestInstrumentMeta()
{
    std::cout << "[12] Backtest instrument metadata and independent state\n";

    auto data = makeDataset();

    backtest::BacktestConfig config;
    config.instrumentMeta["NOVA"] = {"Nova Test Corp", market::Sector::Finance};

    backtest::BacktestEngine engine(data, config);
    backtest::BuyAndHoldStrategy strategy("NOVA", 1);
    auto result = engine.run(strategy);

    CHECK(result.numberOfTrades == 1);
    CHECK(result.equityCurve.size() == data.stepCount());
}

static void testEndToEndPhase4Path()
{
    std::cout << "[13] End-to-end Phase 4 architecture path\n";

    auto data = makeDataset();

    backtest::BacktestConfig config;
    config.startingCash = 50000.0;
    config.commissionPerOrder = 2.0;
    config.benchmarkSymbol = "QBIT";

    backtest::BacktestEngine engine(data, config);
    BuyThenSellStrategy strategy;

    auto result = engine.run(strategy);

    // HistoricalDataSet -> MarketReplay -> Market -> Broker ->
    // ExecutionService -> MatchingEngine/OrderBook -> Portfolio ->
    // BacktestResult.
    CHECK(result.equityCurve.size() == 4);
    CHECK(result.numberOfTrades == 2);
    CHECK(result.trades.size() == 2);
    CHECK(result.equityCurve.front().timestamp == 1000);
    CHECK(result.equityCurve.back().timestamp == 4000);
    CHECK(near(result.benchmarkReturnPercent, (209.0 - 201.0) / 201.0 * 100.0));
    CHECK(near(result.totalTransactionCosts, 4.0));
}

int main()
{
    testHistoricalSeriesValidation();
    testHistoricalDataSet();
    testCsvPersistence();
    testSyntheticGeneration();
    testMarketHistoricalBar();
    testMarketReplay();
    testMarketViewNoLookahead();
    testStrategyImplementations();
    testBuyAndHoldBacktest();
    testFullBacktestMetrics();
    testBacktestRejectionDoesNotStopRun();
    testBacktestInstrumentMeta();
    testEndToEndPhase4Path();

    std::cout << "\n"
              << g_checks << " checks, " << g_failures << " failed\n";
    std::cout << (g_failures == 0 ? "ALL PHASE 4 TESTS PASSED\n"
                                  : "PHASE 4 TESTS FAILED\n");

    return g_failures == 0 ? 0 : 1;
}
