#include "Portfolio.h"

namespace portfolio
{
    std::string transactionSideToString(TransactionSide side)
    {
        return side == TransactionSide::Buy ? "BUY" : "SELL";
    }

    Transaction::Transaction(std::string symbol_, TransactionSide side_, int quantity_,
                             double price_, double realizedPnL_)
        : symbol(std::move(symbol_)), side(side_), quantity(quantity_), price(price_),
          realizedPnL(realizedPnL_), timestamp(std::time(nullptr)) {}

    Holding::Holding(std::string symbol, int quantity, double avgCost)
        : symbol_(std::move(symbol)), quantity_(quantity), avgCost_(avgCost)
    {
        if (quantity_ <= 0)
            throw std::invalid_argument("Holding must start with a positive quantity: " + symbol_);
        if (avgCost_ < 0.0)
            throw std::invalid_argument("Holding average cost cannot be negative: " + symbol_);
    }

    void Holding::addShares(int qty, double price)
    {
        if (qty <= 0)
            throw std::invalid_argument("Quantity to add must be positive");
        avgCost_ = (avgCost_ * quantity_ + price * qty) / (quantity_ + qty); // weighted average
        quantity_ += qty;
    }

    void Holding::removeShares(int qty)
    {
        if (qty <= 0)
            throw std::invalid_argument("Quantity to remove must be positive");
        if (qty > quantity_)
            throw std::invalid_argument("Cannot remove more shares than are held: " + symbol_);
        quantity_ -= qty; // average cost does not change on a sale
    }

    Portfolio::Portfolio(double startingCash) : cash_(startingCash)
    {
        if (cash_ < 0.0)
            throw std::invalid_argument("Starting cash cannot be negative");
    }

    void Portfolio::applyBuy(const std::string &symbol, int qty, double price)
    {
        double cost = price * qty;
        if (cost > cash_)
            throw PortfolioError("Insufficient cash for this operation");
        cash_ -= cost;

        auto it = holdings_.find(symbol);
        if (it != holdings_.end())
            it->second.addShares(qty, price);
        else
            holdings_.emplace(symbol, Holding(symbol, qty, price));
    }

    void Portfolio::applySell(const std::string &symbol, int qty, double price)
    {
        auto it = holdings_.find(symbol);
        if (it == holdings_.end())
            throw PortfolioError("No holding found for symbol: " + symbol);

        it->second.removeShares(qty);
        cash_ += price * qty;
        if (it->second.quantity() == 0)
            holdings_.erase(it);
    }

    double PortfolioAnalyzer::totalMarketValue(const Portfolio &p, const market::Market &mkt)
    {
        double total = 0.0;
        for (const auto &[symbol, holding] : p.holdings())
            total += holding.quantity() * mkt.getStock(symbol).price();
        return total;
    }

    double PortfolioAnalyzer::totalUnrealizedPnL(const Portfolio &p, const market::Market &mkt)
    {
        double total = 0.0;
        for (const auto &[symbol, holding] : p.holdings())
            total += (mkt.getStock(symbol).price() - holding.avgCost()) * holding.quantity();
        return total;
    }

    double PortfolioAnalyzer::totalRealizedPnL(const Portfolio &p)
    {
        double total = 0.0;
        for (const auto &t : p.history())
            if (t.side == TransactionSide::Sell)
                total += t.realizedPnL;
        return total;
    }

    double PortfolioAnalyzer::totalPortfolioValue(const Portfolio &p, const market::Market &mkt)
    {
        return p.cash() + totalMarketValue(p, mkt);
    }

    std::map<market::Sector, double> PortfolioAnalyzer::sectorAllocation(const Portfolio &p, const market::Market &mkt)
    {
        std::map<market::Sector, double> valueBySector;
        double total = totalMarketValue(p, mkt);

        for (const auto &[symbol, holding] : p.holdings())
        {
            const market::Stock &stock = mkt.getStock(symbol);
            valueBySector[stock.sector()] += holding.quantity() * stock.price();
        }
        if (total > 0.0)
            for (auto &[sector, value] : valueBySector)
                value = (value / total) * 100.0; // convert to percent of holdings value
        return valueBySector;
    }

    std::vector<HoldingReportLine> PortfolioAnalyzer::holdingsReport(const Portfolio &p, const market::Market &mkt)
    {
        std::vector<HoldingReportLine> lines;
        for (const auto &[symbol, holding] : p.holdings())
        {
            double price = mkt.getStock(symbol).price();
            lines.push_back({symbol, holding.quantity(), holding.avgCost(), price,
                             price * holding.quantity(), (price - holding.avgCost()) * holding.quantity()});
        }
        return lines;
    }

} // namespace portfolio
