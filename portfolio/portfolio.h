#ifndef MARKETSIM_PORTFOLIO_PORTFOLIO_H
#define MARKETSIM_PORTFOLIO_PORTFOLIO_H

#include <string>
#include <map>
#include <vector>
#include <ctime>
#include <stdexcept>
#include "../Market/Market.h"

// Portfolio module: owns the user's virtual cash, holdings, and transaction
// history. Also contains PortfolioAnalyzer, a stateless reporting layer
// kept separate from Portfolio itself (single-responsibility separation).
namespace portfolio
{

    // Thrown for portfolio-internal invariant violations (e.g. selling a
    // position that does not exist). Trading-level validation should normally
    // prevent these from ever being thrown; they exist as a defensive layer.
    class PortfolioError : public std::runtime_error
    {
    public:
        explicit PortfolioError(const std::string &message);
    };

    enum class TransactionSide
    {
        Buy,
        Sell
    };

    std::string transactionSideToString(TransactionSide side);

    // An immutable record of a completed trade. This is the permanent audit
    // trail the rest of the system relies on (e.g. for realized P&L).
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

    // A single position within the portfolio: how many shares are held and at
    // what average cost basis.
    class Holding
    {
    public:
        Holding(std::string symbol, int quantity, double avgCost);

        const std::string &symbol() const;
        int quantity() const;
        double avgCost() const;

        // Adds shares to the position, recalculating the weighted average cost.
        void addShares(int qty, double price);

        // Removes shares from the position. Throws std::invalid_argument if
        // qty is not positive or exceeds the currently held quantity.
        void removeShares(int qty);

    private:
        std::string symbol_;
        int quantity_;
        double avgCost_;
    };

    // Owns the real, authoritative state of the user's portfolio: cash,
    // holdings, and transaction history. Mutation only happens through
    // controlled methods - there is no way to get a non-const handle to the
    // holdings map from outside this class.
    class Portfolio
    {
    public:
        explicit Portfolio(double startingCash);

        double cash() const;
        bool hasHolding(const std::string &symbol) const;
        int holdingQuantity(const std::string &symbol) const;
        const std::map<std::string, Holding> &holdings() const;
        const std::vector<Transaction> &history() const;

        // Applies a buy: debits cash and grows/creates the relevant holding.
        void applyBuy(const std::string &symbol, int qty, double price);

        // Applies a sell: credits cash and shrinks/removes the relevant holding.
        // Throws PortfolioError if no such holding exists.
        void applySell(const std::string &symbol, int qty, double price);

        void recordTransaction(const Transaction &transaction);

    private:
        void debitCash(double amount);
        void creditCash(double amount);

        double cash_;
        std::map<std::string, Holding> holdings_;
        std::vector<Transaction> history_;
    };

    // One line of a holdings report, used by the UI to print a table without
    // PortfolioAnalyzer needing to know anything about formatting or streams.
    struct HoldingReportLine
    {
        std::string symbol;
        int quantity;
        double avgCost;
        double currentPrice;
        double marketValue;
        double unrealizedPnL;
    };

    // Stateless computation layer over Portfolio + Market. Kept deliberately
    // separate from Portfolio so that "state container" and "reporting/analysis"
    // responsibilities do not mix.
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