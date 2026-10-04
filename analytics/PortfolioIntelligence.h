#ifndef MARKETSIM_ANALYTICS_PORTFOLIOINTELLIGENCE_H
#define MARKETSIM_ANALYTICS_PORTFOLIOINTELLIGENCE_H

#include <cstddef>
#include <ctime>
#include <optional>
#include <string>
#include <vector>
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"
#include "../Risk/RiskManager.h"

// Analytics module: read-only portfolio intelligence for consoles and GUIs.
//
// PortfolioIntelligence NEVER mutates Market, Portfolio or RiskManager. It
// reads them (through PortfolioAnalyzer and RiskManager::assess(), so no
// calculation is duplicated) and returns plain structs with no text
// formatting. The only state it owns is its own bounded snapshot history,
// which is filled ONLY when the caller asks (recordSnapshot()). Nothing
// historical is ever invented.
//
// Definitions used throughout:
//   exposure (position)  = |signed market value|
//   grossWeightPercent   = position exposure / gross exposure * 100
//   equityWeightPercent  = signed market value / equity * 100 (0 if equity <= 0;
//                          negative for shorts)
//   unrealizedPnLPercent = unrealized P&L / cost basis * 100,
//                          cost basis = |quantity| * average entry price
//   totalReturnPercent   = (equity - initialEquity) / initialEquity * 100
//                          (0 if initialEquity == 0)
//   closed trade         = one CloseLong / CoverShort transaction (one per fill portion)
//   win rate             = wins / (wins + losses) * 100; break-even trades are
//                          counted separately and excluded
// Lifetime: the referenced Market, Portfolio and RiskManager must outlive this object.
namespace analytics
{

    enum class PositionSide
    {
        Long,
        Short
    };

    // ---- Overview ---------------------------------------------------------
    struct PortfolioOverview
    {
        double cash = 0.0;
        double equity = 0.0; // cash + net position value
        double longExposure = 0.0;
        double shortExposure = 0.0; // positive number
        double grossExposure = 0.0; // long + short
        double netExposure = 0.0;   // long - short
        double realizedPnL = 0.0;
        double unrealizedPnL = 0.0;
        double totalPnL = 0.0; // realized + unrealized
        double initialEquity = 0.0;
        double totalReturnPercent = 0.0;
        std::size_t positionCount = 0;
        std::size_t longPositionCount = 0;
        std::size_t shortPositionCount = 0;
    };

    // ---- Positions --------------------------------------------------------
    struct PositionAnalytics
    {
        std::string symbol;
        std::string name;
        market::Sector sector = market::Sector::Technology;
        PositionSide side = PositionSide::Long;
        int signedQuantity = 0; // > 0 long, < 0 short
        double avgEntryPrice = 0.0;
        double currentPrice = 0.0;
        double marketValue = 0.0; // signed
        double exposure = 0.0;    // |marketValue|
        double costBasis = 0.0;   // |quantity| * avgEntryPrice
        double unrealizedPnL = 0.0;
        double unrealizedPnLPercent = 0.0;
        double grossWeightPercent = 0.0;
        double equityWeightPercent = 0.0;
    };

    // ---- Allocation -------------------------------------------------------
    struct SectorAllocation
    {
        market::Sector sector = market::Sector::Technology;
        double longValue = 0.0;
        double shortValue = 0.0; // positive number
        double grossValue = 0.0;
        double netValue = 0.0;     // long - short
        double grossPercent = 0.0; // share of total gross exposure (from PortfolioAnalyzer)
        std::size_t positionCount = 0;
    };

    struct ExposureBreakdown
    {
        double longExposure = 0.0;
        double shortExposure = 0.0;
        double grossExposure = 0.0;
        double netExposure = 0.0;
        double longPercentOfGross = 0.0;
        double shortPercentOfGross = 0.0;
    };

    struct ConcentrationMetrics
    {
        double herfindahlIndex = 0.0;        // sum of squared gross weights (fractions): 1/N .. 1; 0 if empty
        double effectivePositionCount = 0.0; // 1 / herfindahlIndex; 0 if empty
        double largestPositionPercent = 0.0;
        double topThreePositionsPercent = 0.0;
        double largestSectorPercent = 0.0;
    };

    struct AllocationAnalytics
    {
        std::vector<SectorAllocation> sectors; // largest gross value first
        ExposureBreakdown exposure;
        ConcentrationMetrics concentration;
        std::vector<PositionAnalytics> largestPositions; // largest first, at most config.topPositionCount
    };

    // ---- Risk -------------------------------------------------------------
    enum class WarningType
    {
        MarginCall,
        PositionConcentration,
        SectorConcentration
    };

    enum class WarningSeverity
    {
        Warning,
        Critical
    };

    // Structured warning: no message text (the GUI/console formats it).
    //   MarginCall            : value = equity, threshold = maintenance margin required
    //   PositionConcentration : symbol set, value = gross weight %, threshold = limit %
    //   SectorConcentration   : sector set, value = gross weight %, threshold = limit %
    struct RiskWarning
    {
        WarningType type = WarningType::MarginCall;
        WarningSeverity severity = WarningSeverity::Warning;
        std::string symbol;
        market::Sector sector = market::Sector::Technology;
        double value = 0.0;
        double threshold = 0.0;
    };

    struct RiskAnalytics
    {
        double equity = 0.0;
        double grossExposure = 0.0;
        double leverage = 0.0; // gross / equity; 0 if no exposure; +infinity if equity <= 0
        double buyingPower = 0.0;
        double marginUsed = 0.0;      // initial margin required
        double availableMargin = 0.0; // equity - margin used (may be negative)
        double maintenanceMarginRequired = 0.0;
        double maintenanceCushion = 0.0; // equity - maintenance required (negative = in margin call)
        bool marginCall = false;

