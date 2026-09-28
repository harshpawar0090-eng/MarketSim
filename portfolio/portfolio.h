#ifndef MARKETSIM_PORTFOLIO_PORTFOLIO_H
#define MARKETSIM_PORTFOLIO_PORTFOLIO_H

#include <string>
#include <map>
#include <vector>
#include <ctime>
#include <stdexcept>
#include "../Market/Market.h"

// Portfolio module: owns the user's virtual cash, holdings and transaction
// history. PortfolioAnalyzer is a separate, stateless reporting class.
namespace portfolio
{
    // Defensive error for invalid portfolio operations (e.g. selling a position that does not exist).
    class PortfolioError : public std::runtime_error
    {
    public:
        explicit PortfolioError(const std::string &message) : std::runtime_error(message) {}
    };

    enum class TransactionSide
    {
        Buy,
        Sell
    };

    std::string transactionSideToString(TransactionSide side);

    // Record of a completed trade: the audit trail used for realized P&L.
    struct Transaction
    {
        std::string symbol;
        TransactionSide side;
        int quantity;
        double price;
        double realizedPnL; // 0.0 for buys, computed for sells
        std::time_t timestamp;

        Transaction(std::string symbol_, TransactionSide side_, int quantity_,
                    double price_, double realizedPnL_);
    };

    // One position: number of shares held and their average cost basis.
    class Holding
    {
    public:
        Holding(std::string symbol, int quantity, double avgCost);

        const std::string &symbol() const { return symbol_; }
        int quantity() const { return quantity_; }
        double avgCost() const { return avgCost_; }

        // Adds shares and recalculates the weighted average cost.
        void addShares(int qty, double price);
        // Removes shares; throws std::invalid_argument if qty is invalid or exceeds holding.
        void removeShares(int qty);

    private:
        std::string symbol_;
        int quantity_;
        double avgCost_;
    };

    // Owns the real state: cash, holdings and history. Holdings can only be
    // changed through applyBuy/applySell (outside code gets const access only).
    class Portfolio
    {
    public:
        explicit Portfolio(double startingCash);

        double cash() const { return cash_; }
        bool hasHolding(const std::string &symbol) const { return holdings_.count(symbol) > 0; }
        int holdingQuantity(const std::string &symbol) const { return hasHolding(symbol) ? holdings_.at(symbol).quantity() : 0; }
        const std::map<std::string, Holding> &holdings() const { return holdings_; }
        const std::vector<Transaction> &history() const { return history_; }

        // Debits cash and grows/creates the holding. Throws PortfolioError if cash is insufficient.
        void applyBuy(const std::string &symbol, int qty, double price);
        // Credits cash and shrinks/removes the holding. Throws PortfolioError if no such holding exists.
        void applySell(const std::string &symbol, int qty, double price);

        void recordTransaction(const Transaction &transaction) { history_.push_back(transaction); }

    private:
        double cash_;
        std::map<std::string, Holding> holdings_;
        std::vector<Transaction> history_;
    };

    // One row of the holdings report (lets the UI print a table without any math).
    struct HoldingReportLine
    {
        std::string symbol;
        int quantity;
        double avgCost;
        double currentPrice;
        double marketValue;
        double unrealizedPnL;
    };

    // Stateless calculations over Portfolio + Market.
    class PortfolioAnalyzer
    {
    public:
        static double totalMarketValue(const Portfolio &p, const market::Market &mkt);
        static double totalUnrealizedPnL(const Portfolio &p, const market::Market &mkt);
        static double totalRealizedPnL(const Portfolio &p);
        static double totalPortfolioValue(const Portfolio &p, const market::Market &mkt);
        static std::map<market::Sector, double> sectorAllocation(const Portfolio &p, const market::Market &mkt);
        static std::vector<HoldingReportLine> holdingsReport(const Portfolio &p, const market::Market &mkt);
    };

} // namespace portfolio

#endif // MARKETSIM_PORTFOLIO_PORTFOLIO_H
