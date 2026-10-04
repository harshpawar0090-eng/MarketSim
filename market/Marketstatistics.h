#ifndef MARKETSIM_MARKET_MARKETSTATISTICS_H
#define MARKETSIM_MARKET_MARKETSTATISTICS_H

#include <string>
#include <vector>
#include "Market.h"

// MarketStatistics: stateless computations over a Market snapshot, the
// same pattern already used by PortfolioAnalyzer over Portfolio. Every
// method takes a const Market& and returns a fresh result - nothing is
// cached, so results are always as of whatever quotes the Market
// currently holds (e.g. right after a tick()).
namespace market
{

    struct MoverEntry
    {
        std::string symbol;
        double percentChange;
    };

    struct ActivityEntry
    {
        std::string symbol;
        long long volume;
        double turnover;
    };

    class MarketStatistics
    {
    public:
        static int advancingCount(const Market &market);
        static int decliningCount(const Market &market);
        static int unchangedCount(const Market &market);

        static long long totalVolume(const Market &market);
        static double totalTurnover(const Market &market);

        static std::vector<MoverEntry> topGainers(const Market &market, int topN = 5);
        static std::vector<MoverEntry> topLosers(const Market &market, int topN = 5);
        static std::vector<ActivityEntry> mostActiveByVolume(const Market &market, int topN = 5);
        static std::vector<ActivityEntry> mostActiveByTurnover(const Market &market, int topN = 5);
    };

} // namespace market

#endif // MARKETSIM_MARKET_MARKETSTATISTICS_H