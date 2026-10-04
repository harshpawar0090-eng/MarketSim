#include "BacktestEngine.h"
#include <algorithm>
#include "../Portfolio/Portfolio.h"
#include <optional>
namespace backtest
{

    BacktestEngine::BacktestEngine(const historical::HistoricalDataSet &data, BacktestConfig config)
        : data_(data), config_(std::move(config)), market_(), portfolio_(config_.startingCash),
          executionService_(market_, portfolio_, execution::LiquidityConfig(), config_.riskConfig),
          broker_(executionService_), replay_(market_, broker_, data_)
    {
        seedMarket();
        // ExecutionService's constructor already called refreshAllLiquidity(),
        // but the market had no stocks yet at that point - do it again now
        // that seedMarket() has added them.
        broker_.onMarketUpdate();
    }

    void BacktestEngine::seedMarket()
    {
        for (const auto &symbol : data_.symbols())
        {
            const historical::HistoricalBar &firstBar = data_.barAt(symbol, 0);

            std::string name = symbol;
            market::Sector sector = market::Sector::Technology;
            auto metaIt = config_.instrumentMeta.find(symbol);
            if (metaIt != config_.instrumentMeta.end())
            {
                name = metaIt->second.name.empty() ? symbol : metaIt->second.name;
                sector = metaIt->second.sector;
            }

            market_.addStock(market::Stock(symbol, name, sector, firstBar.open));
        }
    }

    void BacktestEngine::placeIntent(const OrderIntent &intent)
    {
        try
        {
            std::optional<execution::OrderReceipt> receipt;
            if (intent.limitPrice > 0.0)
            {
                receipt = broker_.placeLimitOrder(intent.side, intent.symbol, intent.quantity, intent.limitPrice);
            }
            else if (config_.slippageBps > 0.0)
            {
                const double ref = market_.getQuote(intent.symbol).price();
                const double adjusted = (intent.side == execution::OrderSide::Buy)
                                            ? ref * (1.0 + config_.slippageBps / 10000.0)
                                            : ref * (1.0 - config_.slippageBps / 10000.0);
                receipt = broker_.placeLimitOrder(intent.side, intent.symbol, intent.quantity, adjusted);
            }
            else
            {
                receipt = broker_.placeMarketOrder(intent.side, intent.symbol, intent.quantity);
            }

            if (receipt.has_value() && receipt->filledQuantity() > 0 && config_.commissionPerOrder > 0.0)
            {
                cumulativeCommission_ += config_.commissionPerOrder;
            }
        }
        catch (const execution::OrderRejectedError &)
        {
            // Rejected intents (e.g. insufficient buying power, bad quantity)
            // are simply skipped - same as a live trader's rejected order
            // doing nothing. The backtest keeps running.
        }
    }

    BacktestResult BacktestEngine::run(Strategy &strategy)
    {
        BacktestResult result;
        result.strategyName = strategy.name();
        result.startingEquity = portfolio::PortfolioAnalyzer::totalPortfolioValue(portfolio_, market_);

        double peakEquity = result.startingEquity;
        const std::vector<std::string> allSymbols = data_.symbols();
        const std::size_t steps = replay_.stepCount();

        for (std::size_t i = 0; i < steps; ++i)
        {
            replay_.step(); // applies bar i for every symbol, then broker_.onMarketUpdate()

            MarketView view(market_, portfolio_, data_, i);
            std::vector<OrderIntent> intents = strategy.onStep(view);
            for (const auto &intent : intents)
            {
                placeIntent(intent);
            }
            if (!intents.empty())
            {
                broker_.onMarketUpdate(); // re-run stops/margin after the strategy's own orders
            }

            const double equity =
                portfolio::PortfolioAnalyzer::totalPortfolioValue(portfolio_, market_) - cumulativeCommission_;
            peakEquity = std::max(peakEquity, equity);
            const double drawdown = peakEquity > 0.0 ? (peakEquity - equity) / peakEquity * 100.0 : 0.0;
            result.maxDrawdownPercent = std::max(result.maxDrawdownPercent, drawdown);

            const std::time_t stepTimestamp = allSymbols.empty() ? 0 : data_.barAt(allSymbols.front(), i).timestamp;
            result.equityCurve.push_back(EquityPoint{stepTimestamp, i, equity});
        }

        result.endingEquity = result.equityCurve.empty() ? result.startingEquity : result.equityCurve.back().equity;
        result.totalReturnPercent = result.startingEquity > 0.0
                                        ? (result.endingEquity - result.startingEquity) / result.startingEquity * 100.0
                                        : 0.0;

        if (!config_.benchmarkSymbol.empty() && data_.hasSymbol(config_.benchmarkSymbol))
        {
            const auto &series = data_.series(config_.benchmarkSymbol);
            const double firstClose = series.bars().front().close;
            const double lastClose = series.bars().back().close;
            result.benchmarkReturnPercent = firstClose > 0.0 ? (lastClose - firstClose) / firstClose * 100.0 : 0.0;
        }

        for (const auto &t : portfolio_.history())
        {
            result.trades.push_back(TradeRecord{t.timestamp, t.symbol, t.side, t.quantity, t.price, t.realizedPnL});
            if (t.realizedPnL > 0.0)
            {
                ++result.winningTrades;
            }
            else if (t.realizedPnL < 0.0)
            {
                ++result.losingTrades;
            }
        }
        result.numberOfTrades = static_cast<int>(portfolio_.history().size());
        const int decided = result.winningTrades + result.losingTrades;
        result.winRate = decided > 0 ? static_cast<double>(result.winningTrades) / decided * 100.0 : 0.0;
        result.totalTransactionCosts = cumulativeCommission_;

        return result;
    }

} // namespace backtest