#ifndef MARKETSIM_MARKET_MARKET_H
#define MARKETSIM_MARKET_MARKET_H

#include <string>
#include <map>
#include <vector>

// Market module: represents the simulated market data.
// Contains no knowledge of portfolios, orders, or trading rules.
namespace market {

// Sector is pure classification data - no behavior needed, so it's an enum,
// not a class. Adding a full class here would be unjustified complexity.
enum class Sector {
    Technology,
    Energy,
    Finance,
    Healthcare,
    Consumer,
    Industrial
};

// Converts a Sector value into a human-readable string for display purposes.
std::string sectorToString(Sector sector);

// Represents a single tradeable stock: identity, classification, and price.
class Stock {
public:
    Stock(std::string symbol, std::string name, Sector sector, double price);

    const std::string& symbol() const;
    const std::string& name() const;
    Sector sector() const;
    double price() const;

    // Controlled mutation point for price changes (kept intentionally simple
    // for this MVP; a future feature could call this to simulate price drift).
    void setPrice(double newPrice);

private:
    std::string symbol_;
    std::string name_;
    Sector sector_;
    double price_;
};

// Owns and manages the full set of stocks available in the simulation.
// This is the single source of truth for "what does a share cost right now".
class Market {
public:
    Market() = default;

    void addStock(Stock stock);
    bool exists(const std::string& symbol) const;

    // Throws std::out_of_range if the symbol does not exist.
    const Stock& getStock(const std::string& symbol) const;

    std::vector<std::string> listSymbols() const;
    std::vector<const Stock*> stocksBySector(Sector sector) const;
    const std::map<std::string, Stock>& allStocks() const;

private:
    std::map<std::string, Stock> stocks_;
};

} // namespace market

#endif // MARKETSIM_MARKET_MARKET_H