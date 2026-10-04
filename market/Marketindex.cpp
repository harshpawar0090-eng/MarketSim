#include "MarketIndex.h"
#include <stdexcept>

namespace market
{

    MarketIndex::MarketIndex(std::string name, std::vector<std::string> basketSymbols, double baseValue)
        : name_(std::move(name)), basketSymbols_(std::move(basketSymbols)), baseValue_(baseValue),
          baseTotalPrice_(0.0), currentValue_(baseValue), previousValue_(baseValue)
    {
        if (basketSymbols_.empty())
        {
            throw std::invalid_argument("MarketIndex basket cannot be empty");
        }
    }

    double MarketIndex::basketTotalPrice(const Market &market) const
    {
        double total = 0.0;
        for (const auto &symbol : basketSymbols_)
        {
            total += market.getQuote(symbol).price(); // throws if symbol is not in the market
        }
        return total;
    }

    void MarketIndex::initialize(const Market &market)
    {
        basePrices_.clear();
        for (const auto &symbol : basketSymbols_)
        {
            basePrices_[symbol] = market.getQuote(symbol).price();
        }
        baseTotalPrice_ = basketTotalPrice(market);
        currentValue_ = baseValue_;
        previousValue_ = baseValue_;
    }

    void MarketIndex::update(const Market &market)
    {
        previousValue_ = currentValue_;
        if (baseTotalPrice_ > 0.0)
        {
            currentValue_ = baseValue_ * (basketTotalPrice(market) / baseTotalPrice_);
        }
    }

} // namespace market