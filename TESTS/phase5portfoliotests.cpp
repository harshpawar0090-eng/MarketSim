#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include "../Analytics/PortfolioIntelligence.h"
#include "../Execution/ExecutionService.h"
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"
#include "../Risk/RiskManager.h"

using namespace analytics;

static int g_checks = 0;
static int g_failures = 0;

static void check(bool ok, const std::string &what, int line)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::cout << "  FAIL (line " << line << "): " << what << "\n";
    }
}

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

#define CHECK(cond) check((cond), #cond, __LINE__)
#define CHECK_THROWS(expr, ExType)                              \
    do                                                          \
    {                                                           \
        bool threw = false;                                     \
        try                                                     \
        {                                                       \
            expr;                                               \
        }                                                       \
        catch (const ExType &)                                  \
        {                                                       \
            threw = true;                                       \
        }                                                       \
        catch (...)                                             \
        {                                                       \
        }                                                       \
        check(threw, #expr " should throw " #ExType, __LINE__); \
    } while (0)

// ------------------------------ helpers -------------------------------------

static market::Market makeMarket()
{
    market::Market m;
    m.addStock(market::Stock("NOVA", "Nova Technologies", market::Sector::Technology, 100.00));
    m.addStock(market::Stock("SOLR", "Solaris Energy", market::Sector::Energy, 50.00));
    m.addStock(market::Stock("BNKX", "Bankex Financial", market::Sector::Finance, 80.00));
    m.addStock(market::Stock("MEDI", "MediCare Plus", market::Sector::Healthcare, 200.00));
    return m;
}

// Not copyable/movable on purpose: RiskManager and PortfolioIntelligence keep
// references to the members declared before them.
struct Fixture
{
    market::Market market;
    portfolio::Portfolio pf;
    risk::RiskManager rm;
    PortfolioIntelligence intel;

    explicit Fixture(double cash = 100000.0,
                     risk::RiskConfig riskConfig = risk::RiskConfig::marginAccount(),
                     IntelligenceConfig intelConfig = IntelligenceConfig())
        : market(makeMarket()), pf(cash), rm(market, pf, riskConfig), intel(market, pf, rm, cash, intelConfig) {}

    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;
};

// Mirror what ExecutionService::settle() records, so the transaction history
// has the same shape as in the real application.
static void doBuy(portfolio::Portfolio &pf, const std::string &symbol, int qty, double price)
{
    const portfolio::FillResult r = pf.applyBuyFill(symbol, qty, price);
    if (r.closedQuantity > 0)
    {
        pf.recordTransaction(portfolio::Transaction(symbol, portfolio::TransactionSide::Buy, r.closedQuantity,
                                                    price, r.realizedPnL, portfolio::PositionEffect::CoverShort));
    }
    if (r.openedQuantity > 0)
    {
        pf.recordTransaction(portfolio::Transaction(symbol, portfolio::TransactionSide::Buy, r.openedQuantity,
                                                    price, 0.0, portfolio::PositionEffect::OpenLong));
    }
}

static void doSell(portfolio::Portfolio &pf, const std::string &symbol, int qty, double price)
{
    const portfolio::FillResult r = pf.applySellFill(symbol, qty, price);
    if (r.closedQuantity > 0)
    {
        pf.recordTransaction(portfolio::Transaction(symbol, portfolio::TransactionSide::Sell, r.closedQuantity,
                                                    price, r.realizedPnL, portfolio::PositionEffect::CloseLong));
    }
    if (r.openedQuantity > 0)
    {
        pf.recordTransaction(portfolio::Transaction(symbol, portfolio::TransactionSide::Sell, r.openedQuantity,
                                                    price, 0.0, portfolio::PositionEffect::OpenShort));
    }
}

// Mixed book (start 100000):
//   long 100 NOVA @100 -> price 120 (MV 12000, +2000)
//   long  50 BNKX @ 80 -> price 70  (MV  3500,  -500)
//   short 100 SOLR @50 -> price 40  (MV -4000, +1000)
//   cash 91000, equity 102500, long 15500, short 4000, gross 19500
static void setupMixed(Fixture &f)
{
    doBuy(f.pf, "NOVA", 100, 100.0);
    doBuy(f.pf, "BNKX", 50, 80.0);
    doSell(f.pf, "SOLR", 100, 50.0);
    f.market.setPrice("NOVA", 120.0);
    f.market.setPrice("BNKX", 70.0);
    f.market.setPrice("SOLR", 40.0);
}

static const RiskWarning *findWarning(const RiskAnalytics &r, WarningType type, const std::string &symbol = "")
{
    for (const auto &w : r.warnings)
    {
        if (w.type == type && (symbol.empty() || w.symbol == symbol))
        {
            return &w;
        }
    }
    return nullptr;
}

// ------------------------------- tests --------------------------------------

static void testEmptyPortfolio()
{
    std::cout << "[1] Empty portfolio edge cases\n";
    Fixture f;

    const PortfolioOverview o = f.intel.overview();
    CHECK(near(o.cash, 100000.0) && near(o.equity, 100000.0));
    CHECK(near(o.longExposure, 0.0) && near(o.shortExposure, 0.0) && near(o.grossExposure, 0.0));
    CHECK(near(o.netExposure, 0.0));
    CHECK(near(o.realizedPnL, 0.0) && near(o.unrealizedPnL, 0.0) && near(o.totalPnL, 0.0));
    CHECK(near(o.totalReturnPercent, 0.0));
    CHECK(o.positionCount == 0 && o.longPositionCount == 0 && o.shortPositionCount == 0);

    CHECK(f.intel.positions().empty());
    CHECK(f.intel.largestPositions(3).empty());

    const AllocationAnalytics a = f.intel.allocation();
    CHECK(a.sectors.empty() && a.largestPositions.empty());
    CHECK(near(a.exposure.longPercentOfGross, 0.0) && near(a.exposure.shortPercentOfGross, 0.0));
    CHECK(near(a.concentration.herfindahlIndex, 0.0));
    CHECK(near(a.concentration.effectivePositionCount, 0.0));
    CHECK(near(a.concentration.largestPositionPercent, 0.0));
    CHECK(near(a.concentration.topThreePositionsPercent, 0.0));
    CHECK(near(a.concentration.largestSectorPercent, 0.0));

    const RiskAnalytics r = f.intel.riskAnalytics();
    CHECK(near(r.leverage, 0.0) && near(r.marginUsed, 0.0));
    CHECK(near(r.buyingPower, 200000.0)); // equity / 0.5 initial margin
    CHECK(!r.marginCall);
    CHECK(r.largestPositionSymbol.empty() && !r.largestSector.has_value());
    CHECK(r.warnings.empty());

    const PerformanceAnalytics p = f.intel.performance();
    CHECK(p.closedTrades == 0 && !p.winRateAvailable && near(p.winRatePercent, 0.0));
    CHECK(!p.drawdown.available);

    // initialEquity == 0 disables return calculations instead of dividing by zero.
    PortfolioIntelligence zero(f.market, f.pf, f.rm, 0.0);
    CHECK(near(zero.overview().totalReturnPercent, 0.0));

    const PortfolioReport rep = f.intel.report();
    CHECK(rep.positions.empty() && rep.allocation.sectors.empty());
    CHECK(near(rep.overview.equity, 100000.0));
}

static void testLongPosition()
{
    std::cout << "[2] Long position\n";
    Fixture f;
    doBuy(f.pf, "NOVA", 100, 100.0);
    f.market.setPrice("NOVA", 110.0);

    const PortfolioOverview o = f.intel.overview();
    CHECK(near(o.cash, 90000.0) && near(o.equity, 101000.0));
    CHECK(near(o.longExposure, 11000.0) && near(o.shortExposure, 0.0));
    CHECK(near(o.grossExposure, 11000.0) && near(o.netExposure, 11000.0));
    CHECK(near(o.realizedPnL, 0.0) && near(o.unrealizedPnL, 1000.0) && near(o.totalPnL, 1000.0));
    CHECK(near(o.totalReturnPercent, 1.0));
    CHECK(o.positionCount == 1 && o.longPositionCount == 1 && o.shortPositionCount == 0);

    const std::vector<PositionAnalytics> pos = f.intel.positions();
    CHECK(pos.size() == 1);
    if (pos.size() == 1)
    {
        const PositionAnalytics &p = pos[0];
        CHECK(p.symbol == "NOVA" && p.name == "Nova Technologies");
        CHECK(p.sector == market::Sector::Technology && p.side == PositionSide::Long);
        CHECK(p.signedQuantity == 100);
        CHECK(near(p.avgEntryPrice, 100.0) && near(p.currentPrice, 110.0));
        CHECK(near(p.marketValue, 11000.0) && near(p.exposure, 11000.0) && near(p.costBasis, 10000.0));
        CHECK(near(p.unrealizedPnL, 1000.0) && near(p.unrealizedPnLPercent, 10.0));
        CHECK(near(p.grossWeightPercent, 100.0));
        CHECK(near(p.equityWeightPercent, 11000.0 / 101000.0 * 100.0));
    }

    const RiskAnalytics r = f.intel.riskAnalytics();
    CHECK(near(r.leverage, 11000.0 / 101000.0));
    CHECK(near(r.marginUsed, 5500.0));
    CHECK(near(r.availableMargin, 95500.0));
    CHECK(near(r.maintenanceMarginRequired, 2750.0));
    CHECK(near(r.maintenanceCushion, 98250.0));
    CHECK(near(r.buyingPower, 191000.0));
    CHECK(!r.marginCall);

    const PerformanceAnalytics perf = f.intel.performance();
    CHECK(perf.closedTrades == 0 && !perf.winRateAvailable); // only an opening trade so far
}

static void testShortPosition()
{
    std::cout << "[3] Short position (signed quantity, P&L, exposure)\n";
    Fixture f;
    doSell(f.pf, "SOLR", 50, 50.0); // opens a short
    f.market.setPrice("SOLR", 45.0);

    const PortfolioOverview o = f.intel.overview();
    CHECK(near(o.cash, 102500.0) && near(o.equity, 100250.0));
    CHECK(near(o.longExposure, 0.0) && near(o.shortExposure, 2250.0));
    CHECK(near(o.grossExposure, 2250.0) && near(o.netExposure, -2250.0));
    CHECK(near(o.unrealizedPnL, 250.0) && near(o.realizedPnL, 0.0) && near(o.totalPnL, 250.0));
    CHECK(near(o.totalReturnPercent, 0.25));
    CHECK(o.positionCount == 1 && o.longPositionCount == 0 && o.shortPositionCount == 1);

    std::vector<PositionAnalytics> pos = f.intel.positions();
    CHECK(pos.size() == 1);
    if (pos.size() == 1)
    {
        const PositionAnalytics &p = pos[0];
        CHECK(p.symbol == "SOLR" && p.side == PositionSide::Short && p.sector == market::Sector::Energy);
        CHECK(p.signedQuantity == -50);
        CHECK(near(p.avgEntryPrice, 50.0) && near(p.currentPrice, 45.0));
        CHECK(near(p.marketValue, -2250.0) && near(p.exposure, 2250.0) && near(p.costBasis, 2500.0));
        CHECK(near(p.unrealizedPnL, 250.0) && near(p.unrealizedPnLPercent, 10.0)); // price fell: short profits
        CHECK(near(p.grossWeightPercent, 100.0));
        CHECK(near(p.equityWeightPercent, -2250.0 / 100250.0 * 100.0));
    }

    const AllocationAnalytics a = f.intel.allocation();
    CHECK(near(a.exposure.shortPercentOfGross, 100.0) && near(a.exposure.longPercentOfGross, 0.0));
    CHECK(a.sectors.size() == 1);
    if (a.sectors.size() == 1)
    {
        CHECK(a.sectors[0].sector == market::Sector::Energy);
        CHECK(near(a.sectors[0].shortValue, 2250.0) && near(a.sectors[0].longValue, 0.0));
        CHECK(near(a.sectors[0].netValue, -2250.0));
    }
    CHECK(f.intel.performance().closedTrades == 0); // an OpenShort is not a completed trade

    // Price rises: the short loses.
    f.market.setPrice("SOLR", 60.0);
    pos = f.intel.positions();
    CHECK(pos.size() == 1);
    if (pos.size() == 1)
    {
        CHECK(near(pos[0].unrealizedPnL, -500.0) && near(pos[0].unrealizedPnLPercent, -20.0));
    }
    CHECK(near(f.intel.overview().totalPnL, f.intel.overview().equity - 100000.0));
}

static void testMixedPortfolio()
{
    std::cout << "[4] Mixed long/short portfolio\n";
    Fixture f;
    setupMixed(f);

    const PortfolioOverview o = f.intel.overview();
    CHECK(near(o.cash, 91000.0) && near(o.equity, 102500.0));
    CHECK(near(o.longExposure, 15500.0) && near(o.shortExposure, 4000.0));
    CHECK(near(o.grossExposure, 19500.0) && near(o.netExposure, 11500.0));
    CHECK(near(o.realizedPnL, 0.0) && near(o.unrealizedPnL, 2500.0) && near(o.totalPnL, 2500.0));
    CHECK(near(o.totalReturnPercent, 2.5));
    CHECK(o.positionCount == 3 && o.longPositionCount == 2 && o.shortPositionCount == 1);
    CHECK(near(o.totalPnL, o.equity - o.initialEquity)); // accounting invariant

    const std::vector<PositionAnalytics> pos = f.intel.positions();
    CHECK(pos.size() == 3);
    if (pos.size() == 3)
    {
        // Largest exposure first: NOVA 12000, SOLR 4000, BNKX 3500.
        CHECK(pos[0].symbol == "NOVA" && pos[1].symbol == "SOLR" && pos[2].symbol == "BNKX");
        CHECK(near(pos[0].marketValue, 12000.0) && near(pos[0].unrealizedPnL, 2000.0) &&
              near(pos[0].unrealizedPnLPercent, 20.0));
        CHECK(pos[1].side == PositionSide::Short && pos[1].signedQuantity == -100);
        CHECK(near(pos[1].marketValue, -4000.0) && near(pos[1].unrealizedPnL, 1000.0) &&
              near(pos[1].unrealizedPnLPercent, 20.0));
        CHECK(near(pos[2].marketValue, 3500.0) && near(pos[2].unrealizedPnL, -500.0) &&
              near(pos[2].unrealizedPnLPercent, -12.5));
        CHECK(near(pos[0].grossWeightPercent, 12000.0 / 19500.0 * 100.0));
        CHECK(near(pos[1].grossWeightPercent, 4000.0 / 19500.0 * 100.0));
        CHECK(near(pos[2].grossWeightPercent, 3500.0 / 19500.0 * 100.0));
        CHECK(near(pos[0].grossWeightPercent + pos[1].grossWeightPercent + pos[2].grossWeightPercent, 100.0));
        CHECK(near(pos[0].equityWeightPercent, 12000.0 / 102500.0 * 100.0));
        CHECK(near(pos[1].equityWeightPercent, -4000.0 / 102500.0 * 100.0));
    }

    const std::vector<PositionAnalytics> top2 = f.intel.largestPositions(2);
    CHECK(top2.size() == 2);
    if (top2.size() == 2)
    {
        CHECK(top2[0].symbol == "NOVA" && top2[1].symbol == "SOLR");
    }
    CHECK(f.intel.largestPositions(10).size() == 3);

    const RiskAnalytics r = f.intel.riskAnalytics();
    CHECK(near(r.leverage, 19500.0 / 102500.0));
    CHECK(near(r.marginUsed, 9750.0) && near(r.availableMargin, 92750.0));
    CHECK(near(r.maintenanceMarginRequired, 4875.0) && near(r.maintenanceCushion, 97625.0));
    CHECK(near(r.buyingPower, 185500.0));
    CHECK(!r.marginCall);
    CHECK(r.largestPositionSymbol == "NOVA" && near(r.largestPositionValue, 12000.0));
    CHECK(near(r.largestPositionPercent, 12000.0 / 19500.0 * 100.0));
    CHECK(r.largestSector.has_value() && *r.largestSector == market::Sector::Technology);
    CHECK(near(r.largestSectorPercent, 12000.0 / 19500.0 * 100.0));
}

static void testRealizedPnLAndWinRate()
{
    std::cout << "[5] Realized P&L, win rate and trade statistics\n";
    Fixture f;
    doBuy(f.pf, "NOVA", 100, 100.0);
    doSell(f.pf, "NOVA", 40, 110.0); // +400  (win)
    doSell(f.pf, "NOVA", 30, 95.0);  // -150  (loss)
    doSell(f.pf, "SOLR", 20, 50.0);  // opens a short
    doBuy(f.pf, "SOLR", 20, 45.0);   // covers: +100 (win)
    doSell(f.pf, "BNKX", 10, 80.0);  // opens a short
    doBuy(f.pf, "BNKX", 10, 80.0);   // covers at the same price: break-even
    f.market.setPrice("NOVA", 105.0);

    CHECK(f.pf.holdingQuantity("NOVA") == 30);
    CHECK(f.intel.positions().size() == 1); // SOLR and BNKX are flat again

    const PerformanceAnalytics p = f.intel.performance();
    CHECK(p.closedTrades == 4);
    CHECK(p.winningTrades == 2 && p.losingTrades == 1 && p.breakEvenTrades == 1);
    CHECK(p.winRateAvailable);
    CHECK(near(p.winRatePercent, 2.0 / 3.0 * 100.0));
    CHECK(near(p.averageWin, 250.0) && near(p.averageLoss, -150.0));
    CHECK(near(p.realizedPnL, 350.0) && near(p.unrealizedPnL, 150.0) && near(p.totalPnL, 500.0));
    CHECK(near(p.totalReturnPercent, 0.5));

    const PortfolioOverview o = f.intel.overview();
    CHECK(near(o.equity, 100500.0));
    CHECK(near(o.totalPnL, o.equity - 100000.0));
    CHECK(near(o.realizedPnL, 350.0));
}

static void testAllocationAndConcentration()
{
    std::cout << "[6] Allocation, concentration and largest positions\n";
    Fixture f;
    setupMixed(f);

    const AllocationAnalytics a = f.intel.allocation();
    CHECK(a.sectors.size() == 3);
    if (a.sectors.size() == 3)
    {
        CHECK(a.sectors[0].sector == market::Sector::Technology);
        CHECK(near(a.sectors[0].grossValue, 12000.0) && near(a.sectors[0].grossPercent, 12000.0 / 19500.0 * 100.0));
        CHECK(near(a.sectors[0].longValue, 12000.0) && near(a.sectors[0].shortValue, 0.0));
        CHECK(a.sectors[0].positionCount == 1);

        CHECK(a.sectors[1].sector == market::Sector::Energy);
        CHECK(near(a.sectors[1].longValue, 0.0) && near(a.sectors[1].shortValue, 4000.0));
        CHECK(near(a.sectors[1].netValue, -4000.0));
        CHECK(near(a.sectors[1].grossPercent, 4000.0 / 19500.0 * 100.0));

        CHECK(a.sectors[2].sector == market::Sector::Finance);
        CHECK(near(a.sectors[2].grossValue, 3500.0) && near(a.sectors[2].grossPercent, 3500.0 / 19500.0 * 100.0));

        CHECK(near(a.sectors[0].grossPercent + a.sectors[1].grossPercent + a.sectors[2].grossPercent, 100.0));
    }

    CHECK(near(a.exposure.longExposure, 15500.0) && near(a.exposure.shortExposure, 4000.0));
    CHECK(near(a.exposure.grossExposure, 19500.0) && near(a.exposure.netExposure, 11500.0));
    CHECK(near(a.exposure.longPercentOfGross, 15500.0 / 19500.0 * 100.0));
    CHECK(near(a.exposure.shortPercentOfGross, 4000.0 / 19500.0 * 100.0));

    const double expectedHhi = (12000.0 * 12000.0 + 4000.0 * 4000.0 + 3500.0 * 3500.0) / (19500.0 * 19500.0);
    CHECK(near(a.concentration.herfindahlIndex, expectedHhi));
    CHECK(near(a.concentration.effectivePositionCount, 1.0 / expectedHhi));
    CHECK(near(a.concentration.largestPositionPercent, 12000.0 / 19500.0 * 100.0));
    CHECK(near(a.concentration.topThreePositionsPercent, 100.0));
    CHECK(near(a.concentration.largestSectorPercent, 12000.0 / 19500.0 * 100.0));

    CHECK(a.largestPositions.size() == 3);
    if (a.largestPositions.size() == 3)
    {
        CHECK(a.largestPositions[0].symbol == "NOVA");
    }

    // A single position is fully concentrated.
    Fixture single;
    doBuy(single.pf, "MEDI", 10, 200.0);
    const AllocationAnalytics sa = single.intel.allocation();
    CHECK(near(sa.concentration.herfindahlIndex, 1.0));
    CHECK(near(sa.concentration.effectivePositionCount, 1.0));
    CHECK(near(sa.concentration.largestPositionPercent, 100.0));
    CHECK(sa.sectors.size() == 1 && sa.sectors[0].sector == market::Sector::Healthcare);

    // Top-N size is configurable.
    IntelligenceConfig cfg;
    cfg.topPositionCount = 2;
    Fixture two(100000.0, risk::RiskConfig::marginAccount(), cfg);
    setupMixed(two);
    CHECK(two.intel.allocation().largestPositions.size() == 2);
}

static void testConcentrationWarnings()
{
    std::cout << "[7] Concentration warnings\n";
    {
        Fixture f; // default limits: position 25%, sector 40%
        setupMixed(f);
        const RiskAnalytics r = f.intel.riskAnalytics();
        CHECK(r.warnings.size() == 2);
        const RiskWarning *pw = findWarning(r, WarningType::PositionConcentration, "NOVA");
        CHECK(pw != nullptr);
        if (pw != nullptr)
        {
            CHECK(pw->severity == WarningSeverity::Warning);
            CHECK(near(pw->value, 12000.0 / 19500.0 * 100.0) && near(pw->threshold, 25.0));
        }
        const RiskWarning *sw = findWarning(r, WarningType::SectorConcentration);
        CHECK(sw != nullptr && sw->sector == market::Sector::Technology);
        CHECK(findWarning(r, WarningType::MarginCall) == nullptr);
    }
    {
        IntelligenceConfig loose;
        loose.positionConcentrationWarnPercent = 70.0;
        loose.sectorConcentrationWarnPercent = 70.0;
        Fixture f(100000.0, risk::RiskConfig::marginAccount(), loose);
        setupMixed(f);
        CHECK(f.intel.riskAnalytics().warnings.empty()); // largest weight is 61.5%
    }
    {
        IntelligenceConfig strict;
        strict.positionConcentrationWarnPercent = 15.0;
        strict.sectorConcentrationWarnPercent = 15.0;
        Fixture f(100000.0, risk::RiskConfig::marginAccount(), strict);
        setupMixed(f);
        // 17.9% is the smallest weight, so all 3 positions and all 3 sectors warn.
        const RiskAnalytics r = f.intel.riskAnalytics();
        CHECK(r.warnings.size() == 6);
        CHECK(findWarning(r, WarningType::PositionConcentration, "BNKX") != nullptr);
        CHECK(findWarning(r, WarningType::PositionConcentration, "SOLR") != nullptr);
    }
}

static void testMarginRiskIntegration()
{
    std::cout << "[8] Risk integration: margin call, negative equity\n";
    Fixture f(10000.0);              // marginAccount: 50% initial, 25% maintenance
    doBuy(f.pf, "NOVA", 150, 100.0); // 15000 on 10000 of equity -> cash -5000 (margin loan)

    {
        const RiskAnalytics r = f.intel.riskAnalytics();
        CHECK(near(r.equity, 10000.0) && near(r.leverage, 1.5));
        CHECK(!r.marginCall);
        CHECK(findWarning(r, WarningType::MarginCall) == nullptr);
    }

    f.market.setPrice("NOVA", 40.0); // MV 6000, equity 1000 < maintenance 1500
    {
        const RiskAnalytics r = f.intel.riskAnalytics();
        CHECK(near(r.equity, 1000.0));
        CHECK(near(r.leverage, 6.0));
        CHECK(near(r.marginUsed, 3000.0) && near(r.availableMargin, -2000.0));
        CHECK(near(r.maintenanceMarginRequired, 1500.0) && near(r.maintenanceCushion, -500.0));
        CHECK(near(r.buyingPower, 0.0));
        CHECK(r.marginCall);

        CHECK(!r.warnings.empty());
        if (!r.warnings.empty())
        {
            CHECK(r.warnings.front().type == WarningType::MarginCall); // most severe first
            CHECK(r.warnings.front().severity == WarningSeverity::Critical);
            CHECK(near(r.warnings.front().value, 1000.0) && near(r.warnings.front().threshold, 1500.0));
        }
        CHECK(findWarning(r, WarningType::PositionConcentration, "NOVA") != nullptr);
        CHECK(findWarning(r, WarningType::SectorConcentration) != nullptr);

        // Must agree with RiskManager exactly (no duplicated math drifting apart).
        const risk::AccountRisk account = f.rm.assess();
        CHECK(near(r.marginUsed, account.initialMarginRequired));
        CHECK(near(r.maintenanceMarginRequired, account.maintenanceMarginRequired));
        CHECK(r.marginCall == account.marginCall());
    }

    f.market.setPrice("NOVA", 10.0); // MV 1500, equity -3500: negative equity
    {
        const RiskAnalytics r = f.intel.riskAnalytics();
        CHECK(std::isinf(r.leverage));
        CHECK(r.marginCall);
        const std::vector<PositionAnalytics> pos = f.intel.positions();
        CHECK(pos.size() == 1);
        if (pos.size() == 1)
        {
            CHECK(near(pos[0].equityWeightPercent, 0.0)); // no division by negative equity
            CHECK(near(pos[0].grossWeightPercent, 100.0));
        }
        const PortfolioOverview o = f.intel.overview();
        CHECK(near(o.equity, -3500.0));
        CHECK(near(o.totalReturnPercent, -135.0));
        CHECK(near(o.totalPnL, o.equity - o.initialEquity));
    }
}

static void testDrawdownAndHistory()
{
    std::cout << "[9] Snapshot history and maximum drawdown\n";
    Fixture f;
    doBuy(f.pf, "NOVA", 100, 100.0); // cash 90000

    CHECK(!f.intel.performance().drawdown.available); // no snapshots
    CHECK(f.intel.history().empty());

    f.intel.recordSnapshot(1000);                     // price 100: equity 100000
    CHECK(!f.intel.performance().drawdown.available); // a single point is not a drawdown series

    f.market.setPrice("NOVA", 120.0);
    f.intel.recordSnapshot(2000); // 102000
    f.market.setPrice("NOVA", 90.0);
    f.intel.recordSnapshot(3000); // 99000
    f.market.setPrice("NOVA", 95.0);
    f.intel.recordSnapshot(4000); // 99500
    f.market.setPrice("NOVA", 110.0);
    f.intel.recordSnapshot(5000); // 101000

    const std::vector<PortfolioSnapshot> &h = f.intel.history();
    CHECK(h.size() == 5);
    if (h.size() == 5)
    {
        CHECK(h[0].sequence == 0 && h[4].sequence == 4);
        CHECK(h[0].timestamp == 1000 && h[4].timestamp == 5000);
        CHECK(near(h[0].equity, 100000.0) && near(h[1].equity, 102000.0) && near(h[2].equity, 99000.0));
        CHECK(near(h[3].equity, 99500.0) && near(h[4].equity, 101000.0));
        CHECK(near(h[0].cash, 90000.0) && near(h[0].longExposure, 10000.0) && near(h[0].grossExposure, 10000.0));
        CHECK(near(h[0].shortExposure, 0.0) && h[0].positionCount == 1);
        CHECK(near(h[2].unrealizedPnL, -1000.0));
    }

    const DrawdownAnalytics d = f.intel.performance().drawdown;
    CHECK(d.available && d.snapshotCount == 5);
    CHECK(near(d.maxDrawdownPercent, 3000.0 / 102000.0 * 100.0));
    CHECK(near(d.maxDrawdownAmount, 3000.0));
    CHECK(near(d.peakEquity, 102000.0) && near(d.troughEquity, 99000.0));
    CHECK(near(d.currentDrawdownPercent, 1000.0 / 102000.0 * 100.0));

    // The clock-based overload uses the market clock.
    const PortfolioSnapshot s = f.intel.recordSnapshot();
    CHECK(s.timestamp == f.market.clock().now());
    CHECK(s.sequence == 5);

    f.intel.clearHistory();
    CHECK(f.intel.history().empty());
    CHECK(!f.intel.performance().drawdown.available);

    // Bounded history: oldest snapshots are dropped, sequence numbers keep counting.
    IntelligenceConfig cfg;
    cfg.maxHistorySnapshots = 3;
    Fixture g(100000.0, risk::RiskConfig::marginAccount(), cfg);
    doBuy(g.pf, "NOVA", 100, 100.0);
    g.intel.recordSnapshot(1);
    g.market.setPrice("NOVA", 120.0);
    g.intel.recordSnapshot(2);
    g.market.setPrice("NOVA", 90.0);
    g.intel.recordSnapshot(3);
    g.market.setPrice("NOVA", 95.0);
    g.intel.recordSnapshot(4);
    g.market.setPrice("NOVA", 110.0);
    g.intel.recordSnapshot(5);
    const std::vector<PortfolioSnapshot> &gh = g.intel.history();
    CHECK(gh.size() == 3);
    if (gh.size() == 3)
    {
        CHECK(gh[0].sequence == 2 && gh[1].sequence == 3 && gh[2].sequence == 4);
        CHECK(near(gh[0].equity, 99000.0) && near(gh[2].equity, 101000.0));
    }
    const DrawdownAnalytics gd = g.intel.performance().drawdown;
    CHECK(gd.available && gd.snapshotCount == 3);
    CHECK(near(gd.maxDrawdownPercent, 0.0)); // retained series only rises
}

static void testDeterminismAndNoMutation()
{
    std::cout << "[10] Determinism and read-only behaviour\n";
    Fixture f;
    setupMixed(f);

    const double cashBefore = f.pf.cash();
    const std::size_t historyBefore = f.pf.history().size();
    const std::size_t holdingsBefore = f.pf.holdings().size();
    const std::size_t shortsBefore = f.pf.shorts().size();
    const double priceBefore = f.market.getQuote("NOVA").price();
    const long long volumeBefore = f.market.getQuote("NOVA").volume();
    const risk::AccountRisk riskBefore = f.rm.assess();

    const PortfolioReport r1 = f.intel.report();
    const PortfolioReport r2 = f.intel.report();
    f.intel.recordSnapshot(1);
    f.intel.overview();
    f.intel.allocation();
    f.intel.riskAnalytics();
    f.intel.performance();

    CHECK(near(f.pf.cash(), cashBefore));
    CHECK(f.pf.history().size() == historyBefore);
    CHECK(f.pf.holdings().size() == holdingsBefore && f.pf.shorts().size() == shortsBefore);
    CHECK(near(f.market.getQuote("NOVA").price(), priceBefore));
    CHECK(f.market.getQuote("NOVA").volume() == volumeBefore);
    CHECK(near(f.rm.assess().equity, riskBefore.equity));

    CHECK(near(r1.overview.equity, r2.overview.equity));
    CHECK(near(r1.overview.totalPnL, r2.overview.totalPnL));
    CHECK(r1.positions.size() == r2.positions.size());
    if (r1.positions.size() == r2.positions.size())
    {
        for (std::size_t i = 0; i < r1.positions.size(); ++i)
        {
            CHECK(r1.positions[i].symbol == r2.positions[i].symbol);
            CHECK(near(r1.positions[i].marketValue, r2.positions[i].marketValue));
        }
    }
    CHECK(r1.riskAnalytics.warnings.size() == r2.riskAnalytics.warnings.size());

    // Report pieces agree with the individual accessors.
    CHECK(near(r1.overview.equity, f.intel.overview().equity));
    CHECK(near(r1.allocation.concentration.herfindahlIndex, f.intel.allocation().concentration.herfindahlIndex));
    CHECK(near(r1.riskAnalytics.leverage, f.intel.riskAnalytics().leverage));

    // An identical, independently built portfolio gives identical results.
    Fixture g;
    setupMixed(g);
    const PortfolioReport r3 = g.intel.report();
    CHECK(near(r3.overview.equity, r1.overview.equity));
    CHECK(near(r3.allocation.concentration.herfindahlIndex, r1.allocation.concentration.herfindahlIndex));
    CHECK(r3.positions.size() == r1.positions.size());
    if (!r3.positions.empty() && !r1.positions.empty())
    {
        CHECK(r3.positions.front().symbol == r1.positions.front().symbol);
    }

    // Ties in exposure are ordered by symbol.
    Fixture t;
    doBuy(t.pf, "NOVA", 10, 100.0); // 1000
    doBuy(t.pf, "BNKX", 10, 80.0);
    t.market.setPrice("BNKX", 100.0); // 1000: same exposure as NOVA
    const std::vector<PositionAnalytics> tied = t.intel.positions();
    CHECK(tied.size() == 2);
    if (tied.size() == 2)
    {
        CHECK(tied[0].symbol == "BNKX" && tied[1].symbol == "NOVA");
    }
}

static void testConfigValidation()
{
    std::cout << "[11] Configuration validation\n";
    Fixture f;

    IntelligenceConfig bad;
    bad.positionConcentrationWarnPercent = 0.0;
    CHECK_THROWS((void)PortfolioIntelligence(f.market, f.pf, f.rm, 1.0, bad), std::invalid_argument);

    bad = IntelligenceConfig();
    bad.sectorConcentrationWarnPercent = 101.0;
    CHECK_THROWS((void)PortfolioIntelligence(f.market, f.pf, f.rm, 1.0, bad), std::invalid_argument);

    bad = IntelligenceConfig();
    bad.maxHistorySnapshots = 0;
    CHECK_THROWS((void)PortfolioIntelligence(f.market, f.pf, f.rm, 1.0, bad), std::invalid_argument);

    CHECK_THROWS((void)PortfolioIntelligence(f.market, f.pf, f.rm, -1.0), std::invalid_argument);
    CHECK_THROWS((void)PortfolioIntelligence(f.market, f.pf, f.rm, std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
}

static void testRealExecutionIntegration()
{
    std::cout << "[12] Integration with the real ExecutionService / settlement path\n";
    market::Market m = makeMarket();
    portfolio::Portfolio pf(100000.0);
    execution::ExecutionService svc(m, pf, execution::LiquidityConfig(), risk::RiskConfig::marginAccount());
    PortfolioIntelligence intel(m, pf, svc.riskManager(), 100000.0);

    // No shares owned: a market sell opens a short through the full pipeline.
    svc.placeMarketOrder(execution::OrderSide::Sell, "NOVA", 100);

    PortfolioOverview o = intel.overview();
    CHECK(o.shortPositionCount == 1 && o.longPositionCount == 0);
    CHECK(near(o.shortExposure, 100.0 * m.getQuote("NOVA").price()));
    CHECK(near(o.equity, svc.riskManager().assess().equity));
    CHECK(near(o.totalPnL, o.equity - 100000.0));

    std::vector<PositionAnalytics> pos = intel.positions();
    CHECK(pos.size() == 1);
    if (pos.size() == 1)
    {
        CHECK(pos[0].side == PositionSide::Short && pos[0].signedQuantity == -100);
        CHECK(pos[0].symbol == "NOVA");
    }

    // Partial cover: sells at the bid, covers at the ask, so the completed trade is a loss.
    svc.placeMarketOrder(execution::OrderSide::Buy, "NOVA", 40);
    pos = intel.positions();
    CHECK(pos.size() == 1);
    if (pos.size() == 1)
    {
        CHECK(pos[0].signedQuantity == -60);
    }

    const PerformanceAnalytics perf = intel.performance();
    CHECK(perf.closedTrades == 1 && perf.losingTrades == 1 && perf.winningTrades == 0);
    CHECK(perf.winRateAvailable && near(perf.winRatePercent, 0.0));
    CHECK(perf.realizedPnL < 0.0);
    CHECK(near(perf.realizedPnL, portfolio::PortfolioAnalyzer::totalRealizedPnL(pf)));

    o = intel.overview();
    CHECK(near(o.totalPnL, o.equity - 100000.0)); // holds with real settlement too

    const RiskAnalytics r = intel.riskAnalytics();
    CHECK(near(r.marginUsed, svc.riskManager().assess().initialMarginRequired));
    CHECK(!r.marginCall);
}

int main()
{
    testEmptyPortfolio();
    testLongPosition();
    testShortPosition();
    testMixedPortfolio();
    testRealizedPnLAndWinRate();
    testAllocationAndConcentration();
    testConcentrationWarnings();
    testMarginRiskIntegration();
    testDrawdownAndHistory();
    testDeterminismAndNoMutation();
    testConfigValidation();
    testRealExecutionIntegration();

    std::cout << "\n"
              << g_checks << " checks, " << g_failures << " failed\n";
    std::cout << (g_failures == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return g_failures == 0 ? 0 : 1;
}