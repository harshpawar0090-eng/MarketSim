#include <cmath>
#include <iostream>
#include <string>
#include "../Broker/Broker.h"
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

// Moves the NOVA price (like a tick would) without random noise.
static void moveTo(Fixture &f, double price) { f.market.setPrice("NOVA", price); }

static void testOrderModel()
{
    std::cout << "[S1] Order model: stop types and lifecycle\n";
    Order s(5, "A", "X", OrderSide::Sell, OrderType::StopLoss, 10, 0.0, 0, 145.004);
    CHECK(s.status() == OrderStatus::WaitingForTrigger);
    CHECK(s.isPending() && !s.isActive() && s.isOpen());
    CHECK(near(s.stopPrice(), 145.00));
    CHECK(s.executionType() == OrderType::Market);
    CHECK(!s.triggersAt(145.01));
    CHECK(s.triggersAt(145.00));
    CHECK(s.triggersAt(100.00));

    Order tp(6, "A", "X", OrderSide::Sell, OrderType::TakeProfit, 10, 0.0, 0, 160.0);
    CHECK(!tp.triggersAt(159.99));
    CHECK(tp.triggersAt(160.00));

    Order bs(7, "A", "X", OrderSide::Buy, OrderType::Stop, 10, 0.0, 0, 155.0);
    CHECK(!bs.triggersAt(154.99));
    CHECK(bs.triggersAt(155.00));

    Order sl(8, "A", "X", OrderSide::Sell, OrderType::StopLimit, 10, 144.504, 0, 145.0);
    CHECK(sl.executionType() == OrderType::Limit);
    CHECK(near(sl.limitPrice(), 144.50));

    CHECK_THROWS((Order(9, "A", "X", OrderSide::Sell, OrderType::Stop, 10, 0.0, 0, 0.0)), ExecutionError);
    CHECK_THROWS((Order(9, "A", "X", OrderSide::Sell, OrderType::StopLimit, 10, 0.0, 0, 145.0)), ExecutionError);

    CHECK(orderTypeToString(OrderType::StopLimit) == "STOP_LIMIT");
    CHECK(orderTypeToString(OrderType::TakeProfit) == "TAKE_PROFIT");
    CHECK(orderStatusToString(OrderStatus::WaitingForTrigger) == "WAITING_FOR_TRIGGER");

    // A pending order cannot be filled until activated.
    CHECK_THROWS(s.applyFill(1), ExecutionError);
    s.activate();
    CHECK(s.status() == OrderStatus::New && s.triggered() && s.isActive());
    CHECK_THROWS(s.activate(), ExecutionError);
    s.applyFill(4);
    CHECK(s.status() == OrderStatus::PartiallyFilled);

    Order c(10, "A", "X", OrderSide::Buy, OrderType::Stop, 10, 0.0, 0, 155.0);
    c.cancel();
    CHECK(c.status() == OrderStatus::Cancelled);
}

static void testEngineOnlyTakesExecutable()
{
    std::cout << "[S2] MatchingEngine only matches executable orders\n";
    MatchingEngine e;
    CHECK_THROWS(e.submit("A", "T", OrderSide::Sell, OrderType::StopLoss, 10, 0.0, 0), ExecutionError);
    Order pending(1, "A", "T", OrderSide::Sell, OrderType::Stop, 10, 0.0, 0, 145.0);
    CHECK_THROWS(e.submitOrder(pending, 0), ExecutionError);
    CHECK(e.book("T") == nullptr || e.book("T")->empty());

    // An activated stop is matched like any other order.
    e.submit("MM", "T", OrderSide::Buy, OrderType::Limit, 100, 144.00, 0);
    pending.activate();
    SubmitResult r = e.submitOrder(pending, 5);
    CHECK(r.fills.size() == 1);
    CHECK(r.fills.size() == 1 && near(r.fills[0].price, 144.00) && r.fills[0].quantity == 10);
    CHECK(r.order.status() == OrderStatus::Filled);
    CHECK(r.order.id() == 1);
}

