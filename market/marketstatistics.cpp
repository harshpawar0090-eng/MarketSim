#include "MarketStatistics.h"
#include <algorithm>

namespace market
{

    int MarketStatistics::advancingCount(const Market &market)
    {
        int count = 0;
        for (const auto &symbol : market.listSymbols())
        {
            if (market.getQuote(symbol).change() > 0.0)
            {
                ++count;
            }
        }
        return count;
    }

    int MarketStatistics::decliningCount(const Market &market)
    {
        int count = 0;
        for (const auto &symbol : market.listSymbols())
        {
            if (market.getQuote(symbol).change() < 0.0)
            {
                ++count;
            }
        }
        return count;
    }

    int MarketStatistics::unchangedCount(const Market &market)
    {
        int count = 0;
        for (const auto &symbol : market.listSymbols())
        {
            if (market.getQuote(symbol).change() == 0.0)
            {
                ++count;
            }
        }
        return count;
    }

    long long MarketStatistics::totalVolume(const Market &market)
    {
        long long total = 0;
        for (const auto &symbol : market.listSymbols())
        {
            total += market.getQuote(symbol).volume();
        }
        return total;
    }

    double MarketStatistics::totalTurnover(const Market &market)
    {
        double total = 0.0;
        for (const auto &symbol : market.listSymbols())
        {
            total += market.getQuote(symbol).turnover();
        }
        return total;
    }

    std::vector<MoverEntry> MarketStatistics::topGainers(const Market &market, int topN)
    {
        std::vector<MoverEntry> movers;
        for (const auto &symbol : market.listSymbols())
        {
            movers.push_back({symbol, market.getQuote(symbol).percentChange()});
        }
        std::sort(movers.begin(), movers.end(),
                  [](const MoverEntry &a, const MoverEntry &b)
                  { return a.percentChange > b.percentChange; });
        if (static_cast<int>(movers.size()) > topN)
        {
            movers.resize(topN);
        }
        return movers;
    }

    std::vector<MoverEntry> MarketStatistics::topLosers(const Market &market, int topN)
    {
        std::vector<MoverEntry> movers;
        for (const auto &symbol : market.listSymbols())
        {
            movers.push_back({symbol, market.getQuote(symbol).percentChange()});
        }
        std::sort(movers.begin(), movers.end(),
                  [](const MoverEntry &a, const MoverEntry &b)
                  { return a.percentChange < b.percentChange; });
        if (static_cast<int>(movers.size()) > topN)
        {
            movers.resize(topN);
        }
        return movers;
    }

    std::vector<ActivityEntry> MarketStatistics::mostActiveByVolume(const Market &market, int topN)
    {
        std::vector<ActivityEntry> active;
        for (const auto &symbol : market.listSymbols())
        {
            const Quote &q = market.getQuote(symbol);
            active.push_back({symbol, q.volume(), q.turnover()});
        }
        std::sort(active.begin(), active.end(),
                  [](const ActivityEntry &a, const ActivityEntry &b)
                  { return a.volume > b.volume; });
        if (static_cast<int>(active.size()) > topN)
        {
            active.resize(topN);
        }
        return active;
    }

    std::vector<ActivityEntry> MarketStatistics::mostActiveByTurnover(const Market &market, int topN)
    {
        std::vector<ActivityEntry> active;
        for (const auto &symbol : market.listSymbols())
        {
            const Quote &q = market.getQuote(symbol);
            active.push_back({symbol, q.volume(), q.turnover()});
        }
        std::sort(active.begin(), active.end(),
                  [](const ActivityEntry &a, const ActivityEntry &b)
                  { return a.turnover > b.turnover; });
        if (static_cast<int>(active.size()) > topN)
        {
            active.resize(topN);
        }
        return active;
    }

} // namespace market