#ifndef MARKETSIM_BROKER_BROKER_H
#define MARKETSIM_BROKER_BROKER_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "../Execution/ExecutionService.h"
#include "../Risk/RiskManager.h"

// Broker module: the client-facing trading API. The UI talks to the Broker
// and nothing else about trading. The Broker is deliberately thin - it
// contains NO matching logic (MatchingEngine), NO order-book logic
// (OrderBook), NO settlement logic (ExecutionService/Portfolio) and NO risk
// logic (RiskManager, reached through ExecutionService). It exposes named
// operations and read-only views.
namespace broker
{

    // Read-only snapshot of one symbol's order book for display.
    struct MarketDepth
    {
        std::string symbol;
        std::vector<execution::BookLevel> bids; // best (highest) first
        std::vector<execution::BookLevel> asks; // best (lowest) first
        std::size_t totalBidLevels = 0;         // levels in the book (>= bids.size())
        std::size_t totalAskLevels = 0;
        std::optional<double> bestBid;
        std::optional<double> bestAsk;

        std::optional<double> spread() const
        {
            if (bestBid && bestAsk)
            {
                return *bestAsk - *bestBid;
            }
            return std::nullopt;
        }
    };

    class Broker
    {
    public:
        explicit Broker(execution::ExecutionService &service);

        // ---- Order entry (throw execution::OrderRejectedError on rejection) ----
        // A SELL without owned shares opens a short when short selling is enabled;
        // a BUY with a short open covers it.
        execution::OrderReceipt placeMarketOrder(execution::OrderSide side, const std::string &symbol,
                                                 int quantity);
        execution::OrderReceipt placeLimitOrder(execution::OrderSide side, const std::string &symbol,
                                                int quantity, double limitPrice);
        execution::OrderReceipt placeStopOrder(execution::OrderSide side, const std::string &symbol,
                                               int quantity, double stopPrice);
        // Protective stop and profit target for shares already held (SELL).
        execution::OrderReceipt placeStopLossOrder(const std::string &symbol, int quantity,
                                                   double stopPrice);
        execution::OrderReceipt placeTakeProfitOrder(const std::string &symbol, int quantity,
                                                     double triggerPrice);
        execution::OrderReceipt placeStopLimitOrder(execution::OrderSide side, const std::string &symbol,
                                                    int quantity, double stopPrice, double limitPrice);

        // ---- Order management ----
        bool cancelOrder(execution::OrderId id);
        std::optional<execution::Order> getOrder(execution::OrderId id) const;
        // Resting orders plus stop orders waiting for their trigger.
        std::vector<execution::Order> openOrders() const;

        double reservedCash() const;
        int reservedShares(const std::string &symbol) const;

        // ---- Risk views / controls ----
        risk::AccountRisk accountRisk() const;
        // Extra exposure that can still be opened (after open orders).
        double buyingPower() const;
        const risk::RiskConfig &riskConfig() const;
        // Throws std::invalid_argument for an invalid configuration (state unchanged).
        void setRiskConfig(const risk::RiskConfig &config);
        // Margin check; force-liquidates through the execution path if in margin call.
        execution::LiquidationReport enforceMargin();

        // ---- Market data views ----
        // nullopt if the symbol has no order book. depth 0 = all levels.
        std::optional<MarketDepth> marketDepth(const std::string &symbol, std::size_t depth = 10) const;

        // ---- Market events ----
        // Call after the market moves (e.g. after Market::tick()). Re-centres
        // liquidity, fires stop orders whose trigger was reached, then runs the
        // margin check.
        execution::MarketUpdateResult onMarketUpdate();

    private:
        execution::ExecutionService &service_;
    };

} // namespace broker

#endif // MARKETSIM_BROKER_BROKER_H