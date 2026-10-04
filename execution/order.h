#ifndef MARKETSIM_EXECUTION_ORDER_H
#define MARKETSIM_EXECUTION_ORDER_H

#include <ctime>
#include <stdexcept>
#include <string>

// Execution module: Order model, Fill records, order book, matching engine
// and the ExecutionService that settles fills into the Portfolio. Kept in its
// own namespace so the legacy trading::Order / BuyOrder / SellOrder classes
// keep working unchanged.
namespace execution
{

    using OrderId = unsigned long long;

    // Participant identifiers. There is one real user; the market maker is a
    // simple liquidity source so the user's orders have a counterparty.
    inline constexpr const char *kUserId = "USER";
    inline constexpr const char *kMarketMakerId = "MARKET_MAKER";

    enum class OrderSide
    {
        Buy,
        Sell
    };

    // Market, Limit      - executable immediately.
    // Stop               - waits for a trigger price, then becomes a Market order.
    // StopLoss           - protective stop (SELL only), same trigger rule as Stop.
    // TakeProfit         - profit target (SELL only): triggers when the price
    //                      RISES to the trigger, then becomes a Market order.
    // StopLimit          - waits for a trigger price, then becomes a Limit order.
    //
    // Trigger rule (stopPrice vs the market price):
    //   Stop / StopLoss / StopLimit : BUY triggers when price >= stop,
    //                                 SELL triggers when price <= stop.
    //   TakeProfit                  : BUY triggers when price <= trigger,
    //                                 SELL triggers when price >= trigger.
    enum class OrderType
    {
        Market,
        Limit,
        Stop,
        StopLoss,
        TakeProfit,
        StopLimit
    };

    // WaitingForTrigger - stop-type order waiting for its trigger. It is NOT in
    //                     the order book. (Lifecycle: WaitingForTrigger ->
    //                     [trigger] -> New -> PartiallyFilled -> Filled, or
    //                     Cancelled at any point before Filled.)
    // New               - accepted and executable, nothing filled yet (resting,
    //                     or about to be matched).
    // PartiallyFilled   - some quantity filled, remainder still resting.
    // Filled            - fully filled.
    // Cancelled         - no longer active. filledQuantity() may still be > 0:
    //                     that happens when a Market order exhausts the book
    //                     (remainder cancelled) or a partially filled order is
    //                     cancelled. Fills already made stay valid.
    enum class OrderStatus
    {
        New,
        PartiallyFilled,
        Filled,
        Cancelled,
        WaitingForTrigger
    };

    std::string orderSideToString(OrderSide side);
    std::string orderTypeToString(OrderType type);
    std::string orderStatusToString(OrderStatus status);

    // Base type for all execution errors.
    class ExecutionError : public std::runtime_error
    {
    public:
        explicit ExecutionError(const std::string &message) : std::runtime_error(message) {}
    };

    // Thrown by ExecutionService when an order fails validation (bad input,
    // unknown symbol, insufficient cash/shares). Nothing is mutated.
    class OrderRejectedError : public ExecutionError
    {
    public:
        explicit OrderRejectedError(const std::string &message) : ExecutionError(message) {}
    };

    // An instruction to buy or sell. An Order is NOT a Trade/Fill and NOT a
    // Position: it only tracks its own lifecycle (quantity, filled quantity,
    // status). A stop-type order keeps the same id and type label after it
    // triggers; triggered() tells the two phases apart.
    class Order
    {
    public:
        // limitPrice is used by Limit and StopLimit orders (rounded to cents so
        // price levels compare exactly) and ignored (stored as 0) otherwise.
        // stopPrice is required (> 0) for Stop/StopLoss/TakeProfit/StopLimit and
        // ignored otherwise. Stop-type orders start as WaitingForTrigger.
        Order(OrderId id, std::string participant, std::string symbol, OrderSide side,
              OrderType type, int quantity, double limitPrice, std::time_t timestamp,
              double stopPrice = 0.0);

        OrderId id() const { return id_; }
        const std::string &participant() const { return participant_; }
        const std::string &symbol() const { return symbol_; }
        OrderSide side() const { return side_; }
        OrderType type() const { return type_; }
        int quantity() const { return quantity_; }
        int filledQuantity() const { return filledQuantity_; }
        int remainingQuantity() const { return quantity_ - filledQuantity_; }
        double limitPrice() const { return limitPrice_; }
        double stopPrice() const { return stopPrice_; }
        OrderStatus status() const { return status_; }
        std::time_t timestamp() const { return timestamp_; }

        bool isBuy() const { return side_ == OrderSide::Buy; }

        // Executable and not finished (New / PartiallyFilled).
        bool isActive() const
        {
            return status_ == OrderStatus::New || status_ == OrderStatus::PartiallyFilled;
        }
        // Stop-type order still waiting for its trigger.
        bool isPending() const { return status_ == OrderStatus::WaitingForTrigger; }
        bool isOpen() const { return isActive() || isPending(); }

        bool isStop() const { return isStopType(type_); }
        bool triggered() const { return triggered_; }

        // The executable type this order uses once active: Limit for Limit and
        // StopLimit, Market for everything else.
        OrderType executionType() const
        {
            return (type_ == OrderType::Limit || type_ == OrderType::StopLimit) ? OrderType::Limit
                                                                                : OrderType::Market;
        }

        // True if a waiting stop-type order should trigger at marketPrice.
        bool triggersAt(double marketPrice) const;

        // Human-readable trigger condition, e.g. "market price <= $145.00"
        // (empty for non-stop orders).
        std::string triggerDescription() const;

        // WaitingForTrigger -> New. Throws ExecutionError otherwise.
        void activate();

        // Records a fill of qty shares. Throws ExecutionError if the order is
        // not active or qty is not within (0, remaining].
        void applyFill(int qty);

        // Cancels an active or pending order (no-op if it is already finished).
        void cancel();

        static double roundPrice(double price);
        static bool isStopType(OrderType type);
        static bool triggerConditionMet(OrderType type, OrderSide side, double stopPrice,
                                        double marketPrice);

    private:
        OrderId id_;
        std::string participant_;
        std::string symbol_;
        OrderSide side_;
        OrderType type_;
        int quantity_;
        int filledQuantity_;
        double limitPrice_;
        double stopPrice_;
        OrderStatus status_;
        bool triggered_;
        std::time_t timestamp_;
    };

    // One execution: a match between a buy order and a sell order at a price.
    // Produced only by the MatchingEngine; consumed by the Portfolio (via
    // ExecutionService) and by market volume/turnover statistics.
    struct Fill
    {
        unsigned long long fillId;
        std::string symbol;
        double price;
        int quantity;
        OrderId buyOrderId;
        OrderId sellOrderId;
        std::string buyParticipant;
        std::string sellParticipant;
        OrderSide aggressorSide; // side of the incoming order that triggered the match
        std::time_t timestamp;

        double value() const { return price * quantity; }
    };

    // "Trade" and "Fill" are the same record in this model.
    using Trade = Fill;

} // namespace execution

#endif // MARKETSIM_EXECUTION_ORDER_H