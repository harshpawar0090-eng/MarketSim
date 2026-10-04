#ifndef MARKETSIM_EXECUTION_EXECUTIONSERVICE_H
#define MARKETSIM_EXECUTION_EXECUTIONSERVICE_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"
#include "../Risk/RiskManager.h"
#include "MatchingEngine.h"

namespace execution
{

    // What the caller gets back for a submitted order: its final state and
    // every fill it produced. For a stop-type order that is only waiting for
    // its trigger, fills is empty and the status is WaitingForTrigger.
    struct OrderReceipt
    {
        Order order;
        std::vector<Fill> fills;

        int filledQuantity() const { return order.filledQuantity(); }
        double totalValue() const
        {
            double total = 0.0;
            for (const auto &f : fills)
            {
                total += f.value();
            }
            return total;
        }
        double averagePrice() const
        {
            const int qty = order.filledQuantity();
            return qty > 0 ? totalValue() / qty : 0.0;
        }
    };

    // Summary of one stop-type order that triggered during a market update.
    struct TriggerEvent
    {
        OrderId orderId;
        std::string symbol;
        OrderSide side;
        OrderType type;
        double marketPrice; // market price that fired the trigger
        int quantity;
        int filledQuantity;  // filled immediately after activation
        double averagePrice; // 0 if nothing filled
        OrderStatus status;  // state right after activation
        bool rejected;       // true if it could not be executed at trigger time
        std::string note;    // reason when rejected
    };

    // One forced-liquidation order sent through the normal execution path.
    struct LiquidationEvent
    {
        OrderId orderId;
        std::string symbol;
        OrderSide side; // Sell closes a long, Buy covers a short
        int requestedQuantity;
        int filledQuantity;
        double averagePrice; // 0 if nothing filled
    };

    // Outcome of ExecutionService::enforceMargin().
    struct LiquidationReport
    {
        bool marginCallDetected = false;
        bool resolved = true;    // false if the account is still in margin call afterwards
        int cancelledOrders = 0; // open orders cancelled before liquidating
        risk::AccountRisk before;
        risk::AccountRisk after;
        std::vector<LiquidationEvent> events;
    };

    // Result of ExecutionService::onMarketUpdate().
    struct MarketUpdateResult
    {
        std::vector<Fill> userFills;        // every fill involving the user during the update
        std::vector<TriggerEvent> triggers; // stop orders that fired
        LiquidationReport liquidation;      // margin check / forced liquidation outcome
    };

    // Glue between the MatchingEngine and the rest of the app:
    //   - gates every order through the RiskManager BEFORE it reaches the
    //     MatchingEngine (buying power, short rules, limits, margin call);
    //     a rejected order changes nothing,
    //   - submits executable orders to the MatchingEngine,
    //   - holds stop-type orders OUTSIDE the order book until their trigger
    //     fires, then re-validates and routes them through normal matching,
    //   - settles fills: Portfolio (long/short netting), transaction records,
    //     Market executed volume/turnover and top-of-book bid/ask,
    //   - monitors margin and force-liquidates through the same execution path.
    // It also keeps the market maker's quotes centred on the current Quote
    // price, re-quoting lazily whenever the price has moved (e.g. after tick()).
    class ExecutionService
    {
    public:
        ExecutionService(market::Market &mkt, portfolio::Portfolio &portfolioRef,
                         LiquidityConfig config = LiquidityConfig(),
                         risk::RiskConfig riskConfig = risk::RiskConfig());

        // Throw OrderRejectedError on invalid input / risk rejection.
        OrderReceipt placeMarketOrder(OrderSide side, const std::string &symbol, int quantity);
        OrderReceipt placeLimitOrder(OrderSide side, const std::string &symbol, int quantity,
                                     double limitPrice);

