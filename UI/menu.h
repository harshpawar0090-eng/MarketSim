#ifndef MARKETSIM_UI_MENU_H
#define MARKETSIM_UI_MENU_H

#include <string>
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"
#include "../Simulation/Simulation.h"

// UI module: console input/output only. Contains no business rules -
// every real decision (validation, pricing, P&L math) is delegated to
// Market, Trading, Portfolio, or Simulation.
namespace ui
{

    class Menu
    {
    public:
        Menu(market::Market &market, portfolio::Portfolio &portfolio);

        // Runs the main menu loop until the user chooses to exit.
        void run();

    private:
        market::Market &market_;
        portfolio::Portfolio &portfolio_;

        void showMainMenu() const;

        void handleViewMarket() const;
        void handleViewPortfolio() const;
        void handleBuyStock();
        void handleSellStock();
        void handleTransactionHistory() const;
        void handlePortfolioAnalysis() const;
        void handleSimulation() const;

        void runBullScenario() const;
        void runBearScenario() const;
        void runCustomScenario() const;
        void printSimulationResult(const simulation::SimulationResult &result) const;

        static int readIntInput(const std::string &prompt);
        static double readDoubleInput(const std::string &prompt);
        static std::string readSymbolInput(const std::string &prompt);
    };

} // namespace ui

#endif // MARKETSIM_UI_MENU_H