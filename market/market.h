#ifndef MARKETSIM_MARKET_MARKET_H
#define MARKETSIM_MARKET_MARKET_H

#include <string>
#include <map>
#include <vector>
#include <deque>
#include <ctime>
#include "PriceEngine.h"

// Market module: static instrument reference data (Instrument), changing
// market data (Quote), OHLC price history (PriceHistory), simulated time
// (MarketClock) and the Market registry that owns all of them, including
// the price-simulation tick. Stock remains a read-only, backward
// compatible view so Trading/Portfolio/Simulation/UI code written against
// Phase 1 keeps compiling unchanged.
namespace market
{

    enum class Sector
    {
        Technology,
        Energy,
        Finance,
        Healthcare,
        Consumer,
        Industrial,
        Telecom,
        Materials,
        Utilities
    };

    std::string sectorToString(Sector sector);

    // ---------------------------------------------------------------------
    // Simulated market time. Nothing in this module calls std::time(nullptr)
    // directly - every "current time" read goes through here. Each tick()
    // advances simulated time by a fixed step (documented as 60 seconds,
    // i.e. one simulated minute per tick); this is a simple placeholder
    // scheme, not a calendar/session model.
    // ---------------------------------------------------------------------
    class MarketClock
    {
    public:
        MarketClock();

        std::time_t now() const { return current_; }
        void advance(long seconds) { current_ += seconds; }

    private:
        std::time_t current_;
    };

    // ---------------------------------------------------------------------
    // Static, rarely-changing information about a tradeable instrument.
    // ---------------------------------------------------------------------
    class Instrument
    {
    public:
        Instrument(std::string symbol, std::string name, Sector sector)
            : symbol_(std::move(symbol)), name_(std::move(name)), sector_(sector) {}

        const std::string &symbol() const { return symbol_; }
        const std::string &name() const { return name_; }
        Sector sector() const { return sector_; }

    private:
        std::string symbol_;
        std::string name_;
        Sector sector_;
    };

    // ---------------------------------------------------------------------
    // Market data that changes over time: current price, previous price
    // (so change/percent-change can be derived), bid/ask, cumulative volume
    // and turnover, and the simulated timestamp of the last update.
    //
    // Phase 3: bid/ask are the order book's best bid/ask once the Execution
    // module has published them (setTopOfBook); otherwise bid == ask == price.
    // Volume/turnover include executed trades (recordExecution) on top of the
    // simulated background activity added by tick().
    // ---------------------------------------------------------------------
    class Quote
    {
    public:
        Quote(double price, double previousPrice, double bidPrice, double askPrice,
              long long volume, double turnover, std::time_t timestamp);

        double price() const { return price_; }
        double previousPrice() const { return previousPrice_; }
        double bidPrice() const { return bidPrice_; }
        double askPrice() const { return askPrice_; }
        long long volume() const { return volume_; }
        double turnover() const { return turnover_; }
        std::time_t timestamp() const { return timestamp_; }

        // Derived figures - not stored, always computed from price/previousPrice.
        double change() const { return price_ - previousPrice_; }
        double percentChange() const
        {
            return previousPrice_ != 0.0 ? (change() / previousPrice_) * 100.0 : 0.0;
        }

    private:
        double price_;
        double previousPrice_;
        double bidPrice_;
        double askPrice_;
        long long volume_;
        double turnover_;
        std::time_t timestamp_;
    };

    // ---------------------------------------------------------------------
    // One completed OHLC bar. In this phase, one bar is recorded per symbol
    // per tick (open = price before the tick, close = price after it) - there
    // is no calendar/session concept yet, so "one bar per tick" is the unit of
    // history. This is intentionally simple but is exactly the shape a future
    // chart or backtest would need.
    // ---------------------------------------------------------------------
    struct OhlcBar
    {
        double open;
        double high;
        double low;
        double close;
        long long volume;
        std::time_t timestamp;
    };

    // ---------------------------------------------------------------------
    // Bounded OHLC history for a single symbol, plus the high/low queries
    // that are naturally answered by scanning that same history. Keeping a
    // bounded deque (oldest bars drop off) avoids unbounded growth in a long
    // running simulation. The same storage is what a future "52-week high/low"
    // would filter by timestamp - no redesign needed, just a narrower scan.
    // ---------------------------------------------------------------------
    class PriceHistory
    {
    public:
        void recordBar(const OhlcBar &bar);

        const std::deque<OhlcBar> &bars() const { return bars_; }
        double highestPrice() const;
        double lowestPrice() const;

    private:
        std::deque<OhlcBar> bars_;
        static constexpr std::size_t kMaxBars = 500;
    };

    // ---------------------------------------------------------------------
    // Backward-compatible view combining an Instrument + Quote into the shape
    // Trading, Portfolio, Simulation and UI already depend on (symbol, name,
    // sector, price), extended with the new dynamic fields (previous price,
    // change, percent change, volume, turnover, timestamp). Existing accessors
    // keep their original meaning, so no other module needs to change.
    // ---------------------------------------------------------------------
    class Stock
    {
    public:
        // Legacy constructor used for seeding (main.cpp). Extra dynamic fields
        // are not meaningful yet at seed time - Market::addStock() builds the
        // real Quote (with proper previousPrice/volume/turnover/timestamp)
        // separately and does not read them from here.
        Stock(std::string symbol, std::string name, Sector sector, double price);

