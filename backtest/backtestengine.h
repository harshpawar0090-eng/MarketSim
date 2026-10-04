#ifndef MARKETSIM_BACKTEST_BACKTESTENGINE_H
#define MARKETSIM_BACKTEST_BACKTESTENGINE_H

#include <ctime>
#include <map>
#include <string>
#include <vector>
#include "../Broker/Broker.h"
#include "../Execution/ExecutionService.h"
#include "../Historical/HistoricalData.h"
#include "../Historical/MarketReplay.h"
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"
#include "../Risk/RiskManager.h"
#include "Strategy.h"

// BacktestEngine: runs a Strategy over a HistoricalDataSet. It builds its
// OWN Market + Portfolio + ExecutionService + Broker, seeded from the
// dataset, so a backtest never touches the live app's objects and never
// needs a second trading/matching/portfolio implementation - every order
// the strategy generates is submitted through the exact same
// Broker -> RiskManager -> ExecutionService -> MatchingEngine -> OrderBook
// path as live trading (architecture rule: no parallel execution system).
namespace backtest
{

    // One settled trade for the trade log (sourced from portfolio::Transaction,
    // not duplicated/recomputed).
    struct TradeRecord
    {
        std::time_t timestamp;
        std::string symbol;
        portfolio::TransactionSide side;
        int quantity;
        double price;
        double realizedPnL; // 0 for an opening trade
    };

    // One point on the equity curve, captured once per replay step.
    struct EquityPoint
    {
        std::time_t timestamp;
        std::size_t stepIndex;
        double equity; // portfolio value (cash + net position value) minus accrued commission
    };

    struct BacktestResult
    {
        std::string strategyName;
        double startingEquity = 0.0;
        double endingEquity = 0.0;
        double totalReturnPercent = 0.0;
        double benchmarkReturnPercent = 0.0; // buy-and-hold on config.benchmarkSymbol; 0 if none configured
        int numberOfTrades = 0;
        int winningTrades = 0; // closing/covering trades with realizedPnL > 0
        int losingTrades = 0;  // closing/covering trades with realizedPnL < 0
        double winRate = 0.0;  // percent, among winning+losing trades (opening trades are excluded)
        double maxDrawdownPercent = 0.0;
        double totalTransactionCosts = 0.0; // sum of commissionPerOrder over filled intents
        std::vector<TradeRecord> trades;
        std::vector<EquityPoint> equityCurve;
    };

    // Per-symbol static info needed to seed the backtest's own Market
    // (company name / sector). Optional - any symbol not given one here
    // falls back to name == symbol and Sector::Technology, which is fine
    // for a backtest since neither affects trading or P&L.
    struct InstrumentMeta
    {
        std::string name;
        market::Sector sector = market::Sector::Technology;
    };

    struct BacktestConfig
    {
        double startingCash = 100000.0;
        double commissionPerOrder = 0.0; // flat fee per intent that produces at least one fill
        double slippageBps = 0.0;        // adverse price adjustment applied to market-order intents, in basis points
        std::string benchmarkSymbol;     // optional; must be a symbol in the dataset if set
        risk::RiskConfig riskConfig = risk::RiskConfig();
        std::map<std::string, InstrumentMeta> instrumentMeta; // optional, see InstrumentMeta
    };

    class BacktestEngine
    {
    public:
        BacktestEngine(const historical::HistoricalDataSet &data, BacktestConfig config = BacktestConfig());

        // Runs strategy over the entire dataset, step 0 to stepCount()-1, and
        // returns the full result. Each BacktestEngine instance is good for
        // exactly one run (it owns the Market/Portfolio it mutates while running).
        BacktestResult run(Strategy &strategy);

    private:
        void seedMarket();
        void placeIntent(const OrderIntent &intent);

        historical::HistoricalDataSet data_;
        BacktestConfig config_;
        market::Market market_;
        portfolio::Portfolio portfolio_;
        execution::ExecutionService executionService_;
        broker::Broker broker_;
        historical::MarketReplay replay_;
        double cumulativeCommission_ = 0.0;
    };

} // namespace backtest

#endif // MARKETSIM_BACKTEST_BACKTESTENGINE_H