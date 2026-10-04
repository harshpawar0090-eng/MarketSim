#ifndef MARKETSIM_HISTORICAL_HISTORICALDATA_H
#define MARKETSIM_HISTORICAL_HISTORICALDATA_H

#include <ctime>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Historical module: a clean representation of historical OHLCV data,
// independent of the live Market/PriceEngine objects used for real-time
// simulation. This is the data MarketReplay feeds into Market, one bar at
// a time. Deliberately simple: a CSV-backed time series per symbol, all
// symbols sharing the same number of steps so a backtest can advance every
// symbol in lockstep by index. A real dataset only needs to be converted
// into this same shape (timestamp,open,high,low,close,volume rows) - no
// other part of the project needs to change for that.
namespace historical
{

    class HistoricalDataError : public std::runtime_error
    {
    public:
        explicit HistoricalDataError(const std::string &message) : std::runtime_error(message) {}
    };

    // One OHLCV bar for one symbol at one point in simulated historical time.
    struct HistoricalBar
    {
        std::string symbol;
        std::time_t timestamp;
        double open;
        double high;
        double low;
        double close;
        long long volume;
    };

    // Ordered (oldest-first) bars for a single symbol. Bars must have
    // strictly increasing timestamps and valid OHLC values - enforced on
    // insertion so nothing downstream has to re-check it.
    class HistoricalSeries
    {
    public:
        explicit HistoricalSeries(std::string symbol);

        const std::string &symbol() const { return symbol_; }
        const std::vector<HistoricalBar> &bars() const { return bars_; }
        std::size_t size() const { return bars_.size(); }
        bool empty() const { return bars_.empty(); }

        // Appends one bar. Throws HistoricalDataError if: the bar's symbol
        // does not match this series, high < low, any price/volume is
        // negative, or its timestamp is not strictly after the previous bar.
        void addBar(const HistoricalBar &bar);

        const HistoricalBar &at(std::size_t index) const;

    private:
        std::string symbol_;
        std::vector<HistoricalBar> bars_;
    };

    // All symbols' series for one backtest/replay. Every series must have
    // the same length (the dataset's "step count") so replaying step i
    // means "the i-th bar of every symbol" - a deliberate simplification
    // (no calendar alignment) documented here rather than hidden.
    class HistoricalDataSet
    {
    public:
        // Throws HistoricalDataError if series is empty, its symbol is
        // already present, or its length does not match series already
        // in the set (the very first series added fixes the step count).
        void addSeries(HistoricalSeries series);

        bool hasSymbol(const std::string &symbol) const { return series_.count(symbol) > 0; }
        // Throws HistoricalDataError if the symbol is not present.
        const HistoricalSeries &series(const std::string &symbol) const;
        std::vector<std::string> symbols() const;

        std::size_t stepCount() const { return stepCount_; }
        // Throws HistoricalDataError if the symbol is missing or index is out of range.
        const HistoricalBar &barAt(const std::string &symbol, std::size_t index) const;

    private:
        std::map<std::string, HistoricalSeries> series_;
        std::size_t stepCount_ = 0;
    };

    // ---- CSV persistence --------------------------------------------------
    // Format: a header line "timestamp,open,high,low,close,volume" followed
    // by one row per bar, oldest first. timestamp is a raw std::time_t
    // (seconds since epoch) so the format has no locale/timezone ambiguity.

    // Throws HistoricalDataError if the file cannot be opened or a row is malformed.
    HistoricalSeries loadHistoricalSeriesCsv(const std::string &symbol, const std::string &filePath);
    // Throws HistoricalDataError if the file cannot be written.
    void saveHistoricalSeriesCsv(const HistoricalSeries &series, const std::string &filePath);

    // ---- Synthetic data generation -----------------------------------------
    // Produces a plausible historical series using the same PriceEngine
    // random-walk model the live Market uses for ticks, so test/demo data
    // does not need a real dataset. barCount bars, barIntervalSeconds apart,
    // starting at startTime with startPrice as the first bar's open.
    HistoricalSeries generateSyntheticSeries(const std::string &symbol, double startPrice, int barCount,
                                             std::time_t startTime, long barIntervalSeconds = 86400,
                                             double volatility = 0.02, unsigned int seed = 1);

} // namespace historical

#endif // MARKETSIM_HISTORICAL_HISTORICALDATA_H