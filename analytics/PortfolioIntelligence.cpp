#include "PortfolioIntelligence.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <stdexcept>

namespace analytics
{

    namespace
    {
        constexpr double kPnLEpsilon = 1e-9; // |P&L| at or below this counts as break-even

        DrawdownAnalytics computeDrawdown(const std::vector<PortfolioSnapshot> &history)
        {
            DrawdownAnalytics d;
            d.snapshotCount = history.size();
            if (history.size() < 2)
            {
                return d; // not enough real data: report "unavailable", never a made-up number
            }

            d.available = true;
            double peak = history.front().equity;
            double currentPercent = 0.0;
            for (const auto &s : history)
            {
                peak = std::max(peak, s.equity);
                const double amount = peak - s.equity;
                const double percent = peak > 0.0 ? amount / peak * 100.0 : 0.0;
                if (percent > d.maxDrawdownPercent)
                {
                    d.maxDrawdownPercent = percent;
                    d.maxDrawdownAmount = amount;
                    d.peakEquity = peak;
                    d.troughEquity = s.equity;
                }
                currentPercent = percent;
            }
            d.currentDrawdownPercent = currentPercent;
            if (d.maxDrawdownPercent == 0.0)
            {
                d.peakEquity = peak;
                d.troughEquity = peak;
            }
            return d;
        }
    } // namespace

    PortfolioIntelligence::PortfolioIntelligence(const market::Market &mkt,
                                                 const portfolio::Portfolio &portfolioRef,
                                                 const risk::RiskManager &riskManager,
                                                 double initialEquity, IntelligenceConfig config)
        : market_(mkt), portfolio_(portfolioRef), risk_(riskManager), initialEquity_(initialEquity),
          config_(config)
    {
        validateConfig(config_);
        if (!std::isfinite(initialEquity_) || initialEquity_ < 0.0)
        {
            throw std::invalid_argument("Initial equity must be a finite, non-negative number");
        }
    }

    void PortfolioIntelligence::validateConfig(const IntelligenceConfig &c)
    {
        if (!(c.positionConcentrationWarnPercent > 0.0 && c.positionConcentrationWarnPercent <= 100.0))
        {
            throw std::invalid_argument("Position concentration warning threshold must be in (0, 100]");
        }
        if (!(c.sectorConcentrationWarnPercent > 0.0 && c.sectorConcentrationWarnPercent <= 100.0))
        {
            throw std::invalid_argument("Sector concentration warning threshold must be in (0, 100]");
        }
        if (c.maxHistorySnapshots < 1)
        {
            throw std::invalid_argument("History must be able to hold at least one snapshot");
        }
    }

    // ---------------------------- builders ----------------------------------

    PortfolioOverview PortfolioIntelligence::buildOverview(const risk::AccountRisk &account) const
    {
        PortfolioOverview o;
        o.cash = portfolio_.cash();
        o.equity = portfolio::PortfolioAnalyzer::totalPortfolioValue(portfolio_, market_);
        o.longExposure = account.longMarketValue;
        o.shortExposure = account.shortMarketValue;
        o.grossExposure = account.grossExposure;
        o.netExposure = account.longMarketValue - account.shortMarketValue;
        o.realizedPnL = portfolio::PortfolioAnalyzer::totalRealizedPnL(portfolio_);
        o.unrealizedPnL = portfolio::PortfolioAnalyzer::totalUnrealizedPnL(portfolio_, market_);
        o.totalPnL = o.realizedPnL + o.unrealizedPnL;
        o.initialEquity = initialEquity_;
        o.totalReturnPercent =
            initialEquity_ > 0.0 ? (o.equity - initialEquity_) / initialEquity_ * 100.0 : 0.0;
        o.longPositionCount = portfolio_.holdings().size();
        o.shortPositionCount = portfolio_.shorts().size();
        o.positionCount = o.longPositionCount + o.shortPositionCount;
        return o;
    }

