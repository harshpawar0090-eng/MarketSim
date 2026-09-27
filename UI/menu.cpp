#include "Menu.h"
#include "../Trading/Trading.h"

#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
#include <algorithm>
#include <cctype>
#include <ctime>

namespace ui
{

    Menu::Menu(market::Market &market, portfolio::Portfolio &portfolio)
        : market_(market), portfolio_(portfolio) {}

    int Menu::readIntInput(const std::string &prompt)
    {
        int value;
        while (true)
        {
            std::cout << prompt;
            if (std::cin >> value)
            {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return value;
            }
            std::cout << "Invalid input. Please enter a whole number.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    double Menu::readDoubleInput(const std::string &prompt)
    {
        double value;
        while (true)
        {
            std::cout << prompt;
            if (std::cin >> value)
            {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return value;
            }
            std::cout << "Invalid input. Please enter a number.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    std::string Menu::readSymbolInput(const std::string &prompt)
    {
        std::string value;
        std::cout << prompt;
        std::cin >> value;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::toupper(c)); });
        return value;
    }

    void Menu::showMainMenu() const
    {
        std::cout << "\n===================== MarketSim =====================\n";
        std::cout << "1. View Market\n";
        std::cout << "2. View Portfolio\n";
        std::cout << "3. Buy Stock\n";
        std::cout << "4. Sell Stock\n";
        std::cout << "5. Transaction History\n";
        std::cout << "6. Portfolio Analysis\n";
        std::cout << "7. Run What-If Simulation\n";
        std::cout << "8. Exit\n";
        std::cout << "======================================================\n";
    }

    void Menu::run()
    {
        bool running = true;
        while (running)
        {
            showMainMenu();
            int choice = readIntInput("Select an option (1-8): ");

            switch (choice)
            {
            case 1:
                handleViewMarket();
                break;
            case 2:
                handleViewPortfolio();
                break;
            case 3:
                handleBuyStock();
                break;
            case 4:
                handleSellStock();
                break;
            case 5:
                handleTransactionHistory();
                break;
            case 6:
                handlePortfolioAnalysis();
                break;
            case 7:
                handleSimulation();
                break;
            case 8:
                std::cout << "Exiting MarketSim. Goodbye.\n";
                running = false;
                break;
            default:
                std::cout << "Invalid option. Please choose 1-8.\n";
            }
        }
    }

    void Menu::handleViewMarket() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Market ---\n";
        std::cout << std::left << std::setw(8) << "Symbol"
                  << std::setw(22) << "Name"
                  << std::setw(14) << "Sector"
                  << std::right << std::setw(10) << "Price" << "\n";

        for (const auto &[symbol, stock] : market_.allStocks())
        {
            std::cout << std::left << std::setw(8) << stock.symbol()
                      << std::setw(22) << stock.name()
                      << std::setw(14) << market::sectorToString(stock.sector())
                      << std::right << std::setw(10) << stock.price() << "\n";
        }
    }

    void Menu::handleViewPortfolio() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Portfolio ---\n";
        std::cout << "Cash Balance: $" << portfolio_.cash() << "\n\n";

        auto lines = portfolio::PortfolioAnalyzer::holdingsReport(portfolio_, market_);
        if (lines.empty())
        {
            std::cout << "No holdings.\n";
        }
        else
        {
            std::cout << std::left << std::setw(8) << "Symbol"
                      << std::right << std::setw(10) << "Qty"
                      << std::setw(12) << "Avg Cost"
                      << std::setw(12) << "Price"
                      << std::setw(14) << "Mkt Value"
                      << std::setw(16) << "Unrealized P&L" << "\n";

            for (const auto &line : lines)
            {
                std::cout << std::left << std::setw(8) << line.symbol
                          << std::right << std::setw(10) << line.quantity
                          << std::setw(12) << line.avgCost
                          << std::setw(12) << line.currentPrice
                          << std::setw(14) << line.marketValue
                          << std::setw(16) << line.unrealizedPnL << "\n";
            }
        }

        double marketValue = portfolio::PortfolioAnalyzer::totalMarketValue(portfolio_, market_);
        double unrealized = portfolio::PortfolioAnalyzer::totalUnrealizedPnL(portfolio_, market_);
        double realized = portfolio::PortfolioAnalyzer::totalRealizedPnL(portfolio_);
        double totalValue = portfolio::PortfolioAnalyzer::totalPortfolioValue(portfolio_, market_);

