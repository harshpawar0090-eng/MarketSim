#include <cmath>
#include <iostream>
#include <string>
#include "../Execution/ExecutionService.h"
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"

using namespace execution;

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

static market::Market makeMarket()
{
    market::Market m;
    m.addStock(market::Stock("NOVA", "Nova Technologies", market::Sector::Technology, 150.00));
    m.addStock(market::Stock("QBIT", "Qubit Systems", market::Sector::Technology, 320.50));
    return m;
}

struct Fixture
{
    market::Market market;
    portfolio::Portfolio pf;
    ExecutionService svc;

    explicit Fixture(double cash = 100000.0)
        : market(makeMarket()), pf(cash), svc(market, pf) {}
};

static void testOrderValidation()
{
    std::cout << "[1] Order validation\n";
    CHECK_THROWS((Order(1, "A", "X", OrderSide::Buy, OrderType::Limit, 0, 10.0, 0)), ExecutionError);
    CHECK_THROWS((Order(1, "A", "X", OrderSide::Buy, OrderType::Limit, 10, 0.0, 0)), ExecutionError);
    CHECK_THROWS((Order(1, "A", "X", OrderSide::Buy, OrderType::Limit, 10, -5.0, 0)), ExecutionError);
    Order ok(7, "A", "X", OrderSide::Sell, OrderType::Limit, 10, 10.004, 0);
    CHECK(ok.status() == OrderStatus::New);
    CHECK(near(ok.limitPrice(), 10.00));
    CHECK(ok.id() == 7);
}

static void testPriceTimePriority()
{
    std::cout << "[2] Price-time priority + partial fill of resting order\n";
    MatchingEngine e;
    std::time_t t = 1000;
    auto a = e.submit("A", "TEST", OrderSide::Sell, OrderType::Limit, 100, 10.00, t);
    auto b = e.submit("B", "TEST", OrderSide::Sell, OrderType::Limit, 100, 10.00, t);
    auto c = e.submit("C", "TEST", OrderSide::Sell, OrderType::Limit, 50, 9.99, t);
    auto d = e.submit("D", "TEST", OrderSide::Buy, OrderType::Market, 180, 0.0, t);

    CHECK(d.fills.size() == 3);
    if (d.fills.size() == 3)
    {
        CHECK(d.fills[0].sellParticipant == "C" && near(d.fills[0].price, 9.99) && d.fills[0].quantity == 50);
        CHECK(d.fills[1].sellParticipant == "A" && near(d.fills[1].price, 10.00) && d.fills[1].quantity == 100);
        CHECK(d.fills[2].sellParticipant == "B" && near(d.fills[2].price, 10.00) && d.fills[2].quantity == 30);
    }
    CHECK(d.order.status() == OrderStatus::Filled);
    CHECK(e.getOrder(a.order.id())->status() == OrderStatus::Filled);
    CHECK(e.getOrder(c.order.id())->status() == OrderStatus::Filled);
    auto bState = e.getOrder(b.order.id());
    CHECK(bState && bState->status() == OrderStatus::PartiallyFilled && bState->remainingQuantity() == 70);
    CHECK(e.book("TEST")->askLevels().size() == 1);
    CHECK(e.book("TEST")->askLevels()[0].quantity == 70);
}

static void testLimitBehaviour()
{
    std::cout << "[3] Limit orders: resting, crossing, trade at resting price\n";
    MatchingEngine e;
    std::time_t t = 1000;
    e.submit("A", "TEST", OrderSide::Sell, OrderType::Limit, 100, 10.00, t);

    auto rest = e.submit("D", "TEST", OrderSide::Buy, OrderType::Limit, 100, 9.99, t);
    CHECK(rest.fills.empty());
    CHECK(rest.order.status() == OrderStatus::New);
    CHECK(e.book("TEST")->bestBid() && near(*e.book("TEST")->bestBid(), 9.99));

    auto cross = e.submit("E", "TEST", OrderSide::Buy, OrderType::Limit, 250, 10.50, t);
    CHECK(cross.fills.size() == 1);
    CHECK(cross.fills.size() == 1 && near(cross.fills[0].price, 10.00)); // resting price, not 10.50
    CHECK(cross.order.status() == OrderStatus::PartiallyFilled);
    CHECK(cross.order.remainingQuantity() == 150);
    CHECK(e.book("TEST")->bestBid() && near(*e.book("TEST")->bestBid(), 10.50));
    CHECK(!e.book("TEST")->bestAsk().has_value());
}

static void testMarketNoLiquidity()
{
    std::cout << "[4] Market order with no / partial liquidity\n";
    MatchingEngine e;
    std::time_t t = 1000;
    auto none = e.submit("D", "TEST", OrderSide::Buy, OrderType::Market, 10, 0.0, t);
    CHECK(none.fills.empty());
    CHECK(none.order.status() == OrderStatus::Cancelled);

    e.submit("A", "TEST", OrderSide::Sell, OrderType::Limit, 50, 10.00, t);
    auto part = e.submit("D", "TEST", OrderSide::Buy, OrderType::Market, 80, 0.0, t);
    CHECK(part.fills.size() == 1);
    CHECK(part.order.filledQuantity() == 50);
    CHECK(part.order.status() == OrderStatus::Cancelled);
    CHECK(e.book("TEST")->empty());
}

