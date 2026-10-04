#include "Broker.h"

namespace broker
{

    Broker::Broker(execution::ExecutionService &service) : service_(service) {}

    execution::OrderReceipt Broker::placeMarketOrder(execution::OrderSide side, const std::string &symbol,
                                                     int quantity)
    {
        return service_.placeMarketOrder(side, symbol, quantity);
    }

    execution::OrderReceipt Broker::placeLimitOrder(execution::OrderSide side, const std::string &symbol,
                                                    int quantity, double limitPrice)
    {
        return service_.placeLimitOrder(side, symbol, quantity, limitPrice);
    }

    execution::OrderReceipt Broker::placeStopOrder(execution::OrderSide side, const std::string &symbol,
                                                   int quantity, double stopPrice)
    {
        return service_.placeStopOrder(execution::OrderType::Stop, side, symbol, quantity, stopPrice);
    }

    execution::OrderReceipt Broker::placeStopLossOrder(const std::string &symbol, int quantity,
                                                       double stopPrice)
    {
        return service_.placeStopOrder(execution::OrderType::StopLoss, execution::OrderSide::Sell, symbol,
                                       quantity, stopPrice);
    }

    execution::OrderReceipt Broker::placeTakeProfitOrder(const std::string &symbol, int quantity,
                                                         double triggerPrice)
    {
        return service_.placeStopOrder(execution::OrderType::TakeProfit, execution::OrderSide::Sell, symbol,
                                       quantity, triggerPrice);
    }

    execution::OrderReceipt Broker::placeStopLimitOrder(execution::OrderSide side, const std::string &symbol,
                                                        int quantity, double stopPrice, double limitPrice)
    {
        return service_.placeStopOrder(execution::OrderType::StopLimit, side, symbol, quantity, stopPrice,
                                       limitPrice);
    }

    bool Broker::cancelOrder(execution::OrderId id)
    {
        return service_.cancelOrder(id);
    }

    std::optional<execution::Order> Broker::getOrder(execution::OrderId id) const
    {
        return service_.getOrder(id);
    }

    std::vector<execution::Order> Broker::openOrders() const
    {
        return service_.openUserOrders();
    }

    double Broker::reservedCash() const
    {
        return service_.reservedCash();
    }

    int Broker::reservedShares(const std::string &symbol) const
    {
        return service_.reservedShares(symbol);
    }

    risk::AccountRisk Broker::accountRisk() const
    {
        return service_.riskManager().assess();
    }

    double Broker::buyingPower() const
    {
        return service_.buyingPower();
    }

    const risk::RiskConfig &Broker::riskConfig() const
    {
        return service_.riskManager().config();
    }

    void Broker::setRiskConfig(const risk::RiskConfig &config)
    {
        service_.riskManager().setConfig(config);
    }

    execution::LiquidationReport Broker::enforceMargin()
    {
        return service_.enforceMargin();
    }

    std::optional<MarketDepth> Broker::marketDepth(const std::string &symbol, std::size_t depth) const
    {
        const execution::OrderBook *book = service_.engine().book(symbol);
        if (book == nullptr)
        {
            return std::nullopt;
        }

        MarketDepth d;
        d.symbol = symbol;
        d.bids = book->bidLevels(depth);
        d.asks = book->askLevels(depth);
        d.totalBidLevels = book->bidLevels().size();
        d.totalAskLevels = book->askLevels().size();
        d.bestBid = book->bestBid();
        d.bestAsk = book->bestAsk();
        return d;
    }

    execution::MarketUpdateResult Broker::onMarketUpdate()
    {
        return service_.onMarketUpdate();
    }

} // namespace broker