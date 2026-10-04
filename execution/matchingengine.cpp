#include "MatchingEngine.h"
#include <algorithm>

namespace execution
{

    OrderBook &MatchingEngine::bookFor(const std::string &symbol)
    {
        auto it = books_.find(symbol);
        if (it == books_.end())
        {
            it = books_.emplace(symbol, OrderBook(symbol)).first;
        }
        return it->second;
    }

    void MatchingEngine::archive(const Order &order)
    {
        closed_.insert_or_assign(order.id(), order);
    }

    const OrderBook *MatchingEngine::book(const std::string &symbol) const
    {
        auto it = books_.find(symbol);
        return it == books_.end() ? nullptr : &it->second;
    }

    SubmitResult MatchingEngine::submit(const std::string &participant, const std::string &symbol,
                                        OrderSide side, OrderType type, int quantity,
                                        double limitPrice, std::time_t timestamp)
    {
        if (type != OrderType::Market && type != OrderType::Limit)
        {
            throw ExecutionError("MatchingEngine only accepts Market/Limit orders; "
                                 "stop orders must be triggered first");
        }

        Order order(nextOrderId_, participant, symbol, side, type, quantity, limitPrice, timestamp);
        ++nextOrderId_;
        return submitOrder(std::move(order), timestamp);
    }

    SubmitResult MatchingEngine::submitOrder(Order order, std::time_t timestamp)
    {
        if (order.status() != OrderStatus::New)
        {
            throw ExecutionError("Only a new, executable order can be matched "
                                 "(stop orders must be triggered first)");
        }

        const std::string symbol = order.symbol();
        const std::string participant = order.participant();
        const OrderSide side = order.side();
        const bool isLimit = (order.executionType() == OrderType::Limit);

        OrderBook &bk = bookFor(symbol);
        const OrderSide restingSide = (side == OrderSide::Buy) ? OrderSide::Sell : OrderSide::Buy;
        std::vector<Fill> fills;

        while (order.remainingQuantity() > 0)
        {
            const Order *best = bk.bestOrder(restingSide);
            if (best == nullptr)
            {
                break;
            }

            if (isLimit)
            {
                const bool crosses = (side == OrderSide::Buy)
                                         ? (order.limitPrice() >= best->limitPrice())
                                         : (order.limitPrice() <= best->limitPrice());
                if (!crosses)
                {
                    break;
                }
            }

            if (best->participant() == participant)
            {
                // Self-trade prevention: cancel the resting order, keep matching.
                const OrderId restingId = best->id();
                if (auto cancelled = bk.cancel(restingId))
                {
                    archive(*cancelled);
                }
                continue;
            }

            const int qty = std::min(order.remainingQuantity(), best->remainingQuantity());
            const double price = best->limitPrice(); // trade at the resting order's price
            Order resting = bk.fillBest(restingSide, qty);
            order.applyFill(qty);

            const bool incomingIsBuy = (side == OrderSide::Buy);
            Fill fill{
                nextFillId_++,
                symbol,
                price,
                qty,
                incomingIsBuy ? order.id() : resting.id(),
                incomingIsBuy ? resting.id() : order.id(),
                incomingIsBuy ? order.participant() : resting.participant(),
                incomingIsBuy ? resting.participant() : order.participant(),
                side,
                timestamp};
            fills.push_back(fill);
            fillLog_.push_back(fill);

            if (resting.status() == OrderStatus::Filled)
            {
                archive(resting);
            }
        }

        if (order.remainingQuantity() > 0)
        {
            if (isLimit)
            {
                bk.add(order); // rest the remainder (price-time priority from here)
            }
            else
            {
                order.cancel(); // market order: unfilled remainder is cancelled
            }
        }

        if (!order.isActive())
        {
            archive(order);
        }

        return SubmitResult{order, std::move(fills)};
    }

    std::optional<Order> MatchingEngine::cancel(OrderId id)
    {
        for (auto &entry : books_)
        {
            if (auto cancelled = entry.second.cancel(id))
            {
                archive(*cancelled);
                return cancelled;
            }
        }
        return std::nullopt;
    }

    std::optional<Order> MatchingEngine::getOrder(OrderId id) const
    {
        for (const auto &entry : books_)
        {
            if (auto resting = entry.second.find(id))
            {
                return resting;
            }
        }
        auto it = closed_.find(id);
        if (it != closed_.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<Fill> MatchingEngine::refreshLiquidity(const std::string &symbol, double referencePrice,
                                                       std::time_t timestamp, const LiquidityConfig &config)
    {
        OrderBook &bk = bookFor(symbol);
        for (const auto &cancelled : bk.cancelAllFor(kMarketMakerId))
        {
            archive(cancelled);
        }

        std::vector<Fill> allFills;
        if (!(referencePrice > 0.0) || config.levels <= 0 || config.quantityPerLevel <= 0)
        {
            return allFills;
        }

        for (int level = 0; level < config.levels; ++level)
        {
            const double offset = config.halfSpreadFraction + level * config.stepFraction;
            double bid = Order::roundPrice(referencePrice * (1.0 - offset));
            double ask = Order::roundPrice(referencePrice * (1.0 + offset));
            if (bid < 0.01)
            {
                bid = 0.01;
            }
            if (ask <= bid)
            {
                ask = bid + 0.01;
            }

            SubmitResult b = submit(kMarketMakerId, symbol, OrderSide::Buy, OrderType::Limit,
                                    config.quantityPerLevel, bid, timestamp);
            allFills.insert(allFills.end(), b.fills.begin(), b.fills.end());

            SubmitResult a = submit(kMarketMakerId, symbol, OrderSide::Sell, OrderType::Limit,
                                    config.quantityPerLevel, ask, timestamp);
            allFills.insert(allFills.end(), a.fills.begin(), a.fills.end());
        }
        return allFills;
    }

} // namespace execution