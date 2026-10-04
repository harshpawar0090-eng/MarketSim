#ifndef MARKETSIM_PORTFOLIO_PORTFOLIO_H
#define MARKETSIM_PORTFOLIO_PORTFOLIO_H

#include <string>
#include <map>
#include <vector>
#include <ctime>
#include <stdexcept>
#include "../Market/Market.h"

// Portfolio module: owns the user's virtual cash, long holdings, short
// positions and transaction history. PortfolioAnalyzer is a separate,
// stateless reporting class.
//
// Position model: a long is stored in holdings() (positive quantity, average
// cost); a short is stored in shorts() (shares owed + average entry/sale
// price). Signed views (positionQuantity(), positions()) report a long as
// positive and a short as negative. A symbol is never long and short at once:
// fills net against the opposite side first.
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

    // What a transaction did to the position.
    //   OpenLong   - BUY that opens/increases a long
    //   CloseLong  - SELL that reduces/closes a long (realizes P&L)
    //   OpenShort  - SELL that opens/increases a short
    //   CoverShort - BUY that reduces/closes a short (realizes P&L)
    enum class PositionEffect
    {
        OpenLong,
        CloseLong,
        OpenShort,
        CoverShort
    };

    // "BUY", "SELL", "SELL SHORT", "BUY TO COVER".
    std::string positionEffectToString(PositionEffect effect);

    // Record of a completed trade: the audit trail used for realized P&L.
    struct Transaction
    {
        std::string symbol;
        TransactionSide side;
        int quantity;
        double price;
        double realizedPnL; // 0.0 for opening trades, computed for closing/covering trades
        std::time_t timestamp;
        PositionEffect effect;

        // Effect defaults to OpenLong for a Buy and CloseLong for a Sell.
        Transaction(std::string symbol_, TransactionSide side_, int quantity_,
                    double price_, double realizedPnL_);
        Transaction(std::string symbol_, TransactionSide side_, int quantity_,
                    double price_, double realizedPnL_, PositionEffect effect_);
    };

    // One position: number of shares and their average price. For a long this
    // is the average cost; for a short (stored in shorts()) quantity is the
    // number of shares owed and avgCost() is the average sale (entry) price.
    class Holding
    {
    public:
        Holding(std::string symbol, int quantity, double avgCost);

        const std::string &symbol() const { return symbol_; }
        int quantity() const { return quantity_; }
        double avgCost() const { return avgCost_; }

        // Adds shares and recalculates the weighted average price.
        void addShares(int qty, double price);
        // Removes shares; throws std::invalid_argument if qty is invalid or exceeds holding.
        void removeShares(int qty);

    private:
        std::string symbol_;
        int quantity_;
        double avgCost_;
    };

    // Result of applying one executed fill to the portfolio.
    struct FillResult
    {
        int closedQuantity = 0; // shares that reduced the opposite position (sold long / covered short)
        int openedQuantity = 0; // shares that opened/increased a position
        double realizedPnL = 0.0;
    };

    // Signed view of one position: quantity > 0 long, < 0 short.
    struct Position
    {
        std::string symbol;
        int quantity;
        double avgEntryPrice;

        bool isShort() const { return quantity < 0; }
    };

    // Owns the real state: cash, positions and history. Positions can only be
    // changed through the apply* methods (outside code gets const access only).
    class Portfolio
    {
    public:
        explicit Portfolio(double startingCash);

        double cash() const { return cash_; }
        bool hasHolding(const std::string &symbol) const { return holdings_.count(symbol) > 0; }
        // Long shares owned. Shorted shares are NOT owned shares: this is 0 for a short.
        int holdingQuantity(const std::string &symbol) const { return hasHolding(symbol) ? holdings_.at(symbol).quantity() : 0; }
        const std::map<std::string, Holding> &holdings() const { return holdings_; }

        bool hasShort(const std::string &symbol) const { return shorts_.count(symbol) > 0; }
        // Shares currently owed (positive number).
        int shortQuantity(const std::string &symbol) const { return hasShort(symbol) ? shorts_.at(symbol).quantity() : 0; }
        const std::map<std::string, Holding> &shorts() const { return shorts_; }

        // +long / -short / 0 flat.
        int positionQuantity(const std::string &symbol) const { return holdingQuantity(symbol) - shortQuantity(symbol); }
        // All open positions (longs and shorts), signed, sorted by symbol.
        std::vector<Position> positions() const;

        const std::vector<Transaction> &history() const { return history_; }

        // Legacy long-only operations (unchanged behavior).
        // Debits cash and grows/creates the holding. Throws PortfolioError if cash is insufficient.
        void applyBuy(const std::string &symbol, int qty, double price);
        // Credits cash and shrinks/removes the holding. Throws PortfolioError if no such holding exists.
        void applySell(const std::string &symbol, int qty, double price);

        // Applies an executed fill (used by settlement). Cash may go negative
        // (margin loan): affordability is the RiskManager's job, not the Portfolio's.
        // BUY : covers a short first (realizing short P&L), any remainder opens/increases a long.
        // SELL: closes a long first (realizing long P&L), any remainder opens/increases a short.
        FillResult applyBuyFill(const std::string &symbol, int qty, double price);
        FillResult applySellFill(const std::string &symbol, int qty, double price);

        void recordTransaction(const Transaction &transaction) { history_.push_back(transaction); }

    private:
        double cash_;
        std::map<std::string, Holding> holdings_;
        std::map<std::string, Holding> shorts_;
        std::vector<Transaction> history_;
    };

    // One row of the holdings report (lets the UI print a table without any math).
    // quantity is signed: negative = short. marketValue is signed too, and
    // unrealizedPnL = (currentPrice - avgCost) * quantity works for both sides.
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
        // Net market value of positions: longs minus shorts.
        static double totalMarketValue(const Portfolio &p, const market::Market &mkt);
        static double totalUnrealizedPnL(const Portfolio &p, const market::Market &mkt);
        // Sum of realized P&L over every transaction (sells of longs, covers of shorts).
        static double totalRealizedPnL(const Portfolio &p);
        // Equity: cash + net market value.
        static double totalPortfolioValue(const Portfolio &p, const market::Market &mkt);
        // Share of gross position value (|qty| * price) per sector, in percent.
        static std::map<market::Sector, double> sectorAllocation(const Portfolio &p, const market::Market &mkt);
        static std::vector<HoldingReportLine> holdingsReport(const Portfolio &p, const market::Market &mkt);
    };

} // namespace portfolio

#endif // MARKETSIM_PORTFOLIO_PORTFOLIO_H