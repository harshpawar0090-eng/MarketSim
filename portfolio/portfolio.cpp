#include "Portfolio.h"

namespace portfolio
{

    PortfolioError::PortfolioError(const std::string &message)
        : std::runtime_error(message) {}

    std::string transactionSideToString(TransactionSide side)
    {
        switch (side)
        {
        case TransactionSide::Buy:
            return "BUY";
        case TransactionSide::Sell:
            return "SELL";
        }
        return "UNKNOWN";
    }

    Transaction::Transaction(std::string symbol_, TransactionSide side_, int quantity_,
                             double price_, double realizedPnL_)
        : symbol(std::move(symbol_)),
          side(side_),
          quantity(quantity_),
          price(price_),
          realizedPnL(realizedPnL_),
          timestamp(std::time(nullptr)) {}

    Holding::Holding(std::string symbol, int quantity, double avgCost)
        : symbol_(std::move(symbol)), quantity_(quantity), avgCost_(avgCost)
    {
        if (quantity_ <= 0)
        {
            throw std::invalid_argument("Holding must start with a positive quantity: " + symbol_);
        }
        if (avgCost_ < 0.0)
        {
            throw std::invalid_argument("Holding average cost cannot be negative: " + symbol_);
        }
    }

    const std::string &Holding::symbol() const { return symbol_; }
    int Holding::quantity() const { return quantity_; }
    double Holding::avgCost() const { return avgCost_; }

    void Holding::addShares(int qty, double price)
    {
        if (qty <= 0)
        {
            throw std::invalid_argument("Quantity to add must be positive");
        }
        double totalCostBefore = avgCost_ * quantity_;
        double totalCostAdded = price * qty;
        quantity_ += qty;
        avgCost_ = (totalCostBefore + totalCostAdded) / quantity_;
    }

    void Holding::removeShares(int qty)
    {
        if (qty <= 0)
        {
            throw std::invalid_argument("Quantity to remove must be positive");
        }
        if (qty > quantity_)
        {
            throw std::invalid_argument("Cannot remove more shares than are held: " + symbol_);
        }
        quantity_ -= qty;
        // Average cost basis does not change when only some shares are sold.
    }

    Portfolio::Portfolio(double startingCash) : cash_(startingCash)
    {
        if (cash_ < 0.0)
        {
            throw std::invalid_argument("Starting cash cannot be negative");
        }
    }

    double Portfolio::cash() const { return cash_; }

    bool Portfolio::hasHolding(const std::string &symbol) const
    {
        return holdings_.find(symbol) != holdings_.end();
    }

    int Portfolio::holdingQuantity(const std::string &symbol) const
    {
        auto it = holdings_.find(symbol);
        if (it == holdings_.end())
        {
            return 0;
        }
        return it->second.quantity();
    }

    const std::map<std::string, Holding> &Portfolio::holdings() const { return holdings_; }
    const std::vector<Transaction> &Portfolio::history() const { return history_; }

    void Portfolio::debitCash(double amount)
    {
        if (amount > cash_)
        {
            throw PortfolioError("Insufficient cash for this operation");
        }
        cash_ -= amount;
    }

    void Portfolio::creditCash(double amount)
    {
        cash_ += amount;
    }

    void Portfolio::applyBuy(const std::string &symbol, int qty, double price)
    {
        double cost = price * qty;
        debitCash(cost);

        auto it = holdings_.find(symbol);
        if (it != holdings_.end())
        {
            it->second.addShares(qty, price);
        }
        else
        {
            holdings_.emplace(symbol, Holding(symbol, qty, price));
        }
    }

    void Portfolio::applySell(const std::string &symbol, int qty, double price)
    {
        auto it = holdings_.find(symbol);
        if (it == holdings_.end())
        {
            throw PortfolioError("No holding found for symbol: " + symbol);
        }

        it->second.removeShares(qty);
        creditCash(price * qty);

        if (it->second.quantity() == 0)
        {
            holdings_.erase(it);
        }
    }

    void Portfolio::recordTransaction(const Transaction &transaction)
    {
        history_.push_back(transaction);
    }

    double PortfolioAnalyzer::totalMarketValue(const Portfolio &p, const market::Market &mkt)
    {
        double total = 0.0;
        for (const auto &[symbol, holding] : p.holdings())
        {
            total += holding.quantity() * mkt.getStock(symbol).price();
        }
        return total;
    }

    double PortfolioAnalyzer::totalUnrealizedPnL(const Portfolio &p, const market::Market &mkt)
    {
        double total = 0.0;
        for (const auto &[symbol, holding] : p.holdings())
        {
            double currentPrice = mkt.getStock(symbol).price();
            total += (currentPrice - holding.avgCost()) * holding.quantity();
        }
        return total;
    }

    double PortfolioAnalyzer::totalRealizedPnL(const Portfolio &p)
    {
        double total = 0.0;
        for (const auto &transaction : p.history())
        {
            if (transaction.side == TransactionSide::Sell)
            {
                total += transaction.realizedPnL;
            }
        }
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
            double value = holding.quantity() * stock.price();
            valueBySector[stock.sector()] += value;
        }

        if (total > 0.0)
        {
            for (auto &[sector, value] : valueBySector)
            {
                value = (value / total) * 100.0;
            }
        }

        return valueBySector;
    }

    std::vector<HoldingReportLine> PortfolioAnalyzer::holdingsReport(const Portfolio &p, const market::Market &mkt)
    {
        std::vector<HoldingReportLine> lines;
        for (const auto &[symbol, holding] : p.holdings())
        {
            double currentPrice = mkt.getStock(symbol).price();
            double marketValue = currentPrice * holding.quantity();
            double unrealizedPnL = (currentPrice - holding.avgCost()) * holding.quantity();

            HoldingReportLine line;
            line.symbol = symbol;
            line.quantity = holding.quantity();
            line.avgCost = holding.avgCost();
            line.currentPrice = currentPrice;
            line.marketValue = marketValue;
            line.unrealizedPnL = unrealizedPnL;
            lines.push_back(line);
        }
        return lines;
    }

} // namespace portfolio