        // Places a Stop, StopLoss, TakeProfit or StopLimit order. It waits
        // (WaitingForTrigger) outside the order book until the market price
        // meets the trigger. limitPrice is used only for StopLimit.
        // Rejected if: wrong type, bad input, StopLoss/TakeProfit that is a BUY
        // or that is placed without owning shares (they protect a long; use a
        // BUY Stop/Stop-Limit to protect a short), the trigger would fire
        // immediately at the current price, or the risk check fails.
        OrderReceipt placeStopOrder(OrderType type, OrderSide side, const std::string &symbol,
                                    int quantity, double stopPrice, double limitPrice = 0.0);

        // Cancels one of the USER's open orders (resting or waiting for a
        // trigger). False if not found / not the user's / already finished.
        bool cancelOrder(OrderId id);

        // Any order the service knows about (waiting stops, resting, finished).
        std::optional<Order> getOrder(OrderId id) const;

        // The user's open orders: resting book orders plus stop orders waiting
        // for their trigger, sorted by id.
        std::vector<Order> openUserOrders() const;
        std::vector<Order> pendingStopOrders() const;

        // Cash tied up by open BUY orders that would open/increase a long.
        // (A BUY that covers a short reserves no cash.)
        double reservedCash() const;
        // Shares tied up by open SELL orders in this symbol.
        int reservedShares(const std::string &symbol) const;
        // $ exposure of open orders (either side) that would open/increase a position.
        double openOrderExposure() const;
        // Extra exposure that can still be opened, after open orders.
        double buyingPower() const;

        const risk::RiskManager &riskManager() const { return risk_; }
        risk::RiskManager &riskManager() { return risk_; }

        // Re-centres the market maker's quotes on the current price. Fills
        // this causes against the user's resting orders are settled.
        void refreshLiquidity(const std::string &symbol);
        void refreshAllLiquidity();

        // Evaluates every waiting stop order against the current market price;
        // triggered orders are activated, re-validated and executed through the
        // normal matching path and settled.
        std::vector<TriggerEvent> processTriggers();

        // Margin check. If the account is in margin call and automatic
        // liquidation is enabled: cancels all open orders, then repeatedly
        // closes the largest position (sell a long / buy to cover a short) with
        // market orders through the normal execution path until the call is
        // cleared. Bounded: stops after 200 orders or as soon as an order
        // cannot fill even after a liquidity refresh.
        LiquidationReport enforceMargin();

        // Call after the market has moved (e.g. after Market::tick()):
        // re-centres liquidity for every symbol, evaluates stop triggers on the
        // fresh liquidity, then runs the margin check / forced liquidation.
        MarketUpdateResult onMarketUpdate();

        const MatchingEngine &engine() const { return engine_; }

    private:
        OrderReceipt placeOrder(OrderSide side, OrderType type, const std::string &symbol,
                                int quantity, double limitPrice, bool liquidation = false);
        void ensureLiquidity(const std::string &symbol);
        void validateOrder(OrderSide side, OrderType type, const std::string &symbol,
                           int quantity, double limitPrice, bool liquidation) const;
        void validateStopOrder(OrderSide side, OrderType type, const std::string &symbol,
                               int quantity, double stopPrice, double limitPrice) const;
        risk::OrderCheck makeCheck(OrderSide side, const std::string &symbol, int quantity,
                                   double notional, bool liquidation) const;
        double referencePrice(const Order &order) const;
        int openQuantity(const std::string &symbol, bool isBuy) const;
        void settle(const std::vector<Fill> &fills);
        void syncTopOfBook(const std::string &symbol);
        std::vector<Order> restingUserOrders() const;

        market::Market &market_;
        portfolio::Portfolio &portfolio_;
        MatchingEngine engine_;
        LiquidityConfig config_;
        risk::RiskManager risk_;
        std::map<std::string, double> liquidityRef_; // price the maker last quoted around
        // Stop-type orders. Pending ones wait here (never in the order book);
        // a triggered order is removed once it is handed to the engine. Stops
        // that were cancelled or rejected at trigger time stay for lookup.
        std::map<OrderId, Order> stops_;
    };

} // namespace execution

#endif // MARKETSIM_EXECUTION_EXECUTIONSERVICE_H