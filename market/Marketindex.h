#ifndef MARKETSIM_MARKET_MARKETINDEX_H
#define MARKETSIM_MARKET_MARKETINDEX_H

#include <string>
#include <vector>
#include <map>
#include "Market.h"

// MarketIndex: a simple, clearly-defined price index over a basket of
// symbols (e.g. "MarketSim 50"). Read-only with respect to Market - it
// only ever observes quotes via const Market&, it never mutates the
// market. It IS stateful with respect to itself: it remembers the
// previous value so change()/percentChange() mean something.
//
// Methodology (documented, not hidden): the index value is the basket's
// total current price divided by the basket's total base price (captured
// at initialize() time), scaled to baseValue. This is a simple
// price-relative, equal-weighted index - it answers "how has this basket
// grown or shrunk, on average, since the index started", without needing
// market-cap or shares-outstanding data that the project does not model
// yet.
namespace market
{

    class MarketIndex
    {
    public:
        MarketIndex(std::string name, std::vector<std::string> basketSymbols, double baseValue = 1000.0);

        // Captures each basket symbol's current price as its base price, and
        // sets the initial index value to baseValue. Call once, after the
        // Market has been seeded with the basket's stocks.
        void initialize(const Market &market);

        // Recomputes the index from the market's current quotes. Call after
        // each Market::tick().
        void update(const Market &market);

        const std::string &name() const { return name_; }
        const std::vector<std::string> &basket() const { return basketSymbols_; }
        double value() const { return currentValue_; }
        double previousValue() const { return previousValue_; }
        double change() const { return currentValue_ - previousValue_; }
        double percentChange() const
        {
            return previousValue_ != 0.0 ? (change() / previousValue_) * 100.0 : 0.0;
        }

    private:
        double basketTotalPrice(const Market &market) const;

        std::string name_;
        std::vector<std::string> basketSymbols_;
        double baseValue_;
        std::map<std::string, double> basePrices_;
        double baseTotalPrice_;
        double currentValue_;
        double previousValue_;
    };

} // namespace market

#endif // MARKETSIM_MARKET_MARKETINDEX_H