static void testCancellation()
{
    std::cout << "[5] Cancellation\n";
    MatchingEngine e;
    std::time_t t = 1000;
    auto r = e.submit("A", "TEST", OrderSide::Buy, OrderType::Limit, 100, 10.00, t);
    auto cancelled = e.cancel(r.order.id());
    CHECK(cancelled.has_value());
    CHECK(cancelled && cancelled->status() == OrderStatus::Cancelled);
    CHECK(e.getOrder(r.order.id())->status() == OrderStatus::Cancelled);
    CHECK(!e.cancel(r.order.id()).has_value()); // already gone
    CHECK(!e.cancel(99999).has_value());        // unknown id
    CHECK(e.book("TEST")->empty());

    auto s = e.submit("A", "TEST", OrderSide::Sell, OrderType::Limit, 10, 10.00, t);
    e.submit("B", "TEST", OrderSide::Buy, OrderType::Market, 10, 0.0, t);
    CHECK(!e.cancel(s.order.id()).has_value()); // filled orders cannot be cancelled
}

static void testSelfTradePrevention()
{
    std::cout << "[6] Self-trade prevention\n";
    MatchingEngine e;
    std::time_t t = 1000;
    auto bid = e.submit("A", "TEST", OrderSide::Buy, OrderType::Limit, 100, 10.00, t);
    auto sell = e.submit("A", "TEST", OrderSide::Sell, OrderType::Market, 50, 0.0, t);
    CHECK(sell.fills.empty());
    CHECK(e.getOrder(bid.order.id())->status() == OrderStatus::Cancelled);
    CHECK(e.fills().empty());
}

static void testServiceBuy()
{
    std::cout << "[7] Service: market buy settles into Portfolio + Market stats\n";
    Fixture f;
    auto r = f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 100);
    CHECK(r.order.status() == OrderStatus::Filled);
    CHECK(r.filledQuantity() == 100);
    CHECK(!r.fills.empty());

    double cost = 0.0;
    for (const auto &fill : r.fills)
        cost += fill.price * fill.quantity;

    CHECK(near(f.pf.cash(), 100000.0 - cost));
    CHECK(f.pf.holdingQuantity("NOVA") == 100);
    CHECK(f.market.executedVolume("NOVA") == 100);
    CHECK(near(f.market.executedTurnover("NOVA"), cost));
    CHECK(f.market.getQuote("NOVA").volume() == 100);
    CHECK(near(f.market.getQuote("NOVA").turnover(), cost));
    CHECK(f.pf.history().size() == r.fills.size());
    CHECK(f.market.getQuote("NOVA").bidPrice() < f.market.getQuote("NOVA").askPrice());
    CHECK(f.market.executedVolume("QBIT") == 0);
}

static void testServiceMultiLevel()
{
    std::cout << "[8] Service: order walking several price levels (partial fills of makers)\n";

    Fixture f(500000.0);

    auto r = f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 700);

    CHECK(r.fills.size() == 2);
    if (r.fills.size() == 2)
    {
        CHECK(r.fills[0].quantity == 500);
        CHECK(r.fills[1].quantity == 200);
        CHECK(r.fills[1].price >= r.fills[0].price);
    }

    CHECK(f.pf.holdingQuantity("NOVA") == 700);
    CHECK(f.market.executedVolume("NOVA") == 700);
}

static void testServiceRejections()
{
    std::cout << "[9] Service: rejections leave state untouched\n";
    Fixture f;
    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 2000), OrderRejectedError); // ~300k > 100k
    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Buy, "NOPE", 1), OrderRejectedError);
    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 0), OrderRejectedError);
    CHECK_THROWS(f.svc.placeLimitOrder(OrderSide::Buy, "NOVA", 1, -1.0), OrderRejectedError);
    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Sell, "NOVA", 10), OrderRejectedError); // none held
    CHECK(near(f.pf.cash(), 100000.0));
    CHECK(f.svc.engine().fills().empty());
    CHECK(f.market.executedVolume("NOVA") == 0);
}

static void testServiceSell()
{
    std::cout << "[10] Service: sell, realized P&L, oversell rejected\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 100);
    double avg = f.pf.holdings().at("NOVA").avgCost();
    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Sell, "NOVA", 150), OrderRejectedError);

    auto s = f.svc.placeMarketOrder(OrderSide::Sell, "NOVA", 50);
    CHECK(s.order.status() == OrderStatus::Filled);
    CHECK(f.pf.holdingQuantity("NOVA") == 50);

    double expected = 0.0;
    for (const auto &fill : s.fills)
        expected += (fill.price - avg) * fill.quantity;
    CHECK(near(portfolio::PortfolioAnalyzer::totalRealizedPnL(f.pf), expected));
    CHECK(f.market.executedVolume("NOVA") == 150);
}

