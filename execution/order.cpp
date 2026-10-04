#include "Order.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace execution
{

    namespace
    {
        // True if the trigger fires when the market price RISES to the stop
        // price (price >= stop); false if it fires when the price FALLS to it.
        bool triggersOnRise(OrderType type, OrderSide side)
        {
            if (type == OrderType::TakeProfit)
            {
                return side == OrderSide::Sell;
            }
            return side == OrderSide::Buy;
        }
    } // namespace

    std::string orderSideToString(OrderSide side)
    {
        return side == OrderSide::Buy ? "BUY" : "SELL";
    }

    std::string orderTypeToString(OrderType type)
    {
        switch (type)
        {
        case OrderType::Market:
            return "MARKET";
        case OrderType::Limit:
            return "LIMIT";
        case OrderType::Stop:
            return "STOP";
        case OrderType::StopLoss:
            return "STOP_LOSS";
        case OrderType::TakeProfit:
            return "TAKE_PROFIT";
        case OrderType::StopLimit:
            return "STOP_LIMIT";
        }
        return "UNKNOWN";
    }

    std::string orderStatusToString(OrderStatus status)
    {
        switch (status)
        {
        case OrderStatus::New:
            return "NEW";
        case OrderStatus::PartiallyFilled:
            return "PARTIALLY_FILLED";
        case OrderStatus::Filled:
            return "FILLED";
        case OrderStatus::Cancelled:
            return "CANCELLED";
        case OrderStatus::WaitingForTrigger:
            return "WAITING_FOR_TRIGGER";
        }
        return "UNKNOWN";
    }

    double Order::roundPrice(double price)
    {
        return std::round(price * 100.0) / 100.0;
    }

    bool Order::isStopType(OrderType type)
    {
        return type == OrderType::Stop || type == OrderType::StopLoss ||
               type == OrderType::TakeProfit || type == OrderType::StopLimit;
    }

    bool Order::triggerConditionMet(OrderType type, OrderSide side, double stopPrice, double marketPrice)
    {
        if (!isStopType(type))
        {
            return false;
        }
        return triggersOnRise(type, side) ? (marketPrice >= stopPrice) : (marketPrice <= stopPrice);
    }

    Order::Order(OrderId id, std::string participant, std::string symbol, OrderSide side,
                 OrderType type, int quantity, double limitPrice, std::time_t timestamp,
                 double stopPrice)
        : id_(id), participant_(std::move(participant)), symbol_(std::move(symbol)), side_(side),
          type_(type), quantity_(quantity), filledQuantity_(0), limitPrice_(0.0), stopPrice_(0.0),
          status_(OrderStatus::New), triggered_(false), timestamp_(timestamp)
    {
        if (quantity_ <= 0)
        {
            throw ExecutionError("Order quantity must be positive");
        }
        if (symbol_.empty())
        {
            throw ExecutionError("Order symbol cannot be empty");
        }
        if (type_ == OrderType::Limit || type_ == OrderType::StopLimit)
        {
            if (!std::isfinite(limitPrice))
            {
                throw ExecutionError("Limit price must be a finite number");
            }
            limitPrice_ = roundPrice(limitPrice);
            if (!(limitPrice_ > 0.0))
            {
                throw ExecutionError("Limit price must be positive");
            }
        }
        if (isStopType(type_))
        {
            if (!std::isfinite(stopPrice))
            {
                throw ExecutionError("Stop price must be a finite number");
            }
            stopPrice_ = roundPrice(stopPrice);
            if (!(stopPrice_ > 0.0))
            {
                throw ExecutionError("Stop price must be positive");
            }
            status_ = OrderStatus::WaitingForTrigger;
        }
    }

    bool Order::triggersAt(double marketPrice) const
    {
        return isPending() && triggerConditionMet(type_, side_, stopPrice_, marketPrice);
    }

    std::string Order::triggerDescription() const
    {
        if (!isStop())
        {
            return "";
        }
        std::ostringstream os;
        os << std::fixed << std::setprecision(2);
        os << "market price " << (triggersOnRise(type_, side_) ? ">= " : "<= ") << "$" << stopPrice_;
        return os.str();
    }

    void Order::activate()
    {
        if (!isPending())
        {
            throw ExecutionError("Only an order waiting for its trigger can be activated");
        }
        triggered_ = true;
        status_ = OrderStatus::New;
    }

    void Order::applyFill(int qty)
    {
        if (!isActive())
        {
            throw ExecutionError("Cannot fill an order that is not active");
        }
        if (qty <= 0 || qty > remainingQuantity())
        {
            throw ExecutionError("Invalid fill quantity");
        }
        filledQuantity_ += qty;
        status_ = (remainingQuantity() == 0) ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
    }

    void Order::cancel()
    {
        if (!isOpen())
        {
            return;
        }
        status_ = OrderStatus::Cancelled;
    }

} // namespace execution