        std::string largestPositionSymbol; // empty if no positions
        double largestPositionValue = 0.0;
        double largestPositionPercent = 0.0; // of gross exposure
        std::optional<market::Sector> largestSector;
        double largestSectorValue = 0.0;
        double largestSectorPercent = 0.0;

        // Margin call first (if any), then positions, then sectors, largest first.
        std::vector<RiskWarning> warnings;
    };

    // ---- Performance ------------------------------------------------------
    struct DrawdownAnalytics
    {
        bool available = false; // false until at least two snapshots have been recorded
        std::size_t snapshotCount = 0;
        double maxDrawdownPercent = 0.0;
        double maxDrawdownAmount = 0.0;
        double peakEquity = 0.0;   // peak before the worst drawdown (overall peak if none)
        double troughEquity = 0.0; // equity at the worst drawdown (overall peak if none)
        double currentDrawdownPercent = 0.0;
    };

    struct PerformanceAnalytics
    {
        double realizedPnL = 0.0;
        double unrealizedPnL = 0.0;
        double totalPnL = 0.0;
        double totalReturnPercent = 0.0;

        std::size_t closedTrades = 0;
        std::size_t winningTrades = 0;
        std::size_t losingTrades = 0;
        std::size_t breakEvenTrades = 0;
        bool winRateAvailable = false; // true if at least one winning or losing trade exists
        double winRatePercent = 0.0;
        double averageWin = 0.0;  // 0 if no wins
        double averageLoss = 0.0; // negative number; 0 if no losses

        DrawdownAnalytics drawdown;
    };

    // ---- History ----------------------------------------------------------
    // One recorded point in time, suitable for a chart (equity curve, exposure curves).
    struct PortfolioSnapshot
    {
        std::size_t sequence = 0; // monotonically increasing, survives dropping old snapshots
        std::time_t timestamp = 0;
        double cash = 0.0;
        double equity = 0.0;
        double longExposure = 0.0;
        double shortExposure = 0.0;
        double grossExposure = 0.0;
        double realizedPnL = 0.0;
        double unrealizedPnL = 0.0;
        std::size_t positionCount = 0;
    };

    struct IntelligenceConfig
    {
        double positionConcentrationWarnPercent = 25.0; // warn when a position's gross weight exceeds this
        double sectorConcentrationWarnPercent = 40.0;   // warn when a sector's gross weight exceeds this
        std::size_t topPositionCount = 5;               // size of AllocationAnalytics::largestPositions
        std::size_t maxHistorySnapshots = 5000;         // oldest snapshots are dropped beyond this
    };

    // Everything at once, computed from one consistent view.
    struct PortfolioReport
    {
        PortfolioOverview overview;
        std::vector<PositionAnalytics> positions; // largest exposure first (ties by symbol)
        AllocationAnalytics allocation;
        RiskAnalytics riskAnalytics;
        PerformanceAnalytics performance;
    };

    class PortfolioIntelligence
    {
    public:
        // initialEquity is the account's starting equity (used for total return);
        // 0 disables return calculations. Throws std::invalid_argument for a
        // negative/non-finite initialEquity or an invalid config.
        PortfolioIntelligence(const market::Market &mkt, const portfolio::Portfolio &portfolioRef,
                              const risk::RiskManager &riskManager, double initialEquity,
                              IntelligenceConfig config = IntelligenceConfig());

        static void validateConfig(const IntelligenceConfig &config);

        PortfolioOverview overview() const;
        // Largest exposure first; ties broken by symbol, so the order is deterministic.
        std::vector<PositionAnalytics> positions() const;
        std::vector<PositionAnalytics> largestPositions(std::size_t count) const;
        AllocationAnalytics allocation() const;
        RiskAnalytics riskAnalytics() const;
        PerformanceAnalytics performance() const;
        PortfolioReport report() const;

        // Records the current state as a snapshot (the only mutating operations;
        // they change this object's own history only). The no-argument form uses
        // the market clock; pass a timestamp explicitly when the clock is not
        // advancing (e.g. historical replay).
        PortfolioSnapshot recordSnapshot();
        PortfolioSnapshot recordSnapshot(std::time_t timestamp);
        const std::vector<PortfolioSnapshot> &history() const { return history_; }
        void clearHistory() { history_.clear(); }

        double initialEquity() const { return initialEquity_; }
        const IntelligenceConfig &config() const { return config_; }

    private:
        PortfolioOverview buildOverview(const risk::AccountRisk &account) const;
        std::vector<PositionAnalytics> buildPositions(const risk::AccountRisk &account) const;
        AllocationAnalytics buildAllocation(const risk::AccountRisk &account,
                                            const std::vector<PositionAnalytics> &positions) const;
        RiskAnalytics buildRisk(const risk::AccountRisk &account,
                                const std::vector<PositionAnalytics> &positions,
                                const AllocationAnalytics &allocation) const;
        PerformanceAnalytics buildPerformance(const PortfolioOverview &overview) const;

        const market::Market &market_;
        const portfolio::Portfolio &portfolio_;
        const risk::RiskManager &risk_;
        double initialEquity_;
        IntelligenceConfig config_;
        std::vector<PortfolioSnapshot> history_;
        std::size_t nextSequence_ = 0;
    };

} // namespace analytics

#endif // MARKETSIM_ANALYTICS_PORTFOLIOINTELLIGENCE_H