        Stock(const Instrument &instrument, const Quote &quote);

        const std::string &symbol() const { return symbol_; }
        const std::string &name() const { return name_; }
        Sector sector() const { return sector_; }
        double price() const { return price_; }

        double previousPrice() const { return previousPrice_; }
        double change() const { return change_; }
        double percentChange() const { return percentChange_; }
        long long volume() const { return volume_; }
        double turnover() const { return turnover_; }
        std::time_t timestamp() const { return timestamp_; }

    private:
        std::string symbol_;
        std::string name_;
        Sector sector_;
        double price_;
        double previousPrice_;
        double change_;
        double percentChange_;
        long long volume_;
        double turnover_;
        std::time_t timestamp_;
    };

    // ---------------------------------------------------------------------
    // Owns the instrument registry (Instrument + Quote + PriceHistory per
    // symbol), the market clock and the price engine. Still the single source
    // of truth for "what does a share cost right now" - tick() is the one
    // operation that advances the simulation.
    // ---------------------------------------------------------------------
    class Market
    {
    public:
        explicit Market(double priceVolatility = 0.02);

        // ---- Existing API: unchanged signatures/behavior used elsewhere ----
        void addStock(Stock stock);
        bool exists(const std::string &symbol) const;
        Stock getStock(const std::string &symbol) const;
        std::vector<std::string> listSymbols() const;
        std::map<std::string, Stock> allStocks() const;

        // ---- Reference/quote access (Phase 1, still available) --------------
        const Instrument &getInstrument(const std::string &symbol) const;
        const Quote &getQuote(const std::string &symbol) const;
        std::vector<const Instrument *> instrumentsBySector(Sector sector) const;
        void setPrice(const std::string &symbol, double newPrice);
        const MarketClock &clock() const { return clock_; }

        // ---- Phase 2: history / high-low -------------------------------------
        const std::deque<OhlcBar> &priceHistory(const std::string &symbol) const;
        double highestPrice(const std::string &symbol) const;
        double lowestPrice(const std::string &symbol) const;

        // ---- Phase 2: market tick ----------------------------------------------
        // Advances the clock, moves every instrument's price via the price
        // engine, recalculates change/percent-change, simulates a background
        // volume delta, updates cumulative turnover, and records one OHLC bar
        // per symbol. Deliberately does not touch Portfolio or execute any
        // order - callers (e.g. the UI) are responsible for updating an
        // index/stats view afterwards, since those are separate, pluggable
        // concerns. Note: tick() resets bid/ask to the new price; the
        // Execution module re-publishes top-of-book on its next operation.
        void tick();

        // ---- Phase 3: execution feedback -----------------------------------------
        // Called by the Execution module for every real fill. Adds the traded
        // quantity to the quote's cumulative volume and price*quantity to its
        // cumulative turnover, and to the executed-only counters below. Does
        // not change the quote's price (price discovery is still PriceEngine's
        // job in this phase).
        void recordExecution(const std::string &symbol, int quantity, double price);

        // Publishes the order book's best bid/ask on the quote.
        void setTopOfBook(const std::string &symbol, double bidPrice, double askPrice);

        // Volume/turnover that came ONLY from real executed trades (excludes
        // the simulated background volume added by tick()).
        long long executedVolume(const std::string &symbol) const;
        double executedTurnover(const std::string &symbol) const;

        // ---- Phase 4: historical replay -----------------------------------------
        // Applies one externally-sourced OHLCV bar to a symbol: sets the quote's
        // price to close (previousPrice becomes the prior price, so change()/
        // percentChange() stay correct), adds volume to the quote's cumulative
        // volume/turnover (the same placeholder role as tick()'s background
        // volume - NOT counted as "executed" trades; only real fills reach
        // recordExecution()), and records the bar EXACTLY as given, unlike
        // setPrice() which only ever synthesizes a bar from the old/new price.
        // Does not advance MarketClock - the replay driver owns its own
        // historical timestamps. Throws std::out_of_range if the symbol does
        // not exist and std::invalid_argument if close is negative.
        void applyHistoricalBar(const std::string &symbol, double open, double high, double low,
                                double close, long long volume, std::time_t timestamp);

    private:
        void recordInitialBar(const std::string &symbol, double price, std::time_t timestamp);

        MarketClock clock_;
        PriceEngine priceEngine_;
        std::map<std::string, Instrument> instruments_;
        std::map<std::string, Quote> quotes_;
        std::map<std::string, PriceHistory> history_;
        std::map<std::string, long long> executedVolume_;
        std::map<std::string, double> executedTurnover_;
    };

} // namespace market

#endif // MARKETSIM_MARKET_MARKET_H