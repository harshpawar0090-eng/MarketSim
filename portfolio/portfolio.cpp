#include "Portfolio.h"
#include <algorithm>

namespace portfolio
{
    std::string transactionSideToString(TransactionSide side)
    {
        return side == TransactionSide::Buy ? "BUY" : "SELL";
    }

    std::string positionEffectToString(PositionEffect effect)
    {
        switch (effect)
        {
        case PositionEffect::OpenLong:
            return "BUY";
        case PositionEffect::CloseLong:
            return "SELL";
        case PositionEffect::OpenShort:
            return "SELL SHORT";
        case PositionEffect::CoverShort:
            return "BUY TO COVER";
        }
        return "UNKNOWN";
    }

    Transaction::Transaction(std::string symbol_, TransactionSide side_, int quantity_,
                             double price_, double realizedPnL_)
        : Transaction(std::move(symbol_), side_, quantity_, price_, realizedPnL_,
                      side_ == TransactionSide::Buy ? PositionEffect::OpenLong : PositionEffect::CloseLong) {}

    Transaction::Transaction(std::string symbol_, TransactionSide side_, int quantity_,
                             double price_, double realizedPnL_, PositionEffect effect_)
        : symbol(std::move(symbol_)), side(side_), quantity(quantity_), price(price_),
          realizedPnL(realizedPnL_), timestamp(std::time(nullptr)), effect(effect_) {}

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

    std::vector<Position> Portfolio::positions() const
    {
        std::vector<Position> result;
        result.reserve(holdings_.size() + shorts_.size());
        for (const auto &entry : holdings_)
            result.push_back(Position{entry.first, entry.second.quantity(), entry.second.avgCost()});
        for (const auto &entry : shorts_)
            result.push_back(Position{entry.first, -entry.second.quantity(), entry.second.avgCost()});
        std::sort(result.begin(), result.end(),
                  [](const Position &a, const Position &b)
                  { return a.symbol < b.symbol; });
        return result;
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

    FillResult Portfolio::applyBuyFill(const std::string &symbol, int qty, double price)
    {
        if (qty <= 0)
            throw std::invalid_argument("Fill quantity must be positive");
        if (price < 0.0)
            throw std::invalid_argument("Fill price cannot be negative");

        FillResult result;
        int remaining = qty;

        // 1. Cover an existing short first. Short P&L = (entry - cover price) * shares.
        auto shortIt = shorts_.find(symbol);
        if (shortIt != shorts_.end())
        {
            const int cover = std::min(remaining, shortIt->second.quantity());
            result.realizedPnL += (shortIt->second.avgCost() - price) * cover;
            shortIt->second.removeShares(cover);
            cash_ -= price * cover;
            result.closedQuantity = cover;
            remaining -= cover;
            if (shortIt->second.quantity() == 0)
                shorts_.erase(shortIt);
        }

        // 2. Any remainder opens/increases a long (weighted average cost).
        if (remaining > 0)
        {
            cash_ -= price * remaining;
            auto it = holdings_.find(symbol);
            if (it != holdings_.end())
                it->second.addShares(remaining, price);
            else
                holdings_.emplace(symbol, Holding(symbol, remaining, price));
            result.openedQuantity = remaining;
        }
        return result;
    }

    FillResult Portfolio::applySellFill(const std::string &symbol, int qty, double price)
    {
        if (qty <= 0)
            throw std::invalid_argument("Fill quantity must be positive");
        if (price < 0.0)
            throw std::invalid_argument("Fill price cannot be negative");

        FillResult result;
        int remaining = qty;

        // 1. Close an existing long first. Long P&L = (price - avg cost) * shares (unchanged rule).
        auto it = holdings_.find(symbol);
        if (it != holdings_.end())
        {
            const int close = std::min(remaining, it->second.quantity());
            result.realizedPnL += (price - it->second.avgCost()) * close;
            it->second.removeShares(close);
            cash_ += price * close;
            result.closedQuantity = close;
            remaining -= close;
            if (it->second.quantity() == 0)
                holdings_.erase(it);
        }

        // 2. Any remainder opens/increases a short; sale proceeds are credited to cash.
        if (remaining > 0)
        {
            cash_ += price * remaining;
            auto shortIt = shorts_.find(symbol);
            if (shortIt != shorts_.end())
                shortIt->second.addShares(remaining, price);
            else
                shorts_.emplace(symbol, Holding(symbol, remaining, price));
            result.openedQuantity = remaining;
        }
        return result;
    }

    double PortfolioAnalyzer::totalMarketValue(const Portfolio &p, const market::Market &mkt)
    {
        double total = 0.0;
        for (const auto &pos : p.positions())
            total += pos.quantity * mkt.getQuote(pos.symbol).price(); // shorts are negative
        return total;
    }

    double PortfolioAnalyzer::totalUnrealizedPnL(const Portfolio &p, const market::Market &mkt)
    {
        double total = 0.0;
        for (const auto &pos : p.positions())
            total += (mkt.getQuote(pos.symbol).price() - pos.avgEntryPrice) * pos.quantity;
        return total;
    }

    double PortfolioAnalyzer::totalRealizedPnL(const Portfolio &p)
    {
        double total = 0.0;
        for (const auto &t : p.history())
            total += t.realizedPnL; // 0 for opening trades
        return total;
    }

    double PortfolioAnalyzer::totalPortfolioValue(const Portfolio &p, const market::Market &mkt)
    {
        return p.cash() + totalMarketValue(p, mkt);
    }

    std::map<market::Sector, double> PortfolioAnalyzer::sectorAllocation(const Portfolio &p, const market::Market &mkt)
    {
        std::map<market::Sector, double> valueBySector;
        double total = 0.0;

        for (const auto &pos : p.positions())
        {
            const int absQuantity = pos.quantity < 0 ? -pos.quantity : pos.quantity;
            const double value = absQuantity * mkt.getQuote(pos.symbol).price();
            valueBySector[mkt.getInstrument(pos.symbol).sector()] += value;
            total += value;
        }
        if (total > 0.0)
            for (auto &entry : valueBySector)
                entry.second = (entry.second / total) * 100.0; // percent of gross position value
        return valueBySector;
    }

    std::vector<HoldingReportLine> PortfolioAnalyzer::holdingsReport(const Portfolio &p, const market::Market &mkt)
    {
        std::vector<HoldingReportLine> lines;
        for (const auto &pos : p.positions())
        {
            double price = mkt.getQuote(pos.symbol).price();
            lines.push_back({pos.symbol, pos.quantity, pos.avgEntryPrice, price,
                             price * pos.quantity, (price - pos.avgEntryPrice) * pos.quantity});
        }
        return lines;
    }

} // namespace portfolio