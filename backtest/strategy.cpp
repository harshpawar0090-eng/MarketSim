#include "Strategy.h"
#include <numeric>

namespace backtest
{

    MarketView::MarketView(const market::Market &market, const portfolio::Portfolio &portfolio,
                           const historical::HistoricalDataSet &data, std::size_t stepIndex)
        : market_(market), portfolio_(portfolio), data_(data), stepIndex_(stepIndex) {}

    std::vector<historical::HistoricalBar> MarketView::history(const std::string &symbol) const
    {
        std::vector<historical::HistoricalBar> result;
        if (!data_.hasSymbol(symbol))
        {
            return result;
        }
        const historical::HistoricalSeries &s = data_.series(symbol);
        const std::size_t last = std::min(stepIndex_, s.size() == 0 ? 0 : s.size() - 1);
        result.reserve(last + 1);
        for (std::size_t i = 0; i <= last && i < s.size(); ++i)
        {
            result.push_back(s.at(i));
        }
        return result;
    }

    BuyAndHoldStrategy::BuyAndHoldStrategy(std::string symbol, int quantity)
        : symbol_(std::move(symbol)), quantity_(quantity) {}

    std::string BuyAndHoldStrategy::name() const { return "Buy & Hold (" + symbol_ + ")"; }

    std::vector<OrderIntent> BuyAndHoldStrategy::onStep(const MarketView &view)
    {
        if (view.portfolio().holdingQuantity(symbol_) == 0)
        {
            return {OrderIntent{symbol_, execution::OrderSide::Buy, quantity_, 0.0}};
        }
        return {};
    }

    MovingAverageCrossStrategy::MovingAverageCrossStrategy(std::string symbol, int shortWindow, int longWindow,
                                                           int quantity)
        : symbol_(std::move(symbol)), shortWindow_(shortWindow), longWindow_(longWindow), quantity_(quantity) {}

    std::string MovingAverageCrossStrategy::name() const
    {
        return "MA Cross " + std::to_string(shortWindow_) + "/" + std::to_string(longWindow_) + " (" + symbol_ + ")";
    }

    namespace
    {
        double averageOfLastN(const std::vector<historical::HistoricalBar> &bars, int n)
        {
            double sum = 0.0;
            for (std::size_t i = bars.size() - static_cast<std::size_t>(n); i < bars.size(); ++i)
            {
                sum += bars[i].close;
            }
            return sum / n;
        }
    } // namespace

    std::vector<OrderIntent> MovingAverageCrossStrategy::onStep(const MarketView &view)
    {
        std::vector<historical::HistoricalBar> bars = view.history(symbol_);
        if (static_cast<int>(bars.size()) < longWindow_)
        {
            return {}; // not enough history yet to compute the long average
        }

        const double shortMA = averageOfLastN(bars, shortWindow_);
        const double longMA = averageOfLastN(bars, longWindow_);
        const bool bullish = shortMA > longMA;
        const int owned = view.portfolio().holdingQuantity(symbol_);

        if (bullish && owned == 0)
        {
            return {OrderIntent{symbol_, execution::OrderSide::Buy, quantity_, 0.0}};
        }
        if (!bullish && owned > 0)
        {
            return {OrderIntent{symbol_, execution::OrderSide::Sell, owned, 0.0}};
        }
        return {};
    }

} // namespace backtest