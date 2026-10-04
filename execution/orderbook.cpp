#include "OrderBook.h"
#include <algorithm>

namespace execution
{

    namespace
    {
        template <class Map>
        std::optional<Order> eraseOrder(Map &levels, double price, OrderId id)
        {
            auto levelIt = levels.find(price);
            if (levelIt == levels.end())
            {
                return std::nullopt;
            }
            auto &queue = levelIt->second;
            for (auto it = queue.begin(); it != queue.end(); ++it)
            {
                if (it->id() == id)
                {
                    Order copy = *it;
                    queue.erase(it);
                    if (queue.empty())
                    {
                        levels.erase(levelIt);
                    }
                    return copy;
                }
            }
            return std::nullopt;
        }

        template <class Map>
        std::vector<BookLevel> summarize(const Map &levels, std::size_t depth)
        {
            std::vector<BookLevel> result;
            for (const auto &entry : levels)
            {
                if (depth > 0 && result.size() >= depth)
                {
                    break;
                }
                int quantity = 0;
                for (const auto &order : entry.second)
                {
                    quantity += order.remainingQuantity();
                }
                result.push_back({entry.first, quantity, static_cast<int>(entry.second.size())});
            }
            return result;
        }

        template <class Map>
        std::size_t countOrders(const Map &levels)
        {
            std::size_t count = 0;
            for (const auto &entry : levels)
            {
                count += entry.second.size();
            }
            return count;
        }

        template <class Map>
        void collectFor(const Map &levels, const std::string &participant, std::vector<Order> &out)
        {
            for (const auto &entry : levels)
            {
                for (const auto &order : entry.second)
                {
                    if (order.participant() == participant)
                    {
                        out.push_back(order);
                    }
                }
            }
        }

        template <class Map>
        double walkLevels(const Map &levels, bool takerIsBuy, int quantity, double limitPrice,
                          const std::string &exclude, int &fillable)
        {
            double cost = 0.0;
            fillable = 0;
            for (const auto &entry : levels)
            {
                const double price = entry.first;
                if (limitPrice > 0.0)
                {
                    // Same crossing rule as MatchingEngine::submitOrder.
                    if (takerIsBuy ? (price > limitPrice) : (price < limitPrice))
                    {
                        break;
                    }
                }
                for (const auto &order : entry.second)
                {
                    if (order.participant() == exclude)
                    {
                        continue;
                    }
                    const int take = std::min(quantity - fillable, order.remainingQuantity());
                    cost += price * take;
                    fillable += take;
                    if (fillable >= quantity)
                    {
                        return cost;
                    }
                }
            }
            return cost;
        }
    } // namespace

    OrderBook::OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

    void OrderBook::add(const Order &order)
    {
        // Only orders that execute as Limit orders can rest (Limit, or a
        // StopLimit that has already triggered). Waiting stop orders are
        // not active, so they are rejected by the isActive() check below.
        if (order.executionType() != OrderType::Limit)
        {
            throw ExecutionError("Only limit orders can rest in the order book");
        }
        if (!order.isActive())
        {
            throw ExecutionError("Cannot rest an order that is not active");
        }
        if (index_.count(order.id()) > 0)
        {
            throw ExecutionError("Duplicate order id in order book");
        }

        if (order.isBuy())
        {
            bids_[order.limitPrice()].push_back(order);
        }
        else
        {
            asks_[order.limitPrice()].push_back(order);
        }
        index_[order.id()] = Locator{order.side(), order.limitPrice()};
    }

    std::optional<Order> OrderBook::cancel(OrderId id)
    {
        auto idx = index_.find(id);
        if (idx == index_.end())
        {
            return std::nullopt;
        }
        const Locator loc = idx->second;
        std::optional<Order> removed = (loc.side == OrderSide::Buy)
                                           ? eraseOrder(bids_, loc.price, id)
                                           : eraseOrder(asks_, loc.price, id);
        index_.erase(idx);
        if (removed)
        {
            removed->cancel();
        }
        return removed;
    }

