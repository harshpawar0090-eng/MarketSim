#include "ExecutionService.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace execution
{

    namespace
    {
        constexpr int kMaxLiquidationOrders = 200; // hard bound on forced-liquidation orders per check
    }

    ExecutionService::ExecutionService(market::Market &mkt, portfolio::Portfolio &portfolioRef,
                                       LiquidityConfig config, risk::RiskConfig riskConfig)
        : market_(mkt), portfolio_(portfolioRef), config_(config), risk_(mkt, portfolioRef, riskConfig)
    {
        refreshAllLiquidity();
    }

    OrderReceipt ExecutionService::placeMarketOrder(OrderSide side, const std::string &symbol, int quantity)
    {
        return placeOrder(side, OrderType::Market, symbol, quantity, 0.0);
    }

    OrderReceipt ExecutionService::placeLimitOrder(OrderSide side, const std::string &symbol,
                                                   int quantity, double limitPrice)
    {
        return placeOrder(side, OrderType::Limit, symbol, quantity, limitPrice);
    }

    OrderReceipt ExecutionService::placeOrder(OrderSide side, OrderType type, const std::string &symbol,
                                              int quantity, double limitPrice, bool liquidation)
    {
        if (quantity <= 0)
        {
            throw OrderRejectedError("Order quantity must be positive");
        }
        if (!market_.exists(symbol))
        {
            throw OrderRejectedError("Symbol not found in market: " + symbol);
        }

        double limit = 0.0;
        if (type == OrderType::Limit)
        {
            if (!std::isfinite(limitPrice))
            {
                throw OrderRejectedError("Limit price must be a finite number");
            }
            limit = Order::roundPrice(limitPrice);
            if (!(limit > 0.0))
            {
                throw OrderRejectedError("Limit price must be positive");
            }
        }

        ensureLiquidity(symbol);
        validateOrder(side, type, symbol, quantity, limit, liquidation); // risk gate: nothing mutated if it throws

        SubmitResult result = engine_.submit(kUserId, symbol, side, type, quantity, limit,
                                             market_.clock().now());
        settle(result.fills);
        syncTopOfBook(symbol);
        return OrderReceipt{result.order, result.fills};
    }

    OrderReceipt ExecutionService::placeStopOrder(OrderType type, OrderSide side, const std::string &symbol,
                                                  int quantity, double stopPrice, double limitPrice)
    {
        if (!Order::isStopType(type))
        {
            throw OrderRejectedError("Not a stop-type order");
        }
        if (quantity <= 0)
        {
            throw OrderRejectedError("Order quantity must be positive");
        }
        if (!market_.exists(symbol))
        {
            throw OrderRejectedError("Symbol not found in market: " + symbol);
        }
        if ((type == OrderType::StopLoss || type == OrderType::TakeProfit) && side == OrderSide::Buy)
        {
            throw OrderRejectedError("Stop-loss and take-profit orders protect shares you hold, so they "
                                     "must be SELL orders (to protect a short position use a BUY Stop or "
                                     "Stop-Limit order)");
        }
        if ((type == OrderType::StopLoss || type == OrderType::TakeProfit) &&
            portfolio_.holdingQuantity(symbol) <= 0)
        {
            throw OrderRejectedError("Stop-loss and take-profit orders protect shares you hold, but you hold "
                                     "none of " +
                                     symbol + " (a short position does not count)");
        }

        if (!std::isfinite(stopPrice))
        {
            throw OrderRejectedError("Stop price must be a finite number");
        }
        const double stop = Order::roundPrice(stopPrice);
        if (!(stop > 0.0))
        {
            throw OrderRejectedError("Stop price must be positive");
        }

        double limit = 0.0;
        if (type == OrderType::StopLimit)
        {
            if (!std::isfinite(limitPrice))
            {
                throw OrderRejectedError("Limit price must be a finite number");
            }
            limit = Order::roundPrice(limitPrice);
            if (!(limit > 0.0))
            {
                throw OrderRejectedError("Limit price must be positive");
            }
        }

        const double current = market_.getQuote(symbol).price();
        if (Order::triggerConditionMet(type, side, stop, current))
        {
            std::ostringstream os;
            os << std::fixed << std::setprecision(2);
            os << "Trigger price $" << stop << " would fire immediately at the current price $" << current
               << " - choose a trigger on the other side of the market price";
            throw OrderRejectedError(os.str());
        }

        validateStopOrder(side, type, symbol, quantity, stop, limit);

        Order order(engine_.allocateOrderId(), kUserId, symbol, side, type, quantity, limit,
                    market_.clock().now(), stop);
        stops_.emplace(order.id(), order);
        return OrderReceipt{order, {}};
    }

    double ExecutionService::referencePrice(const Order &order) const
    {
        if (order.executionType() == OrderType::Limit)
        {
            return order.limitPrice(); // Limit, or a (pending or triggered) Stop-Limit
        }
        if (order.isStop())
        {
            return order.stopPrice(); // pending market-style stop
        }
        return market_.getQuote(order.symbol()).price();
    }

    int ExecutionService::openQuantity(const std::string &symbol, bool isBuy) const
    {
        int total = 0;
        for (const auto &o : openUserOrders())
        {
            if (o.symbol() == symbol && o.isBuy() == isBuy)
            {
                total += o.remainingQuantity();
            }
        }
        return total;
    }

    risk::OrderCheck ExecutionService::makeCheck(OrderSide side, const std::string &symbol, int quantity,
                                                 double notional, bool liquidation) const
    {
        risk::OrderCheck check;
        check.symbol = symbol;
        check.isBuy = (side == OrderSide::Buy);
        check.quantity = quantity;
        check.notional = notional;
        check.openOrderExposure = openOrderExposure();
        check.openBuyQuantity = openQuantity(symbol, true);
        check.openSellQuantity = openQuantity(symbol, false);
        check.liquidation = liquidation;
        return check;
    }

    void ExecutionService::validateOrder(OrderSide side, OrderType type, const std::string &symbol,
                                         int quantity, double limit, bool liquidation) const
    {
        // Expected value of the order: what it would fill against the book now,
        // plus (for a limit order) the remainder that will rest at the limit.
        int fillable = 0;
        double notional = 0.0;
        if (const OrderBook *bk = engine_.book(symbol))
        {
            notional = bk->previewCost(side, quantity, type == OrderType::Limit ? limit : 0.0,
                                       kUserId, fillable);
        }
        if (type == OrderType::Limit)
        {
            notional += static_cast<double>(quantity - fillable) * limit;
        }
        risk_.validate(makeCheck(side, symbol, quantity, notional, liquidation));
    }

    void ExecutionService::validateStopOrder(OrderSide side, OrderType type, const std::string &symbol,
                                             int quantity, double stopPrice, double limitPrice) const
    {
        // A waiting stop is budgeted at the price it is expected to cost: the
        // limit for a stop-limit, otherwise the trigger price. It is
        // re-validated against real liquidity when it triggers.
        const double unit = (type == OrderType::StopLimit) ? limitPrice : stopPrice;
        risk_.validate(makeCheck(side, symbol, quantity, unit * static_cast<double>(quantity), false));
    }

    void ExecutionService::settle(const std::vector<Fill> &fills)
    {
        for (const auto &fill : fills)
        {
            market_.recordExecution(fill.symbol, fill.quantity, fill.price);

            if (fill.buyParticipant == kUserId)
            {
                const portfolio::FillResult r = portfolio_.applyBuyFill(fill.symbol, fill.quantity, fill.price);
                if (r.closedQuantity > 0)
                {
                    portfolio_.recordTransaction(portfolio::Transaction(
                        fill.symbol, portfolio::TransactionSide::Buy, r.closedQuantity, fill.price,
                        r.realizedPnL, portfolio::PositionEffect::CoverShort));
                }
                if (r.openedQuantity > 0)
                {
                    portfolio_.recordTransaction(portfolio::Transaction(
                        fill.symbol, portfolio::TransactionSide::Buy, r.openedQuantity, fill.price,
                        0.0, portfolio::PositionEffect::OpenLong));
                }
            }
            if (fill.sellParticipant == kUserId)
            {
                const portfolio::FillResult r = portfolio_.applySellFill(fill.symbol, fill.quantity, fill.price);
                if (r.closedQuantity > 0)
                {
                    portfolio_.recordTransaction(portfolio::Transaction(
                        fill.symbol, portfolio::TransactionSide::Sell, r.closedQuantity, fill.price,
                        r.realizedPnL, portfolio::PositionEffect::CloseLong));
                }
                if (r.openedQuantity > 0)
                {
                    portfolio_.recordTransaction(portfolio::Transaction(
                        fill.symbol, portfolio::TransactionSide::Sell, r.openedQuantity, fill.price,
                        0.0, portfolio::PositionEffect::OpenShort));
                }
            }
        }
    }

    void ExecutionService::syncTopOfBook(const std::string &symbol)
    {
        const double price = market_.getQuote(symbol).price();
        double bid = price;
        double ask = price;
        if (const OrderBook *bk = engine_.book(symbol))
        {
            if (auto b = bk->bestBid())
            {
                bid = *b;
            }
            if (auto a = bk->bestAsk())
            {
                ask = *a;
            }
        }
        market_.setTopOfBook(symbol, bid, ask);
    }

    void ExecutionService::ensureLiquidity(const std::string &symbol)
    {
        const double price = market_.getQuote(symbol).price();
        auto it = liquidityRef_.find(symbol);
        const OrderBook *bk = engine_.book(symbol);
        const bool stale = (it == liquidityRef_.end()) || (it->second != price) ||
                           (bk == nullptr) || bk->ordersFor(kMarketMakerId).empty();
        if (stale)
        {
            refreshLiquidity(symbol);
        }
    }

    void ExecutionService::refreshLiquidity(const std::string &symbol)
    {
        const double price = market_.getQuote(symbol).price(); // throws if unknown symbol
        std::vector<Fill> fills = engine_.refreshLiquidity(symbol, price, market_.clock().now(), config_);
        liquidityRef_[symbol] = price;
        settle(fills);
        syncTopOfBook(symbol);
    }

    void ExecutionService::refreshAllLiquidity()
    {
        for (const auto &symbol : market_.listSymbols())
        {
            refreshLiquidity(symbol);
        }
    }

    std::vector<TriggerEvent> ExecutionService::processTriggers()
    {
        std::vector<TriggerEvent> events;

        // Decide first, act second: map order == ascending id == time priority.
        std::vector<OrderId> due;
        for (const auto &entry : stops_)
        {
            const Order &o = entry.second;
            if (o.isPending() && o.triggersAt(market_.getQuote(o.symbol()).price()))
            {
                due.push_back(entry.first);
            }
        }

        for (OrderId id : due)
        {
            auto it = stops_.find(id);
            if (it == stops_.end() || !it->second.isPending())
            {
                continue;
            }

            Order order = it->second;
            const std::string symbol = order.symbol();
            const double marketPrice = market_.getQuote(symbol).price();

            order.activate();
            stops_.erase(it); // releases this order's own reservation before re-checking risk

            TriggerEvent ev{id, symbol, order.side(), order.type(), marketPrice, order.quantity(),
                            0, 0.0, OrderStatus::New, false, ""};
            try
            {
                ensureLiquidity(symbol);
                validateOrder(order.side(), order.executionType(), symbol, order.quantity(),
                              order.limitPrice(), false);

                SubmitResult result = engine_.submitOrder(order, market_.clock().now());
                settle(result.fills);
                syncTopOfBook(symbol);

                double value = 0.0;
                for (const auto &f : result.fills)
                {
                    value += f.value();
                }
                ev.status = result.order.status();
                ev.filledQuantity = result.order.filledQuantity();
                ev.averagePrice = ev.filledQuantity > 0 ? value / ev.filledQuantity : 0.0;
            }
            catch (const ExecutionError &e)
            {
                // Could not be executed at trigger time (e.g. buying power no
                // longer sufficient): the order is cancelled, nothing was mutated.
                order.cancel();
                stops_.emplace(id, order);
                ev.status = OrderStatus::Cancelled;
                ev.rejected = true;
                ev.note = e.what();
            }
            events.push_back(ev);
        }
        return events;
    }

    LiquidationReport ExecutionService::enforceMargin()
    {
        LiquidationReport report;
        report.before = risk_.assess();
        report.after = report.before;
        if (!report.before.marginCall())
        {
            return report;
        }

        report.marginCallDetected = true;
        report.resolved = false;
        if (!risk_.config().autoLiquidation)
        {
            return report;
        }

        // Step 1: cancel every open order (resting and waiting stops) so their
        // reservations cannot block the liquidation.
        for (const Order &o : openUserOrders())
        {
            if (cancelOrder(o.id()))
            {
                ++report.cancelledOrders;
            }
        }

        // Step 2: close the largest position with market orders through the
        // normal execution path. Bounded loop; stops when the call is cleared,
        // no positions remain, or an order cannot fill at all.
        for (int i = 0; i < kMaxLiquidationOrders; ++i)
        {
            if (!risk_.assess().marginCall())
            {
                break;
            }

            const std::vector<portfolio::Position> positions = portfolio_.positions();
            const portfolio::Position *target = nullptr;
            double targetValue = -1.0;
            for (const auto &pos : positions)
            {
                const int absQuantity = pos.quantity < 0 ? -pos.quantity : pos.quantity;
                const double value = absQuantity * market_.getQuote(pos.symbol).price();
                if (value > targetValue)
                {
                    target = &pos;
                    targetValue = value;
                }
            }
            if (target == nullptr)
            {
                break;
            }

            const std::string symbol = target->symbol;
            const OrderSide side = (target->quantity > 0) ? OrderSide::Sell : OrderSide::Buy;
            const int quantity = risk_.liquidationQuantity(symbol);
            if (quantity <= 0)
            {
                break;
            }

            try
            {
                OrderReceipt receipt = placeOrder(side, OrderType::Market, symbol, quantity, 0.0, true);
                if (receipt.filledQuantity() == 0)
                {
                    // The maker's side may be exhausted: restore depth once and retry.
                    refreshLiquidity(symbol);
                    receipt = placeOrder(side, OrderType::Market, symbol, quantity, 0.0, true);
                }
                if (receipt.filledQuantity() == 0)
                {
                    break; // no progress possible: stop instead of looping
                }
                report.events.push_back(LiquidationEvent{receipt.order.id(), symbol, side, quantity,
                                                         receipt.filledQuantity(), receipt.averagePrice()});
            }
            catch (const ExecutionError &)
            {
                break;
            }
        }

        report.after = risk_.assess();
        report.resolved = !report.after.marginCall();
        return report;
    }

    MarketUpdateResult ExecutionService::onMarketUpdate()
    {
        MarketUpdateResult result;
        const std::size_t before = engine_.fills().size();

        refreshAllLiquidity();                // keep existing re-quoting behaviour
        result.triggers = processTriggers();  // then evaluate stops on fresh liquidity
        result.liquidation = enforceMargin(); // then the margin check / forced liquidation

        const auto &log = engine_.fills();
        for (std::size_t i = before; i < log.size(); ++i)
        {
            if (log[i].buyParticipant == kUserId || log[i].sellParticipant == kUserId)
            {
                result.userFills.push_back(log[i]);
            }
        }
        return result;
    }

    bool ExecutionService::cancelOrder(OrderId id)
    {
        auto stopIt = stops_.find(id);
        if (stopIt != stops_.end())
        {
            Order &o = stopIt->second;
            if (o.participant() != kUserId || !o.isPending())
            {
                return false;
            }
            o.cancel();
            return true;
        }

        auto order = engine_.getOrder(id);
        if (!order || order->participant() != kUserId || !order->isActive())
        {
            return false;
        }
        const bool cancelled = engine_.cancel(id).has_value();
        if (cancelled)
        {
            syncTopOfBook(order->symbol());
        }
        return cancelled;
    }

    std::optional<Order> ExecutionService::getOrder(OrderId id) const
    {
        auto it = stops_.find(id);
        if (it != stops_.end())
        {
            return it->second;
        }
        return engine_.getOrder(id);
    }

    std::vector<Order> ExecutionService::restingUserOrders() const
    {
        std::vector<Order> result;
        for (const auto &symbol : market_.listSymbols())
        {
            if (const OrderBook *bk = engine_.book(symbol))
            {
                for (const auto &o : bk->ordersFor(kUserId))
                {
                    result.push_back(o);
                }
            }
        }
        return result;
    }

    std::vector<Order> ExecutionService::pendingStopOrders() const
    {
        std::vector<Order> result;
        for (const auto &entry : stops_)
        {
            if (entry.second.isPending() && entry.second.participant() == kUserId)
            {
                result.push_back(entry.second);
            }
        }
        return result;
    }

    std::vector<Order> ExecutionService::openUserOrders() const
    {
        std::vector<Order> result = restingUserOrders();
        for (const auto &o : pendingStopOrders())
        {
            result.push_back(o);
        }
        std::sort(result.begin(), result.end(),
                  [](const Order &a, const Order &b)
                  { return a.id() < b.id(); });
        return result;
    }

    double ExecutionService::reservedCash() const
    {
        double reserved = 0.0;
        for (const auto &o : openUserOrders())
        {
            if (o.isBuy() && risk_.isOpening(o.symbol(), true))
            {
                reserved += referencePrice(o) * o.remainingQuantity();
            }
        }
        return reserved;
    }

    int ExecutionService::reservedShares(const std::string &symbol) const
    {
        int reserved = 0;
        for (const auto &o : openUserOrders())
        {
            if (!o.isBuy() && o.symbol() == symbol)
            {
                reserved += o.remainingQuantity();
            }
        }
        return reserved;
    }

    double ExecutionService::openOrderExposure() const
    {
        double exposure = 0.0;
        for (const auto &o : openUserOrders())
        {
            if (risk_.isOpening(o.symbol(), o.isBuy()))
            {
                exposure += referencePrice(o) * o.remainingQuantity();
            }
        }
        return exposure;
    }

    double ExecutionService::buyingPower() const
    {
        return risk_.buyingPower(openOrderExposure());
    }

} // namespace execution