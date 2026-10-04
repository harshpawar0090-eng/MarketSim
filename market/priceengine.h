#ifndef MARKETSIM_MARKET_PRICEENGINE_H
#define MARKETSIM_MARKET_PRICEENGINE_H

#include <random>

// PriceEngine: simulates price movement for a single instrument.
//
// Deliberately independent of Market, Portfolio and Trading - it knows
// nothing about symbols, holdings or orders. It only ever takes a price in
// and returns a new price out, using a bounded random percentage move each
// time it is called. This keeps it reusable and easy to test on its own.
namespace market
{

    class PriceEngine
    {
    public:
        // volatility is the maximum fractional move per tick in either
        // direction, e.g. 0.02 means the price can move by up to +/-2% on a
        // single call to nextPrice(). A fixed seed can be supplied for
        // reproducible runs (e.g. in tests); by default a random seed is used.
        explicit PriceEngine(double volatility = 0.02,
                             unsigned int seed = std::random_device{}());

        // Returns a new simulated price derived from currentPrice. The move can
        // be upward or downward. The result is never allowed to fall to zero or
        // below - it is floored at a small positive value.
        double nextPrice(double currentPrice);

        double volatility() const { return volatility_; }

    private:
        double volatility_;
        std::mt19937 rng_;
        std::uniform_real_distribution<double> moveDist_;
    };

} // namespace market

#endif // MARKETSIM_MARKET_PRICEENGINE_H