static void testPendingBeforeTrigger()
{
    std::cout << "[S3] Stop order stays pending (and outside the book) before its trigger\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 200);

    auto r = f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 100, 145.00);
    const OrderId id = r.order.id();
    CHECK(r.order.status() == OrderStatus::WaitingForTrigger);
    CHECK(r.fills.empty());

    const OrderBook *bk = f.svc.engine().book("NOVA");
    CHECK(bk != nullptr && bk->ordersFor(kUserId).empty()); // not in the order book
    CHECK(!f.svc.engine().getOrder(id).has_value());        // the engine has never seen it
    CHECK(f.svc.openUserOrders().size() == 1);
    CHECK(f.svc.pendingStopOrders().size() == 1);
    CHECK(f.svc.reservedShares("NOVA") == 100);
    CHECK(f.pf.holdingQuantity("NOVA") == 200);

    moveTo(f, 146.00); // above the stop: no trigger
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.empty());
    CHECK(f.svc.getOrder(id) && f.svc.getOrder(id)->status() == OrderStatus::WaitingForTrigger);
    CHECK(f.pf.holdingQuantity("NOVA") == 200);
    CHECK(f.svc.reservedShares("NOVA") == 100);
}

static void testStopLossTriggers()
{
    std::cout << "[S4] Stop-loss triggers at the boundary and executes via normal matching\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 200);
    auto r = f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 100, 145.00);
    const OrderId id = r.order.id();

    moveTo(f, 145.00); // exactly at the stop: triggers (price <= stop)
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.size() == 1);
    if (upd.triggers.size() == 1)
    {
        const TriggerEvent &ev = upd.triggers[0];
        CHECK(ev.orderId == id);
        CHECK(ev.status == OrderStatus::Filled);
        CHECK(ev.filledQuantity == 100);
        CHECK(!ev.rejected);
        CHECK(near(ev.marketPrice, 145.00));
    }
    auto o = f.svc.getOrder(id);
    CHECK(o && o->status() == OrderStatus::Filled && o->triggered() && o->type() == OrderType::StopLoss);
    CHECK(f.pf.holdingQuantity("NOVA") == 100);
    CHECK(f.svc.reservedShares("NOVA") == 0);
    CHECK(!upd.userFills.empty());
    for (const auto &fill : upd.userFills)
    {
        CHECK(fill.sellParticipant == kUserId && fill.buyParticipant == kMarketMakerId);
        CHECK(fill.price < 145.00); // executed against bids just below the market
    }
    CHECK(portfolio::PortfolioAnalyzer::totalRealizedPnL(f.pf) < 0.0);
    CHECK(f.market.executedVolume("NOVA") == 300);  // 200 bought + 100 sold
    CHECK(f.svc.engine().getOrder(id).has_value()); // now known to the engine
}

static void testBuyStopAndMatching()
{
    std::cout << "[S5] Buy stop triggers when price rises to the stop; settles normally\n";
    Fixture f;
    auto r = f.svc.placeStopOrder(OrderType::Stop, OrderSide::Buy, "NOVA", 50, 155.00);
    const OrderId id = r.order.id();
    CHECK(near(f.svc.reservedCash(), 155.00 * 50));
    CHECK(near(f.pf.cash(), 100000.0)); // reserved, not debited

    moveTo(f, 154.00);
    auto none = f.svc.onMarketUpdate();
    CHECK(none.triggers.empty());
    CHECK(f.svc.getOrder(id)->status() == OrderStatus::WaitingForTrigger);

    moveTo(f, 155.50);
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.size() == 1);
    CHECK(f.svc.getOrder(id)->status() == OrderStatus::Filled);
    CHECK(f.pf.holdingQuantity("NOVA") == 50);

    double cost = 0.0;
    for (const auto &fill : upd.userFills)
        cost += fill.value();
    CHECK(near(f.pf.cash(), 100000.0 - cost));
    CHECK(!f.svc.engine().fills().empty());
    const Fill &last = f.svc.engine().fills().back();
    CHECK(last.buyParticipant == kUserId && last.sellParticipant == kMarketMakerId);
    CHECK(last.aggressorSide == OrderSide::Buy);
    CHECK(last.price > 155.50 && last.price < 156.50);
    CHECK(f.market.executedVolume("NOVA") == 50);
    CHECK(f.market.getQuote("NOVA").volume() == 50);
    CHECK(near(f.svc.reservedCash(), 0.0));
}

