#ifndef MARKETSIM_TRADING_TRADING_H
#define MARKETSIM_TRADING_TRADING_H

#include <string>
#include <stdexcept>
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"

// Trading module: represents the act of placing and executing an order
// against the real Market and Portfolio. This is the only module that is
// allowed to mutate Portfolio (through Portfolio's own public API).
namespace trading
{

    // Base type for all trading errors so the UI can catch one type if it
    // wants a generic handler, or catch the specific subtypes for detail.
    class TradingError : public std::runtime_error
    {
    public:
        explicit TradingError(const std::string &message);
    };

    class InsufficientFundsError : public TradingError
    {
    public:
        explicit InsufficientFundsError(const std::string &message);
    };

    class InsufficientSharesError : public TradingError
    {
    public:
        explicit InsufficientSharesError(const std::string &message);
    };

    class InvalidOrderError : public TradingError
    {
    public:
        explicit InvalidOrderError(const std::string &message);
    };

    // Abstract base for a trading order. Real behavioral variance between a
    // buy and a sell (what "valid" means, what "execute" does) is exactly the
    // kind of case where runtime polymorphism earns its place.
    class Order
    {
    public:
        Order(std::string symbol, int quantity);
        virtual ~Order() = default;

        // Throws a TradingError subtype if the order cannot legally be executed.
        virtual void validate(const portfolio::Portfolio &portfolio, const market::Market &mkt) const = 0;

        // Validates internally, then mutates the portfolio and returns the
        // resulting Transaction record (which the caller should also record).
        virtual portfolio::Transaction execute(portfolio::Portfolio &portfolioRef, const market::Market &mkt) = 0;

        const std::string &symbol() const;
        int quantity() const;

    protected:
        std::string symbol_;
        int quantity_;
    };

    class BuyOrder : public Order
    {
    public:
        BuyOrder(std::string symbol, int quantity);

        void validate(const portfolio::Portfolio &portfolio, const market::Market &mkt) const override;
        portfolio::Transaction execute(portfolio::Portfolio &portfolioRef, const market::Market &mkt) override;
    };

    class SellOrder : public Order
    {
    public:
        SellOrder(std::string symbol, int quantity);

        void validate(const portfolio::Portfolio &portfolio, const market::Market &mkt) const override;
        portfolio::Transaction execute(portfolio::Portfolio &portfolioRef, const market::Market &mkt) override;
    };

} // namespace trading

#endif // MARKETSIM_TRADING_TRADING_H