        std::cout << "\nHoldings Market Value: $" << marketValue << "\n";
        std::cout << "Total Unrealized P&L : $" << unrealized << "\n";
        std::cout << "Total Realized P&L   : $" << realized << "\n";
        std::cout << "Total Portfolio Value: $" << totalValue << " (cash + holdings)\n";
    }

    void Menu::handleBuyStock()
    {
        std::string symbol = readSymbolInput("Enter stock symbol to buy: ");
        int qty = readIntInput("Enter quantity to buy: ");

        try
        {
            trading::BuyOrder order(symbol, qty);
            portfolio::Transaction t = order.execute(portfolio_, market_);
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Bought " << t.quantity << " share(s) of " << t.symbol
                      << " at $" << t.price << " each.\n";
        }
        catch (const std::exception &e)
        {
            std::cout << "Buy failed: " << e.what() << "\n";
        }
    }

    void Menu::handleSellStock()
    {
        std::string symbol = readSymbolInput("Enter stock symbol to sell: ");
        int qty = readIntInput("Enter quantity to sell: ");

        try
        {
            trading::SellOrder order(symbol, qty);
            portfolio::Transaction t = order.execute(portfolio_, market_);
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Sold " << t.quantity << " share(s) of " << t.symbol
                      << " at $" << t.price << " each. Realized P&L: $" << t.realizedPnL << "\n";
        }
        catch (const std::exception &e)
        {
            std::cout << "Sell failed: " << e.what() << "\n";
        }
    }

    void Menu::handleTransactionHistory() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Transaction History ---\n";

        const auto &history = portfolio_.history();
        if (history.empty())
        {
            std::cout << "No transactions yet.\n";
            return;
        }

        for (const auto &t : history)
        {
            std::tm localTime{};
            std::time_t ts = t.timestamp;
#if defined(_WIN32)
            localtime_s(&localTime, &ts);
#else
            localtime_r(&ts, &localTime);
#endif
            std::cout << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << "  "
                      << portfolio::transactionSideToString(t.side) << "  "
                      << t.symbol << "  qty=" << t.quantity
                      << "  price=$" << t.price;
            if (t.side == portfolio::TransactionSide::Sell)
            {
                std::cout << "  realizedPnL=$" << t.realizedPnL;
            }
            std::cout << "\n";
        }
    }

    void Menu::handlePortfolioAnalysis() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Portfolio Analysis ---\n";

        double marketValue = portfolio::PortfolioAnalyzer::totalMarketValue(portfolio_, market_);
        double totalValue = portfolio::PortfolioAnalyzer::totalPortfolioValue(portfolio_, market_);
        std::cout << "Holdings Market Value: $" << marketValue << "\n";
        std::cout << "Total Portfolio Value: $" << totalValue << "\n\n";

        auto allocation = portfolio::PortfolioAnalyzer::sectorAllocation(portfolio_, market_);
        if (allocation.empty())
        {
            std::cout << "No holdings to analyze.\n";
            return;
        }

        std::cout << "Sector Allocation (of holdings market value):\n";
        for (const auto &[sector, percent] : allocation)
        {
            std::cout << "  " << std::left << std::setw(14) << market::sectorToString(sector)
                      << std::right << std::setw(6) << percent << "%\n";
        }
    }

    void Menu::printSimulationResult(const simulation::SimulationResult &result) const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- " << result.scenarioName << " Result ---\n";

        if (result.lines.empty())
        {
            std::cout << "No holdings to simulate.\n";
            return;
        }

        std::cout << std::left << std::setw(8) << "Symbol"
                  << std::right << std::setw(8) << "Qty"
                  << std::setw(14) << "Base Price"
                  << std::setw(14) << "Sim Price"
                  << std::setw(14) << "Base Value"
                  << std::setw(14) << "Sim Value" << "\n";

        for (const auto &line : result.lines)
        {
            std::cout << std::left << std::setw(8) << line.symbol
                      << std::right << std::setw(8) << line.quantity
                      << std::setw(14) << line.baselinePrice
                      << std::setw(14) << line.hypotheticalPrice
                      << std::setw(14) << line.baselineValue
                      << std::setw(14) << line.hypotheticalValue << "\n";
        }

        std::cout << "\nBaseline Holdings Value    : $" << result.baselineValue << "\n";
        std::cout << "Hypothetical Holdings Value: $" << result.hypotheticalValue << "\n";
        std::cout << "Change                     : $" << result.delta()
                  << " (" << result.deltaPercent() << "%)\n";
        std::cout << "Note: the real Market and Portfolio were not modified.\n";
    }

    void Menu::runBullScenario() const
    {
        double growth = readDoubleInput("Enter bull growth percent (e.g. 10 for +10%): ");
        simulation::BullScenario scenario(growth);
        auto result = simulation::SimulationEngine::run(portfolio_, market_, scenario);
        printSimulationResult(result);
    }

    void Menu::runBearScenario() const
    {
        double decline = readDoubleInput("Enter bear decline percent (e.g. 15 for -15%): ");
        simulation::BearScenario scenario(decline);
        auto result = simulation::SimulationEngine::run(portfolio_, market_, scenario);
        printSimulationResult(result);
    }

    void Menu::runCustomScenario() const
    {
        int count = readIntInput("How many symbols do you want to customize? ");
        if (count < 0)
            count = 0;

        std::unordered_map<std::string, double> overrides;
        for (int i = 0; i < count; ++i)
        {
            std::string symbol = readSymbolInput("  Symbol: ");
            double percentChange = readDoubleInput("  Percent change (e.g. 5 or -5): ");
            overrides[symbol] = percentChange;
        }

        simulation::CustomScenario scenario(std::move(overrides));
        auto result = simulation::SimulationEngine::run(portfolio_, market_, scenario);
        printSimulationResult(result);
    }

    void Menu::handleSimulation() const
    {
        std::cout << "\n--- What-If Simulation ---\n";
        std::cout << "1. Bull Scenario\n";
        std::cout << "2. Bear Scenario\n";
        std::cout << "3. Custom Scenario\n";
        std::cout << "4. Back to Main Menu\n";

        int choice = readIntInput("Select scenario type (1-4): ");
        switch (choice)
        {
        case 1:
            runBullScenario();
            break;
        case 2:
            runBearScenario();
            break;
        case 3:
            runCustomScenario();
            break;
        case 4:
            return;
        default:
            std::cout << "Invalid option.\n";
        }
    }

} // namespace ui