static void testStopLimitRests()
{
    std::cout << "[S6] Stop-limit triggers, then rests at its limit and fills at the limit price\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 200);
    auto r = f.svc.placeStopOrder(OrderType::StopLimit, OrderSide::Sell, "NOVA", 100, 145.00, 144.50);
    const OrderId id = r.order.id();

    moveTo(f, 144.00); // triggers, but bids (<= 143.93) are below the 144.50 limit
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.size() == 1);
    if (upd.triggers.size() == 1)
    {
        CHECK(upd.triggers[0].status == OrderStatus::New);
        CHECK(upd.triggers[0].filledQuantity == 0);
        CHECK(!upd.triggers[0].rejected);
    }
    auto o = f.svc.getOrder(id);
    CHECK(o && o->status() == OrderStatus::New && o->triggered() && o->remainingQuantity() == 100);
    const OrderBook *bk = f.svc.engine().book("NOVA");
    auto mine = bk ? bk->ordersFor(kUserId) : std::vector<Order>();
    CHECK(mine.size() == 1);
    CHECK(mine.size() == 1 && mine[0].id() == id && near(mine[0].limitPrice(), 144.50));
    CHECK(f.pf.holdingQuantity("NOVA") == 200); // nothing sold below the limit
    CHECK(f.svc.reservedShares("NOVA") == 100);

    const double cashBefore = f.pf.cash();
    moveTo(f, 146.00); // maker re-quotes bids above 144.50 -> passive fill at the resting price
    auto upd2 = f.svc.onMarketUpdate();
    CHECK(upd2.triggers.empty());
    CHECK(upd2.userFills.size() == 1);
    CHECK(upd2.userFills.size() == 1 && near(upd2.userFills[0].price, 144.50));
    CHECK(f.svc.getOrder(id)->status() == OrderStatus::Filled);
    CHECK(f.pf.holdingQuantity("NOVA") == 100);
    CHECK(near(f.pf.cash(), cashBefore + 100 * 144.50));
}

static void testStopLimitMarketable()
{
    std::cout << "[S7] Stop-limit that is immediately marketable never trades below its limit\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 200);
    auto r = f.svc.placeStopOrder(OrderType::StopLimit, OrderSide::Sell, "NOVA", 100, 145.00, 144.50);

    moveTo(f, 144.80);
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.size() == 1);
    CHECK(f.svc.getOrder(r.order.id())->status() == OrderStatus::Filled);
    CHECK(!upd.userFills.empty());
    for (const auto &fill : upd.userFills)
        CHECK(fill.price >= 144.50);
    CHECK(f.pf.holdingQuantity("NOVA") == 100);
}

static void testTakeProfit()
{
    std::cout << "[S8] Take-profit triggers when the price rises to the target\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 200);

    // A target at/below the current price would fire immediately -> rejected.
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::TakeProfit, OrderSide::Sell, "NOVA", 100, 149.00),
                 OrderRejectedError);

    auto r = f.svc.placeStopOrder(OrderType::TakeProfit, OrderSide::Sell, "NOVA", 100, 160.00);
    const OrderId id = r.order.id();
    CHECK(r.order.status() == OrderStatus::WaitingForTrigger);

    moveTo(f, 159.00);
    CHECK(f.svc.onMarketUpdate().triggers.empty());
    CHECK(f.svc.getOrder(id)->status() == OrderStatus::WaitingForTrigger);

    moveTo(f, 160.50);
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.size() == 1);
    auto o = f.svc.getOrder(id);
    CHECK(o && o->status() == OrderStatus::Filled && o->type() == OrderType::TakeProfit);
    CHECK(f.pf.holdingQuantity("NOVA") == 100);
    CHECK(portfolio::PortfolioAnalyzer::totalRealizedPnL(f.pf) > 0.0);
}