static void testServiceLimitAndCancel()
{
    std::cout << "[11] Service: resting limit order, reservation, cancel\n";
    Fixture f;
    auto r = f.svc.placeLimitOrder(OrderSide::Buy, "NOVA", 10, 100.00);
    CHECK(r.order.status() == OrderStatus::New);
    CHECK(r.fills.empty());
    CHECK(near(f.svc.reservedCash(), 1000.0));
    CHECK(f.svc.openUserOrders().size() == 1);
    CHECK(near(f.pf.cash(), 100000.0)); // cash is reserved, not debited

    CHECK(f.svc.cancelOrder(r.order.id()));
    CHECK(near(f.svc.reservedCash(), 0.0));
    CHECK(f.svc.getOrder(r.order.id())->status() == OrderStatus::Cancelled);
    CHECK(!f.svc.cancelOrder(r.order.id()));
    CHECK(!f.svc.cancelOrder(424242));

    auto m = f.svc.placeLimitOrder(OrderSide::Buy, "NOVA", 10, 200.00); // marketable
    CHECK(m.order.status() == OrderStatus::Filled);
    CHECK(!m.fills.empty() && m.fills[0].price < 151.0); // price improvement vs 200 limit
}

static void testServicePartialLimit()
{
    std::cout << "[12] Service: limit order partially filled, remainder rests\n";
    Fixture f(10000000.0);
    auto r = f.svc.placeLimitOrder(OrderSide::Buy, "NOVA", 3000, 160.00);
    CHECK(r.order.status() == OrderStatus::PartiallyFilled);
    CHECK(r.filledQuantity() == 2500); // whole ask ladder: 5 x 500
    CHECK(r.order.remainingQuantity() == 500);
    CHECK(near(f.svc.reservedCash(), 500 * 160.00));
    CHECK(f.pf.holdingQuantity("NOVA") == 2500);
    const auto *bk = f.svc.engine().book("NOVA");
    CHECK(bk && bk->bestBid() && near(*bk->bestBid(), 160.00));
}

static void testPassiveFillOnRequote()
{
    std::cout << "[13] Service: resting sell filled passively when price moves up\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 100);
    double cashAfterBuy = f.pf.cash();

    auto s = f.svc.placeLimitOrder(OrderSide::Sell, "NOVA", 100, 150.50);
    CHECK(s.order.status() == OrderStatus::New);
    CHECK(f.svc.reservedShares("NOVA") == 100);
    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Sell, "NOVA", 1), OrderRejectedError); // all reserved

    f.market.setPrice("NOVA", 151.00);
    f.svc.refreshLiquidity("NOVA");

    CHECK(f.svc.getOrder(s.order.id())->status() == OrderStatus::Filled);
    CHECK(f.pf.holdingQuantity("NOVA") == 0);
    CHECK(near(f.pf.cash(), cashAfterBuy + 100 * 150.50));
    CHECK(f.pf.history().back().side == portfolio::TransactionSide::Sell);
    CHECK(near(f.pf.history().back().price, 150.50)); // executed at the resting price
    CHECK(f.market.executedVolume("NOVA") == 200);
    CHECK(near(f.svc.reservedShares("NOVA"), 0));
}

static void testAutoRequote()
{
    std::cout << "[14] Service: maker re-quotes automatically after a price change\n";
    Fixture f;
    f.market.setPrice("NOVA", 160.00);
    auto r = f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 10);
    CHECK(r.order.status() == OrderStatus::Filled);
    CHECK(!r.fills.empty() && r.fills[0].price > 158.0 && r.fills[0].price < 162.0);
}

static void testExistingTickStillWorks()
{
    std::cout << "[15] Phase 2 behaviour preserved (tick)\n";
    Fixture f;
    f.market.tick();
    CHECK(f.market.getQuote("NOVA").volume() > 0); // simulated background volume still added
    CHECK(f.market.executedVolume("NOVA") == 0);   // ...but not counted as executed
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 10);
    CHECK(f.market.executedVolume("NOVA") == 10);
    CHECK(f.market.priceHistory("NOVA").size() == 2);
}

int main()
{
    testOrderValidation();
    testPriceTimePriority();
    testLimitBehaviour();
    testMarketNoLiquidity();
    testCancellation();
    testSelfTradePrevention();
    testServiceBuy();
    testServiceMultiLevel();
    testServiceRejections();
    testServiceSell();
    testServiceLimitAndCancel();
    testServicePartialLimit();
    testPassiveFillOnRequote();
    testAutoRequote();
    testExistingTickStillWorks();

    std::cout << "\n"
              << g_checks << " checks, " << g_failures << " failed\n";
    std::cout << (g_failures == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return g_failures == 0 ? 0 : 1;
}