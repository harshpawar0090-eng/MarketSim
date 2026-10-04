#ifndef MARKETSIM_EXECUTION_MATCHINGENGINE_H
#define MARKETSIM_EXECUTION_MATCHINGENGINE_H

#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Order.h"
#include "OrderBook.h"

namespace execution
{

    // Shape of the market maker's resting ladder around a reference price.
    struct LiquidityConfig
    {
        int levels = 5;                     // price levels per side
        int quantityPerLevel = 500;         // shares per level
        double halfSpreadFraction = 0.0005; // 0.05% each side of the reference price
        double stepFraction = 0.0005;       // extra distance per further level
    };

    // Final state of the submitted order plus every fill it caused.
    struct SubmitResult
    {
        Order order;
        std::vector<Fill> fills;
    };

    // Owns one OrderBook per symbol and is the only component that produces
    // Fills. It knows nothing about Portfolio or Market: it takes orders in
    // and returns fills out. It only ever sees EXECUTABLE orders (Market or
    // Limit, or a stop-type order that has already been triggered): waiting
    // stop orders are tracked elsewhere and never reach the book.
    //
    // Rules:
    //  - Price-time priority; trades execute at the RESTING order's price.
    //  - Limit order: matches while prices cross, remainder rests in the book.
    //  - Market order: matches against whatever liquidity exists; any
    //    unfilled remainder is cancelled (it never rests).
    //  - Partial fills are supported on both sides.
    //  - Self-trade prevention: if the best resting order belongs to the same
    //    participant as the incoming order, the RESTING order is cancelled and
    //    matching continues.
    class MatchingEngine
    {
    public:
        MatchingEngine() = default;

        // Creates a new Market or Limit order with the next id and matches it.
        // Throws ExecutionError for malformed orders (qty <= 0, bad limit
        // price) and for stop-type order types (those must be triggered first
        // and go through submitOrder).
        SubmitResult submit(const std::string &participant, const std::string &symbol,
                            OrderSide side, OrderType type, int quantity,
                            double limitPrice, std::time_t timestamp);

        // Reserves the next order id (used for stop orders that wait outside
        // the book but must share the same id sequence).
        OrderId allocateOrderId() { return nextOrderId_++; }

        // Matches an already-built, executable order (status New): e.g. a stop
        // order that has just been activated. fills are stamped with timestamp.
        // Throws ExecutionError if the order is not executable.
        SubmitResult submitOrder(Order order, std::time_t timestamp);

        // Cancels a resting order. Returns it, or nullopt if not resting.
        std::optional<Order> cancel(OrderId id);

        // Replaces the market maker's resting quotes for a symbol with a fresh
        // ladder around referencePrice. New quotes go through normal matching,
        // so they can fill other participants' resting orders they cross.
        std::vector<Fill> refreshLiquidity(const std::string &symbol, double referencePrice,
                                           std::time_t timestamp, const LiquidityConfig &config);

        // nullptr if no order has ever been submitted for the symbol.
        const OrderBook *book(const std::string &symbol) const;

        // Current state of any order the engine has seen (resting or finished).
        std::optional<Order> getOrder(OrderId id) const;

        // Chronological log of every fill.
        const std::vector<Fill> &fills() const { return fillLog_; }

    private:
        OrderBook &bookFor(const std::string &symbol);
        void archive(const Order &order);

        std::map<std::string, OrderBook> books_;
        std::map<OrderId, Order> closed_; // orders that are no longer resting
        std::vector<Fill> fillLog_;
        OrderId nextOrderId_ = 1;
        unsigned long long nextFillId_ = 1;
    };

} // namespace execution

#endif // MARKETSIM_EXECUTION_MATCHINGENGINE_H