static void testCancelPending()
{
    std::cout << "[S9] Cancelling a pending stop order\n";
    Fixture f;
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 200);
    auto r = f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 100, 145.00);
    const OrderId id = r.order.id();

    CHECK(f.svc.cancelOrder(id));
    CHECK(f.svc.getOrder(id)->status() == OrderStatus::Cancelled);
    CHECK(f.svc.reservedShares("NOVA") == 0);
    CHECK(f.svc.openUserOrders().empty());
    CHECK(f.svc.pendingStopOrders().empty());
    CHECK(!f.svc.cancelOrder(id));     // already cancelled
    CHECK(!f.svc.cancelOrder(424242)); // unknown id

    moveTo(f, 144.00); // would have triggered
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.empty());
    CHECK(f.pf.holdingQuantity("NOVA") == 200);

    // The released shares can back a new stop for the full position.
    auto again = f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 200, 140.00);
    CHECK(again.order.status() == OrderStatus::WaitingForTrigger);
}

static void testPartialFillAfterTrigger()
{
    std::cout << "[S10] Partial fill after trigger; remainder rests\n";
    Fixture f(10000000.0);
    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 1200);
    auto r = f.svc.placeStopOrder(OrderType::StopLimit, OrderSide::Sell, "NOVA", 1200, 145.00, 144.60);
    const OrderId id = r.order.id();
    const double cashBefore = f.pf.cash();

    // At 144.80 the bids are 144.73 (500), 144.66 (500), 144.58 (500)...
    // Only the first two are >= the 144.60 limit, so 1000 fill and 200 rest.
    moveTo(f, 144.80);
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.size() == 1);
    if (upd.triggers.size() == 1)
    {
        CHECK(upd.triggers[0].status == OrderStatus::PartiallyFilled);
        CHECK(upd.triggers[0].filledQuantity == 1000);
    }
    CHECK(upd.userFills.size() == 2);
    auto o = f.svc.getOrder(id);
    CHECK(o && o->status() == OrderStatus::PartiallyFilled && o->remainingQuantity() == 200);
    CHECK(f.pf.holdingQuantity("NOVA") == 200);
    CHECK(f.svc.reservedShares("NOVA") == 200);

    const OrderBook *bk = f.svc.engine().book("NOVA");
    auto mine = bk ? bk->ordersFor(kUserId) : std::vector<Order>();
    CHECK(mine.size() == 1);
    CHECK(mine.size() == 1 && mine[0].remainingQuantity() == 200 && near(mine[0].limitPrice(), 144.60));

    double proceeds = 0.0;
    for (const auto &fill : upd.userFills)
        proceeds += fill.value();
    CHECK(near(f.pf.cash(), cashBefore + proceeds));
    CHECK(f.svc.cancelOrder(id)); // the resting remainder can be cancelled
}

