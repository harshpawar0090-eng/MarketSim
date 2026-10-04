#include "PriceEngine.h"
#include <algorithm>

namespace market
{

    PriceEngine::PriceEngine(double volatility, unsigned int seed)
        : volatility_(volatility), rng_(seed), moveDist_(-volatility, volatility) {}

    double PriceEngine::nextPrice(double currentPrice)
    {
        double movePercent = moveDist_(rng_); // e.g. -0.02 .. +0.02
        double newPrice = currentPrice * (1.0 + movePercent);
        return std::max(newPrice, 0.01); // never let price hit zero/negative
    }

} // namespace market