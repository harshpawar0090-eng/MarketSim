#include "RiskManager.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace risk
{

    namespace
    {
        constexpr double kEpsilon = 1e-6;

        std::string money(double value)
        {
            std::ostringstream os;
            os << std::fixed << std::setprecision(2) << "$" << value;
            return os.str();
        }
    } // namespace

    RiskManager::RiskManager(const market::Market &mkt, const portfolio::Portfolio &portfolioRef,
                             RiskConfig config)
        : market_(mkt), portfolio_(portfolioRef), config_(config)
    {
        validateConfig(config_);
    }

    void RiskManager::validateConfig(const RiskConfig &c)
    {
        if (!(c.initialMarginRate > 0.0 && c.initialMarginRate <= 1.0))
        {
            throw std::invalid_argument("Initial margin rate must be in (0, 1]");
        }
        if (!(c.maintenanceMarginRate > 0.0 && c.maintenanceMarginRate <= c.initialMarginRate))
        {
            throw std::invalid_argument("Maintenance margin rate must be in (0, initial margin rate]");
        }
        if (!(c.liquidationBuffer >= 0.0) || !std::isfinite(c.liquidationBuffer))
        {
            throw std::invalid_argument("Liquidation buffer cannot be negative");
        }
        if (c.maxOrderQuantity < 0 || c.maxPositionQuantity < 0)
        {
            throw std::invalid_argument("Quantity limits cannot be negative");
        }
        if (!(c.maxGrossExposure >= 0.0) || !std::isfinite(c.maxGrossExposure))
        {
            throw std::invalid_argument("Gross exposure limit cannot be negative");
        }
    }

    void RiskManager::setConfig(const RiskConfig &config)
    {
        validateConfig(config);
        config_ = config;
    }

    AccountRisk RiskManager::assess() const
    {
        AccountRisk a;
        a.cash = portfolio_.cash();
        for (const auto &entry : portfolio_.holdings())
        {
            a.longMarketValue += entry.second.quantity() * market_.getQuote(entry.first).price();
        }
        for (const auto &entry : portfolio_.shorts())
        {
            a.shortMarketValue += entry.second.quantity() * market_.getQuote(entry.first).price();
        }

        a.equity = a.cash + a.longMarketValue - a.shortMarketValue;
        a.grossExposure = a.longMarketValue + a.shortMarketValue;

        if (a.grossExposure <= 0.0)
        {
            a.leverage = 0.0;
        }
        else if (a.equity > 0.0)
        {
            a.leverage = a.grossExposure / a.equity;
        }
        else
        {
            a.leverage = std::numeric_limits<double>::infinity();
        }

        a.initialMarginRequired = config_.initialMarginRate * a.grossExposure;
        a.maintenanceMarginRequired = config_.maintenanceMarginRate * a.grossExposure;
        a.availableMargin = a.equity - a.initialMarginRequired;
        a.buyingPower = std::max(0.0, a.availableMargin) / config_.initialMarginRate;

        a.status = (a.grossExposure > 0.0 && a.equity < a.maintenanceMarginRequired - kEpsilon)
                       ? MarginStatus::MarginCall
                       : MarginStatus::Healthy;
        return a;
    }

    double RiskManager::buyingPower(double reservedExposure) const
    {
        const AccountRisk a = assess();
        const double available = a.equity - config_.initialMarginRate * (a.grossExposure + reservedExposure);
        return available > 0.0 ? available / config_.initialMarginRate : 0.0;
    }

    bool RiskManager::isOpening(const std::string &symbol, bool isBuy) const
    {
        return isBuy ? portfolio_.shortQuantity(symbol) == 0 : portfolio_.holdingQuantity(symbol) == 0;
    }

    void RiskManager::validate(const OrderCheck &check) const
    {
        if (check.quantity <= 0)
        {
            throw RiskRejectedError("Order quantity must be positive");
        }
        if (!check.liquidation && config_.maxOrderQuantity > 0 && check.quantity > config_.maxOrderQuantity)
        {
            throw RiskRejectedError("Order quantity " + std::to_string(check.quantity) +
                                    " exceeds the maximum order size of " +
                                    std::to_string(config_.maxOrderQuantity));
        }

        const int longQty = portfolio_.holdingQuantity(check.symbol);
        const int shortQty = portfolio_.shortQuantity(check.symbol);

        if (check.isBuy)
        {
            if (shortQty > 0)
            {
                // BUY covers an existing short. It can never exceed the short
                // (no flipping into a long), counting covers already open.
                const int coverable = shortQty - check.openBuyQuantity;
                if (check.quantity > coverable)
                {
                    throw RiskRejectedError("Buy of " + std::to_string(check.quantity) + " " + check.symbol +
                                            " would over-cover the short position (short " +
                                            std::to_string(shortQty) + " shares, " +
                                            std::to_string(check.openBuyQuantity) + " already open to cover)");
                }
                return; // reduces risk: no margin test needed
            }
            checkOpening(check, longQty, check.openBuyQuantity);
            return;
        }

        // SELL
        if (longQty > 0)
        {
            // SELL closes a long: only shares that are owned and not already
            // reserved by other open sell orders.
            const int available = longQty - check.openSellQuantity;
            if (available < check.quantity)
            {
                throw RiskRejectedError("Insufficient available shares to sell " + std::to_string(check.quantity) +
                                        " of " + check.symbol);
            }
            return; // reduces risk
        }

        if (!config_.shortSellingEnabled)
        {
            throw RiskRejectedError("Insufficient available shares to sell " + std::to_string(check.quantity) +
                                    " of " + check.symbol + " (short selling is disabled)");
        }
        // No long position: the sell opens or increases a short.
        checkOpening(check, shortQty, check.openSellQuantity);
    }

    void RiskManager::checkOpening(const OrderCheck &check, int currentPosition, int alreadyOpenQuantity) const
    {
        const AccountRisk account = assess();
        if (account.marginCall())
        {
            throw RiskRejectedError("Account is in margin call: only orders that reduce a position are accepted");
        }

        if (config_.maxPositionQuantity > 0)
        {
            const long long resulting =
                static_cast<long long>(currentPosition) + alreadyOpenQuantity + check.quantity;
            if (resulting > config_.maxPositionQuantity)
            {
                throw RiskRejectedError("Position limit exceeded: this order would bring the position in " +
                                        check.symbol + " to " + std::to_string(resulting) +
                                        " shares (limit " + std::to_string(config_.maxPositionQuantity) + ")");
            }
        }

        const double projectedGross = account.grossExposure + check.openOrderExposure + check.notional;
        if (config_.maxGrossExposure > 0.0 && projectedGross > config_.maxGrossExposure + kEpsilon)
        {
            throw RiskRejectedError("Gross exposure limit exceeded: projected " + money(projectedGross) +
                                    " exceeds the limit of " + money(config_.maxGrossExposure));
        }

        const double requiredEquity = config_.initialMarginRate * projectedGross;
        if (account.equity + kEpsilon < requiredEquity)
        {
            throw RiskRejectedError("Insufficient buying power: this order needs " + money(requiredEquity) +
                                    " of initial margin but equity is " + money(account.equity));
        }
    }

    int RiskManager::liquidationQuantity(const std::string &symbol) const
    {
        const int longQty = portfolio_.holdingQuantity(symbol);
        const int held = longQty > 0 ? longQty : portfolio_.shortQuantity(symbol);
        if (held <= 0)
        {
            return 0;
        }

        const AccountRisk a = assess();
        const double price = market_.getQuote(symbol).price();
        if (!(price > 0.0) || a.equity <= 0.0)
        {
            return held; // cannot restore margin: close the whole position
        }

        // Closing x shares removes x*price of exposure and (ignoring slippage)
        // leaves equity unchanged, so solve equity >= target * (gross - x*price).
        const double target = std::min(config_.initialMarginRate,
                                       config_.maintenanceMarginRate + config_.liquidationBuffer);
        const double needed = (target * a.grossExposure - a.equity) / (target * price);
        if (!(needed > 0.0))
        {
            return 1;
        }
        if (needed >= static_cast<double>(held))
        {
            return held;
        }
        return std::max(1, static_cast<int>(std::ceil(needed)));
    }

} // namespace risk