    std::vector<PositionAnalytics> PortfolioIntelligence::buildPositions(const risk::AccountRisk &account) const
    {
        const std::vector<portfolio::HoldingReportLine> lines =
            portfolio::PortfolioAnalyzer::holdingsReport(portfolio_, market_);

        std::vector<PositionAnalytics> result;
        result.reserve(lines.size());
        for (const auto &line : lines)
        {
            const market::Instrument &instrument = market_.getInstrument(line.symbol);

            PositionAnalytics p;
            p.symbol = line.symbol;
            p.name = instrument.name();
            p.sector = instrument.sector();
            p.side = line.quantity < 0 ? PositionSide::Short : PositionSide::Long;
            p.signedQuantity = line.quantity;
            p.avgEntryPrice = line.avgCost;
            p.currentPrice = line.currentPrice;
            p.marketValue = line.marketValue;
            p.exposure = std::fabs(line.marketValue);

            const int absQuantity = line.quantity < 0 ? -line.quantity : line.quantity;
            p.costBasis = static_cast<double>(absQuantity) * line.avgCost;
            p.unrealizedPnL = line.unrealizedPnL;
            p.unrealizedPnLPercent = p.costBasis > 0.0 ? p.unrealizedPnL / p.costBasis * 100.0 : 0.0;
            p.grossWeightPercent =
                account.grossExposure > 0.0 ? p.exposure / account.grossExposure * 100.0 : 0.0;
            p.equityWeightPercent = account.equity > 0.0 ? p.marketValue / account.equity * 100.0 : 0.0;
            result.push_back(p);
        }

        // Total order (exposure desc, then symbol asc): fully deterministic.
        std::sort(result.begin(), result.end(),
                  [](const PositionAnalytics &a, const PositionAnalytics &b)
                  {
                      if (a.exposure != b.exposure)
                      {
                          return a.exposure > b.exposure;
                      }
                      return a.symbol < b.symbol;
                  });
        return result;
    }

    AllocationAnalytics PortfolioIntelligence::buildAllocation(const risk::AccountRisk &account,
                                                               const std::vector<PositionAnalytics> &positions) const
    {
        AllocationAnalytics a;

        // Sector percentages come straight from the existing PortfolioAnalyzer.
        const std::map<market::Sector, double> percentBySector =
            portfolio::PortfolioAnalyzer::sectorAllocation(portfolio_, market_);

        std::map<market::Sector, SectorAllocation> bySector;
        for (const auto &p : positions)
        {
            SectorAllocation &s = bySector[p.sector];
            s.sector = p.sector;
            if (p.side == PositionSide::Long)
            {
                s.longValue += p.exposure;
            }
            else
            {
                s.shortValue += p.exposure;
            }
            ++s.positionCount;
        }
        for (auto &entry : bySector)
        {
            SectorAllocation s = entry.second;
            s.grossValue = s.longValue + s.shortValue;
            s.netValue = s.longValue - s.shortValue;
            auto it = percentBySector.find(entry.first);
            s.grossPercent = it != percentBySector.end() ? it->second : 0.0;
            a.sectors.push_back(s);
        }
        std::sort(a.sectors.begin(), a.sectors.end(),
                  [](const SectorAllocation &x, const SectorAllocation &y)
                  {
                      if (x.grossValue != y.grossValue)
                      {
                          return x.grossValue > y.grossValue;
                      }
                      return static_cast<int>(x.sector) < static_cast<int>(y.sector);
                  });

        // Long vs short exposure.
        a.exposure.longExposure = account.longMarketValue;
        a.exposure.shortExposure = account.shortMarketValue;
        a.exposure.grossExposure = account.grossExposure;
        a.exposure.netExposure = account.longMarketValue - account.shortMarketValue;
        if (account.grossExposure > 0.0)
        {
            a.exposure.longPercentOfGross = account.longMarketValue / account.grossExposure * 100.0;
            a.exposure.shortPercentOfGross = account.shortMarketValue / account.grossExposure * 100.0;
        }

        // Concentration.
        double hhi = 0.0;
        double topThree = 0.0;
        std::size_t index = 0;
        for (const auto &p : positions)
        {
            const double weight = p.grossWeightPercent / 100.0;
            hhi += weight * weight;
            if (index < 3)
            {
                topThree += p.grossWeightPercent;
            }
            ++index;
        }
        a.concentration.herfindahlIndex = hhi;
        a.concentration.effectivePositionCount = hhi > 0.0 ? 1.0 / hhi : 0.0;
        a.concentration.largestPositionPercent = positions.empty() ? 0.0 : positions.front().grossWeightPercent;
        a.concentration.topThreePositionsPercent = topThree;
        a.concentration.largestSectorPercent = a.sectors.empty() ? 0.0 : a.sectors.front().grossPercent;

        // Largest positions.
        const std::size_t count = std::min(config_.topPositionCount, positions.size());
        a.largestPositions.assign(positions.begin(), positions.begin() + static_cast<std::ptrdiff_t>(count));
        return a;
    }

