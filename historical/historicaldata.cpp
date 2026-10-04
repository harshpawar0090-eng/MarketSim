#include "HistoricalData.h"
#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include "../Market/PriceEngine.h"

namespace historical
{

    HistoricalSeries::HistoricalSeries(std::string symbol) : symbol_(std::move(symbol)) {}

    void HistoricalSeries::addBar(const HistoricalBar &bar)
    {
        if (bar.symbol != symbol_)
        {
            throw HistoricalDataError("Bar symbol '" + bar.symbol + "' does not match series symbol '" +
                                      symbol_ + "'");
        }
        if (bar.high < bar.low)
        {
            throw HistoricalDataError("Bar high cannot be less than low for " + symbol_);
        }
        if (bar.open < 0.0 || bar.high < 0.0 || bar.low < 0.0 || bar.close < 0.0)
        {
            throw HistoricalDataError("Bar prices cannot be negative for " + symbol_);
        }
        if (bar.volume < 0)
        {
            throw HistoricalDataError("Bar volume cannot be negative for " + symbol_);
        }
        if (!bars_.empty() && bar.timestamp <= bars_.back().timestamp)
        {
            throw HistoricalDataError("Bar timestamps must be strictly increasing for " + symbol_);
        }
        bars_.push_back(bar);
    }

    const HistoricalBar &HistoricalSeries::at(std::size_t index) const
    {
        if (index >= bars_.size())
        {
            throw HistoricalDataError("Bar index out of range for " + symbol_);
        }
        return bars_[index];
    }

    void HistoricalDataSet::addSeries(HistoricalSeries series)
    {
        if (series.empty())
        {
            throw HistoricalDataError("Cannot add an empty series to a HistoricalDataSet: " + series.symbol());
        }
        if (series_.count(series.symbol()) > 0)
        {
            throw HistoricalDataError("Symbol already present in HistoricalDataSet: " + series.symbol());
        }
        if (!series_.empty() && series.size() != stepCount_)
        {
            throw HistoricalDataError("Series for " + series.symbol() + " has " +
                                      std::to_string(series.size()) + " bars but this dataset expects " +
                                      std::to_string(stepCount_) +
                                      " (every symbol must have the same number of steps)");
        }
        if (series_.empty())
        {
            stepCount_ = series.size();
        }
        series_.emplace(series.symbol(), std::move(series));
    }

    const HistoricalSeries &HistoricalDataSet::series(const std::string &symbol) const
    {
        auto it = series_.find(symbol);
        if (it == series_.end())
        {
            throw HistoricalDataError("Symbol not found in HistoricalDataSet: " + symbol);
        }
        return it->second;
    }

    std::vector<std::string> HistoricalDataSet::symbols() const
    {
        std::vector<std::string> result;
        result.reserve(series_.size());
        for (const auto &[symbol, s] : series_)
        {
            result.push_back(symbol);
        }
        return result;
    }

    const HistoricalBar &HistoricalDataSet::barAt(const std::string &symbol, std::size_t index) const
    {
        return series(symbol).at(index);
    }

    HistoricalSeries loadHistoricalSeriesCsv(const std::string &symbol, const std::string &filePath)
    {
        std::ifstream in(filePath);
        if (!in)
        {
            throw HistoricalDataError("Could not open historical data file: " + filePath);
        }

        HistoricalSeries series(symbol);
        std::string line;
        std::getline(in, line); // header, discarded

        int lineNumber = 1;
        while (std::getline(in, line))
        {
            ++lineNumber;
            if (line.empty())
            {
                continue;
            }
            std::stringstream ss(line);
            std::string field;
            std::vector<std::string> fields;
            while (std::getline(ss, field, ','))
            {
                fields.push_back(field);
            }
            if (fields.size() != 6)
            {
                throw HistoricalDataError("Malformed row " + std::to_string(lineNumber) + " in " + filePath);
            }
            try
            {
                HistoricalBar bar;
                bar.symbol = symbol;
                bar.timestamp = static_cast<std::time_t>(std::stoll(fields[0]));
                bar.open = std::stod(fields[1]);
                bar.high = std::stod(fields[2]);
                bar.low = std::stod(fields[3]);
                bar.close = std::stod(fields[4]);
                bar.volume = std::stoll(fields[5]);
                series.addBar(bar);
            }
            catch (const std::invalid_argument &)
            {
                throw HistoricalDataError("Malformed numeric value on row " + std::to_string(lineNumber) +
                                          " in " + filePath);
            }
        }
        return series;
    }

    void saveHistoricalSeriesCsv(const HistoricalSeries &series, const std::string &filePath)
    {
        std::ofstream out(filePath, std::ios::trunc);
        if (!out)
        {
            throw HistoricalDataError("Could not open file for writing: " + filePath);
        }
        out << "timestamp,open,high,low,close,volume\n";
        for (const auto &bar : series.bars())
        {
            out << static_cast<long long>(bar.timestamp) << ',' << bar.open << ',' << bar.high << ','
                << bar.low << ',' << bar.close << ',' << bar.volume << '\n';
        }
    }

    HistoricalSeries generateSyntheticSeries(const std::string &symbol, double startPrice, int barCount,
                                             std::time_t startTime, long barIntervalSeconds,
                                             double volatility, unsigned int seed)
    {
        if (barCount <= 0)
        {
            throw HistoricalDataError("barCount must be positive for synthetic series: " + symbol);
        }

        HistoricalSeries series(symbol);
        market::PriceEngine engine(volatility, seed);
        std::mt19937 volumeRng(seed + 1); // separate stream so volume doesn't correlate with price moves
        std::uniform_int_distribution<long long> volumeDist(1000, 50000);

        double open = startPrice;
        std::time_t t = startTime;
        for (int i = 0; i < barCount; ++i)
        {
            double close = engine.nextPrice(open);
            HistoricalBar bar;
            bar.symbol = symbol;
            bar.timestamp = t;
            bar.open = open;
            bar.high = std::max(open, close);
            bar.low = std::min(open, close);
            bar.close = close;
            bar.volume = volumeDist(volumeRng);
            series.addBar(bar);

            open = close;
            t += barIntervalSeconds;
        }
        return series;
    }

} // namespace historical