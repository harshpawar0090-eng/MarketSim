#include "Market.h"
#include <stdexcept>
#include <algorithm>
#include <random>

namespace market
{

    std::string sectorToString(Sector sector)
    {
        switch (sector)
        {
        case Sector::Technology:
            return "Technology";
        case Sector::Energy:
            return "Energy";
        case Sector::Finance:
            return "Finance";
        case Sector::Healthcare:
            return "Healthcare";
        case Sector::Consumer:
            return "Consumer";
        case Sector::Industrial:
            return "Industrial";
        case Sector::Telecom:
            return "Telecom";
        case Sector::Materials:
            return "Materials";
        case Sector::Utilities:
            return "Utilities";
        }
        return "Unknown";
    }

    // ----------------------------- MarketClock -----------------------------

    MarketClock::MarketClock() : current_(std::time(nullptr)) {}

    // -------------------------------- Quote ----------------------------------

    Quote::Quote(double price, double previousPrice, double bidPrice, double askPrice,
                 long long volume, double turnover, std::time_t timestamp)
        : price_(price), previousPrice_(previousPrice), bidPrice_(bidPrice), askPrice_(askPrice),
          volume_(volume), turnover_(turnover), timestamp_(timestamp)
    {
        if (price_ < 0.0)
        {
            throw std::invalid_argument("Quote price cannot be negative");
        }
    }

    // ----------------------------- PriceHistory -----------------------------

    void PriceHistory::recordBar(const OhlcBar &bar)
    {
        bars_.push_back(bar);
        if (bars_.size() > kMaxBars)
        {
            bars_.pop_front();
        }
    }

    double PriceHistory::highestPrice() const
    {
        if (bars_.empty())
        {
            return 0.0;
        }
        double highest = bars_.front().high;
        for (const auto &bar : bars_)
        {
            highest = std::max(highest, bar.high);
        }
        return highest;
    }

    double PriceHistory::lowestPrice() const
    {
        if (bars_.empty())
        {
            return 0.0;
        }
        double lowest = bars_.front().low;
        for (const auto &bar : bars_)
        {
            lowest = std::min(lowest, bar.low);
        }
        return lowest;
    }

    // -------------------------------- Stock -----------------------------------

    Stock::Stock(std::string symbol, std::string name, Sector sector, double price)
        : symbol_(std::move(symbol)), name_(std::move(name)), sector_(sector), price_(price),
          previousPrice_(price), change_(0.0), percentChange_(0.0),
          volume_(0), turnover_(0.0), timestamp_(0)
    {
        if (price_ < 0.0)
        {
            throw std::invalid_argument("Stock price cannot be negative: " + symbol_);
        }
    }

    Stock::Stock(const Instrument &instrument, const Quote &quote)
        : symbol_(instrument.symbol()), name_(instrument.name()), sector_(instrument.sector()),
          price_(quote.price()), previousPrice_(quote.previousPrice()),
          change_(quote.change()), percentChange_(quote.percentChange()),
          volume_(quote.volume()), turnover_(quote.turnover()), timestamp_(quote.timestamp()) {}

    // -------------------------------- Market -----------------------------------

    Market::Market(double priceVolatility) : priceEngine_(priceVolatility) {}

    void Market::recordInitialBar(const std::string &symbol, double price, std::time_t timestamp)
    {
        OhlcBar bar{price, price, price, price, 0, timestamp};
        history_[symbol].recordBar(bar);
    }

    void Market::addStock(Stock stock)
    {
        std::string symbol = stock.symbol();
        std::time_t now = clock_.now();

        Instrument instrument(stock.symbol(), stock.name(), stock.sector());
        // Bid/ask start equal to the seed price and volume/turnover start at
        // zero. The Execution module publishes the real best bid/ask later
        // (setTopOfBook) and adds executed volume/turnover (recordExecution).
        Quote quote(stock.price(), stock.price(), stock.price(), stock.price(), 0, 0.0, now);

        instruments_.insert_or_assign(symbol, std::move(instrument));
        quotes_.insert_or_assign(symbol, std::move(quote));
        executedVolume_.insert_or_assign(symbol, 0LL);
        executedTurnover_.insert_or_assign(symbol, 0.0);
        recordInitialBar(symbol, stock.price(), now);
    }

    bool Market::exists(const std::string &symbol) const
    {
        return instruments_.find(symbol) != instruments_.end();
    }

    const Instrument &Market::getInstrument(const std::string &symbol) const
    {
        auto it = instruments_.find(symbol);
        if (it == instruments_.end())
        {
            throw std::out_of_range("Stock symbol not found in market: " + symbol);
        }
        return it->second;
    }

    const Quote &Market::getQuote(const std::string &symbol) const
    {
        auto it = quotes_.find(symbol);
        if (it == quotes_.end())
        {
            throw std::out_of_range("Stock symbol not found in market: " + symbol);
        }
        return it->second;
    }

    Stock Market::getStock(const std::string &symbol) const
    {
        return Stock(getInstrument(symbol), getQuote(symbol));
    }

    std::vector<std::string> Market::listSymbols() const
    {
        std::vector<std::string> symbols;
        symbols.reserve(instruments_.size());
        for (const auto &[symbol, instrument] : instruments_)
        {
            symbols.push_back(symbol);
        }
        return symbols;
    }

    std::vector<const Instrument *> Market::instrumentsBySector(Sector sector) const
    {
        std::vector<const Instrument *> result;
        for (const auto &[symbol, instrument] : instruments_)
        {
            if (instrument.sector() == sector)
            {
                result.push_back(&instrument);
            }
        }
        return result;
    }