static void testPlacementRejections()
{
    std::cout << "[S11] Stop placement validation\n";
    Fixture f;
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 10, 145.0),
                 OrderRejectedError); // no shares held

    f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 200);
    const std::size_t fillsBefore = f.svc.engine().fills().size();

    CHECK_THROWS(f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Buy, "NOVA", 10, 155.0),
                 OrderRejectedError); // stop-loss must be a sell
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::TakeProfit, OrderSide::Buy, "NOVA", 10, 140.0),
                 OrderRejectedError); // take-profit must be a sell
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 10, 151.0),
                 OrderRejectedError); // would trigger immediately
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::Stop, OrderSide::Buy, "NOVA", 10, 149.0),
                 OrderRejectedError); // would trigger immediately
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::Stop, OrderSide::Buy, "NOVA", 10, 0.0), OrderRejectedError);
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::Stop, OrderSide::Buy, "NOVA", 10, -5.0), OrderRejectedError);
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::StopLimit, OrderSide::Sell, "NOVA", 10, 145.0, -1.0),
                 OrderRejectedError);
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::Stop, OrderSide::Buy, "NOVA", 0, 155.0), OrderRejectedError);
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::Stop, OrderSide::Buy, "NOPE", 10, 155.0), OrderRejectedError);
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::Limit, OrderSide::Buy, "NOVA", 10, 155.0), OrderRejectedError);

    // Shares are reserved: 150 + 50 fit in 200, a further 1 does not.
    CHECK(f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 150, 140.0).order.isPending());
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::TakeProfit, OrderSide::Sell, "NOVA", 100, 170.0),
                 OrderRejectedError);
    CHECK(f.svc.placeStopOrder(OrderType::TakeProfit, OrderSide::Sell, "NOVA", 50, 170.0).order.isPending());
    CHECK_THROWS(f.svc.placeStopOrder(OrderType::Stop, OrderSide::Sell, "NOVA", 1, 130.0), OrderRejectedError);

    CHECK(f.svc.engine().fills().size() == fillsBefore); // rejections executed nothing
    CHECK(f.svc.pendingStopOrders().size() == 2);
}

static void testTriggerTimeRejection()
{
    std::cout << "[S12] Buy stop that can no longer be paid for at trigger time is cancelled\n";
    Fixture f(10000.0);
    auto r = f.svc.placeStopOrder(OrderType::Stop, OrderSide::Buy, "NOVA", 60, 155.00); // reserves 9300
    const OrderId id = r.order.id();

    moveTo(f, 170.00); // 60 shares now cost > 10000
    auto upd = f.svc.onMarketUpdate();
    CHECK(upd.triggers.size() == 1);
    if (upd.triggers.size() == 1)
    {
        CHECK(upd.triggers[0].rejected);
        CHECK(upd.triggers[0].status == OrderStatus::Cancelled);
        CHECK(!upd.triggers[0].note.empty());
    }
    CHECK(f.svc.getOrder(id)->status() == OrderStatus::Cancelled);
    CHECK(f.pf.holdingQuantity("NOVA") == 0);
    CHECK(near(f.pf.cash(), 10000.0));
    CHECK(f.svc.pendingStopOrders().empty());
    CHECK(near(f.svc.reservedCash(), 0.0));
    CHECK(upd.userFills.empty());
}

