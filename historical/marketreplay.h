#ifndef MARKETSIM_HISTORICAL_MARKETREPLAY_H
#define MARKETSIM_HISTORICAL_MARKETREPLAY_H

#include <cstddef>
#include <vector>
#include "../Broker/Broker.h"
#include "../Execution/ExecutionService.h"
#include "../Market/Market.h"
#include "HistoricalData.h"

// MarketReplay: advances a Market through a HistoricalDataSet one step at a
// time, applying every symbol's bar for that step and then calling
// Broker::onMarketUpdate() - exactly what live code does after a price
// change (Market::tick()) - so resting stop orders, liquidity re-quoting and
// margin enforcement all run through the SAME path as live trading. This
// class has no knowledge of strategies; it only knows how to play historical
// data forward through the existing architecture. BacktestEngine builds on
// top of it; it is equally usable on its own (e.g. to step through history
// and just watch Market/Broker react).
namespace historical
{

    class MarketReplay
    {
    public:
        // market/brokerRef are driven forward by this replay. They are
        // typically dedicated instances built for replay/backtesting, not
        // the live app's Market/Broker - this class never knows which.
        MarketReplay(market::Market &market, broker::Broker &brokerRef, const HistoricalDataSet &data);

        std::size_t stepCount() const { return data_.stepCount(); }
        // Number of steps already applied (0 before the first step()).
        std::size_t stepsApplied() const { return nextStep_; }
        bool hasNext() const { return nextStep_ < data_.stepCount(); }

        // Applies the next bar (index stepsApplied()) for every symbol in the
        // dataset to Market via Market::applyHistoricalBar(), then calls
        // broker.onMarketUpdate(). Throws std::out_of_range if hasNext() is false.
        execution::MarketUpdateResult step();

        // Calls step() up to n times (fewer if the data runs out first).
        std::vector<execution::MarketUpdateResult> stepMultiple(std::size_t n);

        const HistoricalDataSet &data() const { return data_; }

    private:
        market::Market &market_;
        broker::Broker &broker_;
        const HistoricalDataSet &data_;
        std::size_t nextStep_ = 0;
    };

} // namespace historical

#endif // MARKETSIM_HISTORICAL_MARKETREPLAY_H