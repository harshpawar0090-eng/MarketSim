#ifndef MARKETSIM_RISK_RISKMANAGER_H
#define MARKETSIM_RISK_RISKMANAGER_H

#include <limits>
#include <string>
#include "../Execution/Order.h"
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"

// Risk module: pre-trade validation, account metrics and margin monitoring.
//
// Simulator margin rule (explicit, simplified - NOT an NSE/exchange model):
//   longMV        = sum(long qty  * market price)
//   shortMV       = sum(short qty * market price)
//   equity        = cash + longMV - shortMV      (short sale proceeds sit in cash)
//   gross         = longMV + shortMV
//   initial req.  = initialMarginRate     * gross   (needed to OPEN / increase)
//   maint. req.   = maintenanceMarginRate * gross   (equity below this = margin call)
//   buying power  = max(0, equity - initialMarginRate * gross) / initialMarginRate
// With the default config (initial rate 1.0, shorting off) this reproduces a
// plain fully-paid cash account.
//
// The RiskManager only reads Market and Portfolio. It never matches orders and
// never changes state; ExecutionService calls it before anything is submitted.
namespace risk
{

    // Thrown when an order fails a risk check. Derives from
    // execution::OrderRejectedError so existing handlers keep working.
    class RiskRejectedError : public execution::OrderRejectedError
    {
    public:
        explicit RiskRejectedError(const std::string &message) : execution::OrderRejectedError(message) {}
    };

    struct RiskConfig
    {
        bool shortSellingEnabled = false;
        double initialMarginRate = 1.0;      // (0,1]; 1.0 = no leverage, 0.5 = 2x
        double maintenanceMarginRate = 0.25; // (0, initialMarginRate]
        double liquidationBuffer = 0.05;     // liquidate until equity >= (maintenance+buffer)*gross, capped at initial rate
        bool autoLiquidation = true;         // forced liquidation on margin call
        int maxOrderQuantity = 0;            // shares per order; 0 = unlimited
        int maxPositionQuantity = 0;         // |shares| per symbol (incl. open orders); 0 = unlimited
        double maxGrossExposure = 0.0;       // $ gross exposure (incl. open orders); 0 = unlimited

        // Convenience: shorting on, 2x leverage, 25% maintenance margin.
        static RiskConfig marginAccount()
        {
            RiskConfig c;
            c.shortSellingEnabled = true;
            c.initialMarginRate = 0.5;
            c.maintenanceMarginRate = 0.25;
            c.liquidationBuffer = 0.05;
            return c;
        }
    };

    enum class MarginStatus
    {
        Healthy,
        MarginCall
    };

    // Snapshot of the account at current market prices.
    struct AccountRisk
    {
        double cash = 0.0;
        double longMarketValue = 0.0;
        double shortMarketValue = 0.0; // positive number
        double equity = 0.0;
        double grossExposure = 0.0;
        double leverage = 0.0;              // gross / equity; 0 if no exposure; +infinity if equity <= 0
        double initialMarginRequired = 0.0; // "margin used"
        double maintenanceMarginRequired = 0.0;
        double availableMargin = 0.0; // equity - initialMarginRequired (may be negative)
        double buyingPower = 0.0;     // extra gross exposure that can be opened (ignores open orders)
        MarginStatus status = MarginStatus::Healthy;

        bool marginCall() const { return status == MarginStatus::MarginCall; }
    };

    // Everything the RiskManager needs to judge one order. ExecutionService
    // fills it in (it knows the open orders and the order book).
    struct OrderCheck
    {
        std::string symbol;
        bool isBuy = true;
        int quantity = 0;
        double notional = 0.0;          // expected $ value of the order (fill preview / limit / stop price)
        double openOrderExposure = 0.0; // $ of the user's open orders that would OPEN positions (all symbols)
        int openBuyQuantity = 0;        // user's open BUY quantity in this symbol
        int openSellQuantity = 0;       // user's open SELL quantity in this symbol
        bool liquidation = false;       // forced-liquidation order (skips the max-order-size limit)
    };

    class RiskManager
    {
    public:
        RiskManager(const market::Market &mkt, const portfolio::Portfolio &portfolioRef,
                    RiskConfig config = RiskConfig());

        const RiskConfig &config() const { return config_; }
        // Validates, then replaces the configuration. Throws std::invalid_argument.
        void setConfig(const RiskConfig &config);
        static void validateConfig(const RiskConfig &config);

        AccountRisk assess() const;

        // Extra gross exposure that can still be opened, after counting
        // reservedExposure (open opening orders).
        double buyingPower(double reservedExposure = 0.0) const;

        // True if an order on this side would open/increase a position
        // (BUY with no short held, SELL with no long held); false if it would
        // reduce one (cover a short / close a long).
        bool isOpening(const std::string &symbol, bool isBuy) const;

        // Throws RiskRejectedError if the order violates a limit.
        void validate(const OrderCheck &check) const;

        // Shares of this symbol's position to close to bring the account back
        // to the liquidation target margin level (1..position size; 0 if no position).
        int liquidationQuantity(const std::string &symbol) const;

    private:
        void checkOpening(const OrderCheck &check, int currentPosition, int alreadyOpenQuantity) const;

        const market::Market &market_;
        const portfolio::Portfolio &portfolio_;
        RiskConfig config_;
    };

} // namespace risk

#endif // MARKETSIM_RISK_RISKMANAGER_H