    std::map<std::string, Stock> Market::allStocks() const
    {
        std::map<std::string, Stock> result;
        for (const auto &[symbol, instrument] : instruments_)
        {
            result.emplace(symbol, Stock(instrument, quotes_.at(symbol)));
        }
        return result;
    }

    void Market::setPrice(const std::string &symbol, double newPrice)
    {
        if (newPrice < 0.0)
        {
            throw std::invalid_argument("Stock price cannot be negative: " + symbol);
        }
        auto it = quotes_.find(symbol);
        if (it == quotes_.end())
        {
            throw std::out_of_range("Stock symbol not found in market: " + symbol);
        }

        double oldPrice = it->second.price();
        std::time_t now = clock_.now();
        Quote updated(newPrice, oldPrice, newPrice, newPrice,
                      it->second.volume(), it->second.turnover(), now);
        it->second = updated;

        OhlcBar bar{oldPrice, std::max(oldPrice, newPrice), std::min(oldPrice, newPrice), newPrice, 0, now};
        history_[symbol].recordBar(bar);
    }

    const std::deque<OhlcBar> &Market::priceHistory(const std::string &symbol) const
    {
        getInstrument(symbol); // throws if the symbol does not exist
        return history_.at(symbol).bars();
    }

    double Market::highestPrice(const std::string &symbol) const
    {
        getInstrument(symbol);
        return history_.at(symbol).highestPrice();
    }

    double Market::lowestPrice(const std::string &symbol) const
    {
        getInstrument(symbol);
        return history_.at(symbol).lowestPrice();
    }

    void Market::tick()
    {
        clock_.advance(60); // one simulated minute per tick (simple, documented placeholder)
        std::time_t now = clock_.now();

        // Simulated background trading activity. Real executed volume now
        // comes from the Execution module via recordExecution(); this
        // placeholder is kept so Phase 2 behavior (volume/turnover/MarketSim 50
        // statistics moving on every tick) is preserved. It is added to the
        // quote's cumulative volume but NOT to the executed-only counters.
        static std::mt19937 volumeRng(std::random_device{}());
        static std::uniform_int_distribution<long long> volumeDist(100, 5000);

        for (auto &[symbol, quote] : quotes_)
        {
            double oldPrice = quote.price();
            double newPrice = priceEngine_.nextPrice(oldPrice);
            long long volumeDelta = volumeDist(volumeRng);
            long long newVolume = quote.volume() + volumeDelta;
            double newTurnover = quote.turnover() + newPrice * static_cast<double>(volumeDelta);

            quote = Quote(newPrice, oldPrice, newPrice, newPrice, newVolume, newTurnover, now);

            OhlcBar bar{oldPrice, std::max(oldPrice, newPrice), std::min(oldPrice, newPrice),
                        newPrice, volumeDelta, now};
            history_[symbol].recordBar(bar);
        }
    }

    // ------------------------- Phase 3: execution feedback ----------------------

    void Market::recordExecution(const std::string &symbol, int quantity, double price)
    {
        if (quantity <= 0)
        {
            throw std::invalid_argument("Executed quantity must be positive: " + symbol);
        }
        if (price < 0.0)
        {
            throw std::invalid_argument("Executed price cannot be negative: " + symbol);
        }
        auto it = quotes_.find(symbol);
        if (it == quotes_.end())
        {
            throw std::out_of_range("Stock symbol not found in market: " + symbol);
        }

        const double value = price * static_cast<double>(quantity);
        const Quote &q = it->second;
        Quote updated(q.price(), q.previousPrice(), q.bidPrice(), q.askPrice(),
                      q.volume() + quantity, q.turnover() + value, q.timestamp());
        it->second = updated;

        executedVolume_[symbol] += quantity;
        executedTurnover_[symbol] += value;
    }

    void Market::setTopOfBook(const std::string &symbol, double bidPrice, double askPrice)
    {
        if (bidPrice < 0.0 || askPrice < 0.0)
        {
            throw std::invalid_argument("Bid/ask cannot be negative: " + symbol);
        }
        auto it = quotes_.find(symbol);
        if (it == quotes_.end())
        {
            throw std::out_of_range("Stock symbol not found in market: " + symbol);
        }

        const Quote &q = it->second;
        Quote updated(q.price(), q.previousPrice(), bidPrice, askPrice,
                      q.volume(), q.turnover(), q.timestamp());
        it->second = updated;
    }

    long long Market::executedVolume(const std::string &symbol) const
    {
        getInstrument(symbol); // throws if the symbol does not exist
        auto it = executedVolume_.find(symbol);
        return it == executedVolume_.end() ? 0LL : it->second;
    }

    double Market::executedTurnover(const std::string &symbol) const
    {
        getInstrument(symbol);
        auto it = executedTurnover_.find(symbol);
        return it == executedTurnover_.end() ? 0.0 : it->second;
    }

    // ------------------------- Phase 4: historical replay ----------------------

    void Market::applyHistoricalBar(const std::string &symbol, double open, double high, double low,
                                    double close, long long volume, std::time_t timestamp)
    {
        if (close < 0.0)
        {
            throw std::invalid_argument("Historical close price cannot be negative: " + symbol);
        }
        auto it = quotes_.find(symbol);
        if (it == quotes_.end())
        {
            throw std::out_of_range("Stock symbol not found in market: " + symbol);
        }

        const double oldPrice = it->second.price();
        const long long newVolume = it->second.volume() + volume;
        const double newTurnover = it->second.turnover() + close * static_cast<double>(volume);

        Quote updated(close, oldPrice, close, close, newVolume, newTurnover, timestamp);
        it->second = updated;

        OhlcBar bar{open, high, low, close, volume, timestamp};
        history_[symbol].recordBar(bar);
    }

} // namespace market