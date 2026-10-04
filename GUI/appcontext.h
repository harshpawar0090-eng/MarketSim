#ifndef MARKETSIM_GUI_APPCONTEXT_H
#define MARKETSIM_GUI_APPCONTEXT_H

#include <QObject>
#include <QString>
#include "../Analytics/PortfolioIntelligence.h"
#include "../Broker/Broker.h"
#include "../Execution/ExecutionService.h"
#include "../Market/Market.h"
#include "../Market/MarketIndex.h"
#include "../Portfolio/Portfolio.h"
#include "../Risk/RiskManager.h"

namespace gui
{

    // Margin account used by the GUI: shorting on, 2x leverage, 25% maintenance,
    // plus order/position/exposure limits (same values as the Phase 3 design).
    inline risk::RiskConfig defaultGuiRiskConfig()
    {
        risk::RiskConfig c = risk::RiskConfig::marginAccount();
        c.maxOrderQuantity = 5000;
        c.maxPositionQuantity = 20000;
        c.maxGrossExposure = 1000000.00;
        return c;
    }

    struct AppConfig
    {
        double startingCash = 500000.00;
        risk::RiskConfig riskConfig = defaultGuiRiskConfig();
    };

    // The ONE owner of the live backend objects for the GUI. It builds exactly
    // one Market, MarketIndex, Portfolio, ExecutionService, Broker and
    // PortfolioIntelligence - the same classes the console app uses - and wires
    // them in dependency order. Pages get read-only access to market/account
    // state and go through the Broker for anything trading-related.
    //
    // advanceTick() performs the existing market-update flow:
    //   Market::tick() -> MarketIndex::update() -> Broker::onMarketUpdate()
    // (liquidity re-quote, stop triggers, margin check) and then notifies the
    // GUI through stateChanged(). It contains no financial logic of its own.
    class AppContext : public QObject
    {
        Q_OBJECT

    public:
        explicit AppContext(const AppConfig &config = AppConfig(), QObject *parent = nullptr);

        // Read-only views of the single source of truth.
        const market::Market &getMarket() const { return market_; }
        const market::MarketIndex &getIndex() const { return index_; }
        const portfolio::Portfolio &getPortfolio() const { return portfolio_; }
        const analytics::PortfolioIntelligence &getIntelligence() const { return intelligence_; }
        const broker::Broker &getBroker() const { return broker_; }

        // Trading entry point (used by the trading pages in Phase 5B.2).
        broker::Broker &getBroker() { return broker_; }

        const AppConfig &config() const { return config_; }
        int tickCount() const { return tickCount_; }
        QString lastTickSummary() const { return lastSummary_; }

    public slots:
        void advanceTick();

        // Call after anything that changes the account outside a market tick
        // (order placed/cancelled, fill, ...). Records a portfolio snapshot and
        // emits stateChanged() so EVERY page refreshes from the same source of
        // truth. Contains no financial logic.
        void notifyAccountChanged();

    signals:
        // Emitted after any change to market/account state; pages refresh from it.
        void stateChanged();
        // Human-readable one-line result of the last tick (for the status bar).
        void tickAdvanced(const QString &summary);

    private:
        // Member order is initialization order: later members reference earlier ones.
        AppConfig config_;
        market::Market market_;
        market::MarketIndex index_;
        portfolio::Portfolio portfolio_;
        execution::ExecutionService execution_;
        broker::Broker broker_;
        analytics::PortfolioIntelligence intelligence_;
        int tickCount_ = 0;
        QString lastSummary_;
    };

} // namespace gui

#endif // MARKETSIM_GUI_APPCONTEXT_H