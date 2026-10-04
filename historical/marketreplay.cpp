#include "MarketReplay.h"
#include <stdexcept>

namespace historical
{

    MarketReplay::MarketReplay(market::Market &market, broker::Broker &brokerRef, const HistoricalDataSet &data)
        : market_(market), broker_(brokerRef), data_(data) {}

    execution::MarketUpdateResult MarketReplay::step()
    {
        if (!hasNext())
        {
            throw std::out_of_range("MarketReplay::step() called with no remaining historical data");
        }

        const std::size_t index = nextStep_;
        for (const auto &symbol : data_.symbols())
        {
            const HistoricalBar &bar = data_.barAt(symbol, index);
            market_.applyHistoricalBar(symbol, bar.open, bar.high, bar.low, bar.close, bar.volume,
                                       bar.timestamp);
        }
        ++nextStep_;

        return broker_.onMarketUpdate();
    }

    std::vector<execution::MarketUpdateResult> MarketReplay::stepMultiple(std::size_t n)
    {
        std::vector<execution::MarketUpdateResult> results;
        results.reserve(n);
        for (std::size_t i = 0; i < n && hasNext(); ++i)
        {
            results.push_back(step());
        }
        return results;
    }

} // namespace historical