#ifndef MARKETSIM_EXECUTION_ORDERBOOK_H
#define MARKETSIM_EXECUTION_ORDERBOOK_H

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Order.h"

namespace execution
{

    // Aggregated view of one price level (for display / tests).
    struct BookLevel
    {
        double price;
        int quantity;   // total remaining quantity at this price
        int orderCount; // number of resting orders at this price
    };

    // Bid/ask books for ONE symbol. Holds only resting Limit orders.
    //
    // Price-time priority: bids are ordered by highest price first, asks by
    // lowest price first; within a price level orders keep arrival order
    // (FIFO). A partially filled order keeps its place in the queue.
    //
    // The book is passive storage: it never decides whether two orders match
    // - that is the MatchingEngine's job.
    class OrderBook
    {
    public:
        explicit OrderBook(std::string symbol);

        const std::string &symbol() const { return symbol_; }

        // Rests an active Limit order. Throws ExecutionError otherwise.
        void add(const Order &order);

        // Removes a resting order and returns it (status Cancelled).
        // Returns nullopt if the id is not resting in this book.
        std::optional<Order> cancel(OrderId id);

        // Cancels every resting order of one participant; returns them.
        std::vector<Order> cancelAllFor(const std::string &participant);

        bool contains(OrderId id) const { return index_.count(id) > 0; }
        std::optional<Order> find(OrderId id) const;

        // Front order of the best level on a side (nullptr if that side is empty).
        Order *bestOrder(OrderSide side);
        const Order *bestOrder(OrderSide side) const;

        // Applies a fill to the best order on a side, removing it when fully
        // filled. Returns the order's state after the fill.
        Order fillBest(OrderSide side, int quantity);

        std::optional<double> bestBid() const;
        std::optional<double> bestAsk() const;

        // depth 0 = all levels.
        std::vector<BookLevel> bidLevels(std::size_t depth = 0) const;
        std::vector<BookLevel> askLevels(std::size_t depth = 0) const;

        std::size_t bidOrderCount() const;
        std::size_t askOrderCount() const;
        bool empty() const { return index_.empty(); }

        std::vector<Order> ordersFor(const std::string &participant) const;

        // Dry run: walks the opposite side exactly as the engine would for an
        // incoming order of takerSide, skipping orders of excludeParticipant
        // (which the engine would cancel for self-trade prevention). limitPrice
        // <= 0 means "no price limit" (market order). Returns the total cost
        // of what would fill; fillable receives the quantity that would fill.
        double previewCost(OrderSide takerSide, int quantity, double limitPrice,
                           const std::string &excludeParticipant, int &fillable) const;

    private:
        struct Locator
        {
            OrderSide side;
            double price;
        };

        using BidMap = std::map<double, std::deque<Order>, std::greater<double>>;
        using AskMap = std::map<double, std::deque<Order>>;

        std::string symbol_;
        BidMap bids_;
        AskMap asks_;
        std::unordered_map<OrderId, Locator> index_;
    };

} // namespace execution

#endif // MARKETSIM_EXECUTION_ORDERBOOK_H