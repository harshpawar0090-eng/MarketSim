#ifndef MARKETSIM_BACKTEST_STRATEGY_H
#define MARKETSIM_BACKTEST_STRATEGY_H

#include <cstddef>
#include <string>
#include <vector>
#include "../Execution/Order.h"
#include "../Historical/HistoricalData.h"
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"

// Backtest module: a Strategy interface mirroring the existing Simulation
// module's Scenario pattern (abstract base, one virtual decision method,
// concrete subclasses for real behavioral variance) - but where Scenario
// computes a read-only hypothetical, a Strategy expresses real order intent
// that BacktestEngine submits through the normal Broker -> RiskManager ->
// ExecutionService -> MatchingEngine -> OrderBook path.
namespace backtest
{

    // What a Strategy is allowed to see at one replay step. Deliberately
    // read-only and bounded to data available up to and including the
    // current step - this is what makes look-ahead bias structurally
    // impossible rather than merely discouraged: history() cannot return a
    // bar beyond stepIndex because the view is rebuilt fresh every step.
    class MarketView
    {
    public:
        MarketView(const market::Market &market, const portfolio::Portfolio &portfolio,
                   const historical::HistoricalDataSet &data, std::size_t stepIndex);

        const market::Market &market() const { return market_; }
        const portfolio::Portfolio &portfolio() const { return portfolio_; }
        std::size_t stepIndex() const { return stepIndex_; }

        // Bars for symbol from index 0 to stepIndex inclusive (oldest first).
        // Empty if the symbol is not in the dataset. Never includes a bar
        // beyond stepIndex, however many more the dataset actually has.
        std::vector<historical::HistoricalBar> history(const std::string &symbol) const;

    private:
        const market::Market &market_;
        const portfolio::Portfolio &portfolio_;
        const historical::HistoricalDataSet &data_;
        std::size_t stepIndex_;
    };

    // One order a Strategy wants placed this step. A Market order if
    // limitPrice <= 0, otherwise a Limit order at that price. The Strategy
    // never touches Broker/Portfolio directly - BacktestEngine is the only
    // thing that turns an OrderIntent into a real order.
    struct OrderIntent
    {
        std::string symbol;
        execution::OrderSide side;
        int quantity;
        double limitPrice = 0.0;
    };

    class Strategy
    {
    public:
        virtual ~Strategy() = default;
        virtual std::string name() const = 0;

        // Called once per replay step, after that step's prices have been
        // applied to Market. Returns zero or more order intents for this step.
        virtual std::vector<OrderIntent> onStep(const MarketView &view) = 0;
    };

    // Buys a fixed quantity of one symbol the first time it holds none, then
    // does nothing. Useful both as a minimal real strategy and as a
    // benchmark comparable to BacktestResult::benchmarkReturnPercent.
    class BuyAndHoldStrategy : public Strategy
    {
    public:
        BuyAndHoldStrategy(std::string symbol, int quantity);
        std::string name() const override;
        std::vector<OrderIntent> onStep(const MarketView &view) override;

    private:
        std::string symbol_;
        int quantity_;
    };

    // Classic simple-moving-average crossover: buy when the short-window
    // average rises above the long-window average and nothing is held; sell
    // the whole position when it falls back below. Uses only view.history(),
    // so it is a natural fixture for look-ahead-bias testing.
    class MovingAverageCrossStrategy : public Strategy
    {
    public:
        MovingAverageCrossStrategy(std::string symbol, int shortWindow, int longWindow, int quantity);
        std::string name() const override;
        std::vector<OrderIntent> onStep(const MarketView &view) override;

    private:
        std::string symbol_;
        int shortWindow_;
        int longWindow_;
        int quantity_;
    };

} // namespace backtest

#endif // MARKETSIM_BACKTEST_STRATEGY_H