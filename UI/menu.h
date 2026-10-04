#ifndef MARKETSIM_UI_MENU_H
#define MARKETSIM_UI_MENU_H

#include <string>
#include "../Market/Market.h"
#include "../Market/MarketIndex.h"
#include "../Market/MarketStatistics.h"
#include "../Portfolio/Portfolio.h"
#include "../Simulation/Simulation.h"
#include "../Broker/Broker.h"

// UI module: console input/output only. Contains no business rules -
// every real decision is delegated: trading and risk go through the Broker,
// and read-only market/portfolio data comes from Market, Portfolio and Simulation.
namespace ui
{

    class Menu
    {
    public:
        Menu(market::Market &market, portfolio::Portfolio &portfolio, market::MarketIndex &index,
             broker::Broker &broker);

        // Runs the main menu loop until the user chooses to exit.
        void run();

    private:
        market::Market &market_;
        portfolio::Portfolio &portfolio_;
        market::MarketIndex &index_;
        broker::Broker &broker_;

        void showMainMenu() const;

        void handleViewMarket() const;
        void handleViewPortfolio() const;
        void handleTransactionHistory() const;
        void handlePortfolioAnalysis() const;
        void handleSimulation() const;

        void runBullScenario() const;
        void runBearScenario() const;
        void runCustomScenario() const;
        void printSimulationResult(const simulation::SimulationResult &result) const;

        // Dynamic market data screens.
        void handleMarketTick();
        void handleMarketIndex() const;
        void handleMarketStatistics() const;
        void handlePriceHistory() const;

        // Order entry and order book screens (all go through the Broker).
        void handlePlaceOrder();
        void handleOpenOrders() const;
        void handleCancelOrder();
        void handleViewOrderBook() const;
        void printOrderReceipt(const execution::OrderReceipt &receipt) const;
        void printMarketUpdate(const execution::MarketUpdateResult &update) const;

        // Risk & margin screens (display only; all numbers come from the Broker).
        void handleRiskAndMargin();
        void printAccountRisk() const;
        void printLiquidationReport(const execution::LiquidationReport &report) const;

        static int readIntInput(const std::string &prompt);
        static double readDoubleInput(const std::string &prompt);
        static std::string readSymbolInput(const std::string &prompt);
        static execution::OrderSide readSideInput();
        static execution::OrderId readOrderIdInput(const std::string &prompt);
    };

} // namespace ui

#endif // MARKETSIM_UI_MENU_H