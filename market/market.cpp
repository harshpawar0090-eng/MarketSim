#include "Market.h"
#include <stdexcept>

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
        }
        return "Unknown";
    }

    Stock::Stock(std::string symbol, std::string name, Sector sector, double price)
        : symbol_(std::move(symbol)), name_(std::move(name)), sector_(sector), price_(price)
    {
        if (price_ < 0.0)
        {
            throw std::invalid_argument("Stock price cannot be negative: " + symbol_);
        }
    }

    const std::string &Stock::symbol() const { return symbol_; }
    const std::string &Stock::name() const { return name_; }
    Sector Stock::sector() const { return sector_; }
    double Stock::price() const { return price_; }

    void Stock::setPrice(double newPrice)
    {
        if (newPrice < 0.0)
        {
            throw std::invalid_argument("Stock price cannot be negative: " + symbol_);
        }
        price_ = newPrice;
    }

    void Market::addStock(Stock stock)
    {
        std::string symbol = stock.symbol();
        stocks_.insert_or_assign(symbol, std::move(stock));
    }

    bool Market::exists(const std::string &symbol) const
    {
        return stocks_.find(symbol) != stocks_.end();
    }

    const Stock &Market::getStock(const std::string &symbol) const
    {
        auto it = stocks_.find(symbol);
        if (it == stocks_.end())
        {
            throw std::out_of_range("Stock symbol not found in market: " + symbol);
        }
        return it->second;
    }

    std::vector<std::string> Market::listSymbols() const
    {
        std::vector<std::string> symbols;
        symbols.reserve(stocks_.size());
        for (const auto &[symbol, stock] : stocks_)
        {
            symbols.push_back(symbol);
        }
        return symbols;
    }

    std::vector<const Stock *> Market::stocksBySector(Sector sector) const
    {
        std::vector<const Stock *> result;
        for (const auto &[symbol, stock] : stocks_)
        {
            if (stock.sector() == sector)
            {
                result.push_back(&stock);
            }
        }
        return result;
    }

    const std::map<std::string, Stock> &Market::allStocks() const
    {
        return stocks_;
    }

} // namespace market