    RiskAnalytics PortfolioIntelligence::buildRisk(const risk::AccountRisk &account,
                                                   const std::vector<PositionAnalytics> &positions,
                                                   const AllocationAnalytics &allocation) const
    {
        RiskAnalytics r;
        r.equity = account.equity;
        r.grossExposure = account.grossExposure;
        r.leverage = account.leverage;
        r.buyingPower = account.buyingPower;
        r.marginUsed = account.initialMarginRequired;
        r.availableMargin = account.availableMargin;
        r.maintenanceMarginRequired = account.maintenanceMarginRequired;
        r.maintenanceCushion = account.equity - account.maintenanceMarginRequired;
        r.marginCall = account.marginCall();

        if (!positions.empty())
        {
            r.largestPositionSymbol = positions.front().symbol;
            r.largestPositionValue = positions.front().exposure;
            r.largestPositionPercent = positions.front().grossWeightPercent;
        }
        if (!allocation.sectors.empty())
        {
            r.largestSector = allocation.sectors.front().sector;
            r.largestSectorValue = allocation.sectors.front().grossValue;
            r.largestSectorPercent = allocation.sectors.front().grossPercent;
        }

        // Warnings: margin call first, then concentration (largest first).
        if (r.marginCall)
        {
            RiskWarning w;
            w.type = WarningType::MarginCall;
            w.severity = WarningSeverity::Critical;
            w.value = account.equity;
            w.threshold = account.maintenanceMarginRequired;
            r.warnings.push_back(w);
        }
        for (const auto &p : positions)
        {
            if (p.grossWeightPercent > config_.positionConcentrationWarnPercent)
            {
                RiskWarning w;
                w.type = WarningType::PositionConcentration;
                w.severity = WarningSeverity::Warning;
                w.symbol = p.symbol;
                w.sector = p.sector;
                w.value = p.grossWeightPercent;
                w.threshold = config_.positionConcentrationWarnPercent;
                r.warnings.push_back(w);
            }
        }
        for (const auto &s : allocation.sectors)
        {
            if (s.grossPercent > config_.sectorConcentrationWarnPercent)
            {
                RiskWarning w;
                w.type = WarningType::SectorConcentration;
                w.severity = WarningSeverity::Warning;
                w.sector = s.sector;
                w.value = s.grossPercent;
                w.threshold = config_.sectorConcentrationWarnPercent;
                r.warnings.push_back(w);
            }
        }
        return r;
    }