static void testBroker()
{
    std::cout << "[S13] Broker facade\n";
    Fixture f;
    broker::Broker b(f.svc);

    auto m = b.placeMarketOrder(OrderSide::Buy, "NOVA", 100);
    CHECK(m.order.status() == OrderStatus::Filled);

    auto sl = b.placeStopLossOrder("NOVA", 50, 145.0);
    CHECK(sl.order.status() == OrderStatus::WaitingForTrigger);
    CHECK(sl.order.side() == OrderSide::Sell && sl.order.type() == OrderType::StopLoss);
    CHECK(b.openOrders().size() == 1);
    CHECK(b.getOrder(sl.order.id()) && b.getOrder(sl.order.id())->isPending());
    CHECK(b.cancelOrder(sl.order.id()));
    CHECK(b.openOrders().empty());

    auto tp = b.placeTakeProfitOrder("NOVA", 50, 160.0);
    auto stl = b.placeStopLimitOrder(OrderSide::Sell, "NOVA", 50, 145.0, 144.5);
    auto bs = b.placeStopOrder(OrderSide::Buy, "NOVA", 10, 155.0);
    CHECK(tp.order.isPending() && stl.order.isPending() && bs.order.isPending());
    CHECK(b.openOrders().size() == 3);
    CHECK(near(b.reservedCash(), 155.0 * 10));
    CHECK(b.reservedShares("NOVA") == 100);

    auto lim = b.placeLimitOrder(OrderSide::Buy, "NOVA", 10, 100.0);
    CHECK(lim.order.status() == OrderStatus::New);

    auto depth = b.marketDepth("NOVA");
    CHECK(depth.has_value());
    if (depth)
    {
        CHECK(depth->bids.size() >= 5 && depth->asks.size() >= 5);
        CHECK(depth->bestBid && depth->bestAsk);
        CHECK(depth->spread() && *depth->spread() > 0.0);
        CHECK(depth->bids.size() == 10 || depth->bids.size() == depth->totalBidLevels);
    }
    CHECK(!b.marketDepth("NOPE").has_value());

    f.market.setPrice("NOVA", 160.50); // take-profit and buy stop fire; stop-limit does not
    auto upd = b.onMarketUpdate();
    CHECK(upd.triggers.size() == 2);
    CHECK(b.getOrder(tp.order.id())->status() == OrderStatus::Filled);
    CHECK(b.getOrder(bs.order.id())->status() == OrderStatus::Filled);
    CHECK(b.getOrder(stl.order.id())->status() == OrderStatus::WaitingForTrigger);
    CHECK(f.pf.holdingQuantity("NOVA") == 60); // 100 - 50 (take-profit) + 10 (buy stop)

    CHECK_THROWS(b.placeStopLossOrder("NOVA", 1000, 100.0), OrderRejectedError);
}

static void testMarketLimitUnchanged()
{
    std::cout << "[S14] Existing market/limit behaviour unchanged\n";
    Fixture f;
    auto m = f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 100);
    CHECK(m.order.status() == OrderStatus::Filled);
    CHECK(m.fills.size() == 1);
    CHECK(f.pf.holdingQuantity("NOVA") == 100);
    CHECK(f.market.executedVolume("NOVA") == 100);

    auto l = f.svc.placeLimitOrder(OrderSide::Buy, "NOVA", 10, 100.00);
    CHECK(l.order.status() == OrderStatus::New && l.fills.empty());
    CHECK(near(f.svc.reservedCash(), 1000.0));
    CHECK(f.svc.openUserOrders().size() == 1);
    CHECK(f.svc.cancelOrder(l.order.id()));
    CHECK(near(f.svc.reservedCash(), 0.0));

    auto cross = f.svc.placeLimitOrder(OrderSide::Buy, "NOVA", 10, 200.00);
    CHECK(cross.order.status() == OrderStatus::Filled);
    CHECK(!cross.fills.empty() && cross.fills[0].price < 151.0);

    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Sell, "NOVA", 5000), OrderRejectedError);
    CHECK_THROWS(f.svc.placeMarketOrder(OrderSide::Buy, "NOPE", 1), OrderRejectedError);

    // Order ids stay unique and increasing across market, limit and stop orders.
    auto s = f.svc.placeStopOrder(OrderType::StopLoss, OrderSide::Sell, "NOVA", 10, 140.0);
    auto m2 = f.svc.placeMarketOrder(OrderSide::Buy, "NOVA", 1);
    CHECK(m2.order.id() > s.order.id());
}

int main()
{
    testOrderModel();
    testEngineOnlyTakesExecutable();
    testPendingBeforeTrigger();
    testStopLossTriggers();
    testBuyStopAndMatching();
    testStopLimitRests();
    testStopLimitMarketable();
    testTakeProfit();
    testCancelPending();
    testPartialFillAfterTrigger();
    testPlacementRejections();
    testTriggerTimeRejection();
    testBroker();
    testMarketLimitUnchanged();

    std::cout << "\n"
              << g_checks << " checks, " << g_failures << " failed\n";
    std::cout << (g_failures == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return g_failures == 0 ? 0 : 1;
}