    std::vector<Order> OrderBook::cancelAllFor(const std::string &participant)
    {
        std::vector<Order> targets = ordersFor(participant);
        std::vector<Order> cancelled;
        for (const auto &order : targets)
        {
            if (auto removed = cancel(order.id()))
            {
                cancelled.push_back(*removed);
            }
        }
        return cancelled;
    }

    std::optional<Order> OrderBook::find(OrderId id) const
    {
        auto idx = index_.find(id);
        if (idx == index_.end())
        {
            return std::nullopt;
        }
        const Locator &loc = idx->second;
        if (loc.side == OrderSide::Buy)
        {
            auto levelIt = bids_.find(loc.price);
            if (levelIt != bids_.end())
            {
                for (const auto &order : levelIt->second)
                {
                    if (order.id() == id)
                    {
                        return order;
                    }
                }
            }
        }
        else
        {
            auto levelIt = asks_.find(loc.price);
            if (levelIt != asks_.end())
            {
                for (const auto &order : levelIt->second)
                {
                    if (order.id() == id)
                    {
                        return order;
                    }
                }
            }
        }
        return std::nullopt;
    }

    Order *OrderBook::bestOrder(OrderSide side)
    {
        if (side == OrderSide::Buy)
        {
            return bids_.empty() ? nullptr : &bids_.begin()->second.front();
        }
        return asks_.empty() ? nullptr : &asks_.begin()->second.front();
    }

    const Order *OrderBook::bestOrder(OrderSide side) const
    {
        if (side == OrderSide::Buy)
        {
            return bids_.empty() ? nullptr : &bids_.begin()->second.front();
        }
        return asks_.empty() ? nullptr : &asks_.begin()->second.front();
    }

    Order OrderBook::fillBest(OrderSide side, int quantity)
    {
        Order *best = bestOrder(side);
        if (best == nullptr)
        {
            throw ExecutionError("Cannot fill: order book side is empty");
        }
        best->applyFill(quantity);
        Order updated = *best; // copy before any erase invalidates the pointer
        if (updated.remainingQuantity() == 0)
        {
            if (side == OrderSide::Buy)
            {
                eraseOrder(bids_, updated.limitPrice(), updated.id());
            }
            else
            {
                eraseOrder(asks_, updated.limitPrice(), updated.id());
            }
            index_.erase(updated.id());
        }
        return updated;
    }

    std::optional<double> OrderBook::bestBid() const
    {
        if (bids_.empty())
        {
            return std::nullopt;
        }
        return bids_.begin()->first;
    }

    std::optional<double> OrderBook::bestAsk() const
    {
        if (asks_.empty())
        {
            return std::nullopt;
        }
        return asks_.begin()->first;
    }

    std::vector<BookLevel> OrderBook::bidLevels(std::size_t depth) const
    {
        return summarize(bids_, depth);
    }

    std::vector<BookLevel> OrderBook::askLevels(std::size_t depth) const
    {
        return summarize(asks_, depth);
    }

    std::size_t OrderBook::bidOrderCount() const { return countOrders(bids_); }
    std::size_t OrderBook::askOrderCount() const { return countOrders(asks_); }

    std::vector<Order> OrderBook::ordersFor(const std::string &participant) const
    {
        std::vector<Order> result;
        collectFor(bids_, participant, result);
        collectFor(asks_, participant, result);
        std::sort(result.begin(), result.end(),
                  [](const Order &a, const Order &b)
                  { return a.id() < b.id(); });
        return result;
    }

    double OrderBook::previewCost(OrderSide takerSide, int quantity, double limitPrice,
                                  const std::string &excludeParticipant, int &fillable) const
    {
        if (takerSide == OrderSide::Buy)
        {
            return walkLevels(asks_, true, quantity, limitPrice, excludeParticipant, fillable);
        }
        return walkLevels(bids_, false, quantity, limitPrice, excludeParticipant, fillable);
    }

} // namespace execution