    PerformanceAnalytics PortfolioIntelligence::buildPerformance(const PortfolioOverview &overview) const
    {
        PerformanceAnalytics p;
        p.realizedPnL = overview.realizedPnL;
        p.unrealizedPnL = overview.unrealizedPnL;
        p.totalPnL = overview.totalPnL;
        p.totalReturnPercent = overview.totalReturnPercent;

        // Only completed (closing / covering) transactions say anything about wins and losses.
        double winTotal = 0.0;
        double lossTotal = 0.0;
        for (const auto &t : portfolio_.history())
        {
            if (t.effect != portfolio::PositionEffect::CloseLong &&
                t.effect != portfolio::PositionEffect::CoverShort)
            {
                continue;
            }
            ++p.closedTrades;
            if (t.realizedPnL > kPnLEpsilon)
            {
                ++p.winningTrades;
                winTotal += t.realizedPnL;
            }
            else if (t.realizedPnL < -kPnLEpsilon)
            {
                ++p.losingTrades;
                lossTotal += t.realizedPnL;
            }
            else
            {
                ++p.breakEvenTrades;
            }
        }

        const std::size_t decided = p.winningTrades + p.losingTrades;
        p.winRateAvailable = decided > 0;
        p.winRatePercent = decided > 0 ? static_cast<double>(p.winningTrades) / static_cast<double>(decided) * 100.0 : 0.0;
        p.averageWin = p.winningTrades > 0 ? winTotal / static_cast<double>(p.winningTrades) : 0.0;
        p.averageLoss = p.losingTrades > 0 ? lossTotal / static_cast<double>(p.losingTrades) : 0.0;

        p.drawdown = computeDrawdown(history_);
        return p;
    }

    // ------------------------------ public API ------------------------------

    PortfolioOverview PortfolioIntelligence::overview() const
    {
        return buildOverview(risk_.assess());
    }

    std::vector<PositionAnalytics> PortfolioIntelligence::positions() const
    {
        return buildPositions(risk_.assess());
    }

    std::vector<PositionAnalytics> PortfolioIntelligence::largestPositions(std::size_t count) const
    {
        std::vector<PositionAnalytics> all = buildPositions(risk_.assess());
        if (all.size() > count)
        {
            all.resize(count);
        }
        return all;
    }

    AllocationAnalytics PortfolioIntelligence::allocation() const
    {
        const risk::AccountRisk account = risk_.assess();
        return buildAllocation(account, buildPositions(account));
    }

    RiskAnalytics PortfolioIntelligence::riskAnalytics() const
    {
        const risk::AccountRisk account = risk_.assess();
        const std::vector<PositionAnalytics> pos = buildPositions(account);
        return buildRisk(account, pos, buildAllocation(account, pos));
    }

    PerformanceAnalytics PortfolioIntelligence::performance() const
    {
        return buildPerformance(buildOverview(risk_.assess()));
    }

    PortfolioReport PortfolioIntelligence::report() const
    {
        const risk::AccountRisk account = risk_.assess();

        PortfolioReport r;
        r.overview = buildOverview(account);
        r.positions = buildPositions(account);
        r.allocation = buildAllocation(account, r.positions);
        r.riskAnalytics = buildRisk(account, r.positions, r.allocation);
        r.performance = buildPerformance(r.overview);
        return r;
    }

    PortfolioSnapshot PortfolioIntelligence::recordSnapshot()
    {
        return recordSnapshot(market_.clock().now());
    }

    PortfolioSnapshot PortfolioIntelligence::recordSnapshot(std::time_t timestamp)
    {
        const PortfolioOverview o = buildOverview(risk_.assess());

        PortfolioSnapshot s;
        s.sequence = nextSequence_++;
        s.timestamp = timestamp;
        s.cash = o.cash;
        s.equity = o.equity;
        s.longExposure = o.longExposure;
        s.shortExposure = o.shortExposure;
        s.grossExposure = o.grossExposure;
        s.realizedPnL = o.realizedPnL;
        s.unrealizedPnL = o.unrealizedPnL;
        s.positionCount = o.positionCount;

        history_.push_back(s);
        if (history_.size() > config_.maxHistorySnapshots)
        {
            const std::size_t excess = history_.size() - config_.maxHistorySnapshots;
            history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(excess));
        }
        return s;
    }

} // namespace analytics