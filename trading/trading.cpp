#include "Trading.h"

namespace trading
{

    TradingError::TradingError(const std::string &message) : std::runtime_error(message) {}
    InsufficientFundsError::InsufficientFundsError(const std::string &message) : TradingError(message) {}
    InsufficientSharesError::InsufficientSharesError(const std::string &message) : TradingError(message) {}
    InvalidOrderError::InvalidOrderError(const std::string &message) : TradingError(message) {}

    Order::Order(std::string symbol, int quantity)
        : symbol_(std::move(symbol)), quantity_(quantity) {}

    const std::string &Order::symbol() const { return symbol_; }
    int Order::quantity() const { return quantity_; }

    BuyOrder::BuyOrder(std::string symbol, int quantity) : Order(std::move(symbol), quantity) {}

    void BuyOrder::validate(const portfolio::Portfolio &portfolioRef, const market::Market &mkt) const
    {
        if (quantity_ <= 0)
        {
            throw InvalidOrderError("Buy quantity must be positive");
        }
        if (!mkt.exists(symbol_))
        {
            throw InvalidOrderError("Symbol not found in market: " + symbol_);
        }

        double price = mkt.getStock(symbol_).price();
        double cost = price * quantity_;
        if (cost > portfolioRef.cash())
        {
            throw InsufficientFundsError(
                "Insufficient cash to buy " + std::to_string(quantity_) + " shares of " + symbol_);
        }
    }

    portfolio::Transaction BuyOrder::execute(portfolio::Portfolio &portfolioRef, const market::Market &mkt)
    {
        validate(portfolioRef, mkt);

        double price = mkt.getStock(symbol_).price();
        portfolioRef.applyBuy(symbol_, quantity_, price);

        portfolio::Transaction t(symbol_, portfolio::TransactionSide::Buy, quantity_, price, 0.0);
        portfolioRef.recordTransaction(t);
        return t;
    }

    SellOrder::SellOrder(std::string symbol, int quantity) : Order(std::move(symbol), quantity) {}

    void SellOrder::validate(const portfolio::Portfolio &portfolioRef, const market::Market &mkt) const
    {
        if (quantity_ <= 0)
        {
            throw InvalidOrderError("Sell quantity must be positive");
        }
        if (!mkt.exists(symbol_))
        {
            throw InvalidOrderError("Symbol not found in market: " + symbol_);
        }
        if (portfolioRef.holdingQuantity(symbol_) < quantity_)
        {
            throw InsufficientSharesError(
                "Insufficient shares to sell " + std::to_string(quantity_) + " of " + symbol_);
        }
    }

    portfolio::Transaction SellOrder::execute(portfolio::Portfolio &portfolioRef, const market::Market &mkt)
    {
        validate(portfolioRef, mkt);

        double price = mkt.getStock(symbol_).price();
        double avgCost = portfolioRef.holdings().at(symbol_).avgCost();
        double realizedPnL = (price - avgCost) * quantity_;

        portfolioRef.applySell(symbol_, quantity_, price);

        portfolio::Transaction t(symbol_, portfolio::TransactionSide::Sell, quantity_, price, realizedPnL);
        portfolioRef.recordTransaction(t);
        return t;
    }

} // namespace trading