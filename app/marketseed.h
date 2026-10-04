#ifndef MARKETSIM_APP_MARKETSEED_H
#define MARKETSIM_APP_MARKETSEED_H

#include "../Market/Market.h"
#include "../Market/MarketIndex.h"

// Application wiring helpers shared by front-ends: the seeded fictional market
// and the "MarketSim 50" index. Pure data setup - no business logic.
namespace app
{

    // ~46 fictional stocks across 9 sectors.
    market::Market createSeededMarket();

    // "MarketSim 50": a representative basket (not literally 50 stocks), initialized from the market.
    market::MarketIndex createMarketIndex(const market::Market &market);

} // namespace app

#endif // MARKETSIM_APP_MARKETSEED_H