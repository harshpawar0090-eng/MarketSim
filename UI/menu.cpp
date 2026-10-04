
#include "Menu.h"

#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <unordered_map>

namespace ui
{

    Menu::Menu(market::Market &market, portfolio::Portfolio &portfolio, market::MarketIndex &index,
               broker::Broker &broker)
        : market_(market), portfolio_(portfolio), index_(index), broker_(broker) {}

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

    execution::OrderSide Menu::readSideInput()
    {
        while (true)
        {
            int choice = readIntInput("Side (1 = Buy, 2 = Sell): ");
            if (choice == 1)
            {
                return execution::OrderSide::Buy;
            }
            if (choice == 2)
            {
                return execution::OrderSide::Sell;
            }
            std::cout << "Please enter 1 for Buy or 2 for Sell.\n";
        }
    }

    execution::OrderId Menu::readOrderIdInput(const std::string &prompt)
    {
        long long value;
        while (true)
        {
            std::cout << prompt;
            if (std::cin >> value)
            {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (value > 0)
                {
                    return static_cast<execution::OrderId>(value);
                }
                std::cout << "Order ID must be a positive whole number.\n";
            }
            else
            {
                std::cout << "Invalid input. Please enter a whole number.\n";
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }
    }

    void Menu::showMainMenu() const
    {
        std::cout << "\n===================== MarketSim =====================\n";
        std::cout << "1.  View Market\n";
        std::cout << "2.  View Portfolio\n";
        std::cout << "3.  Place Order (Market/Limit/Stop/Stop-Loss/Take-Profit/Stop-Limit)\n";
        std::cout << "4.  View Open / Pending Orders\n";
        std::cout << "5.  Cancel Order\n";
        std::cout << "6.  View Order Book\n";
        std::cout << "7.  Transaction History\n";
        std::cout << "8.  Portfolio Analysis\n";
        std::cout << "9.  Run What-If Simulation\n";
        std::cout << "10. Advance Market Tick\n";
        std::cout << "11. Market Index\n";
        std::cout << "12. Market Statistics\n";
        std::cout << "13. Price History (OHLC)\n";
        std::cout << "14. Risk & Margin\n";
        std::cout << "15. Exit\n";
        std::cout << "======================================================\n";
    }

    void Menu::run()
    {
        bool running = true;
        while (running)
        {
            showMainMenu();
            int choice = readIntInput("Select an option (1-15): ");

            switch (choice)
            {
            case 1:
                handleViewMarket();
                break;
            case 2:
                handleViewPortfolio();
                break;
            case 3:
                handlePlaceOrder();
                break;
            case 4:
                handleOpenOrders();
                break;
            case 5:
                handleCancelOrder();
                break;
            case 6:
                handleViewOrderBook();
                break;
            case 7:
                handleTransactionHistory();
                break;
            case 8:
                handlePortfolioAnalysis();
                break;
            case 9:
                handleSimulation();
                break;
            case 10:
                handleMarketTick();
                break;
            case 11:
                handleMarketIndex();
                break;
            case 12:
                handleMarketStatistics();
                break;
            case 13:
                handlePriceHistory();
                break;
            case 14:
                handleRiskAndMargin();
                break;
            case 15:
                std::cout << "Exiting MarketSim. Goodbye.\n";
                running = false;
                break;
            default:
                std::cout << "Invalid option. Please choose 1-15.\n";
            }
        }
    }

    void Menu::handleViewMarket() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Market ---\n";
        std::cout << std::left << std::setw(8) << "Symbol"
                  << std::setw(22) << "Name"
                  << std::setw(12) << "Sector"
                  << std::right << std::setw(10) << "Price"
                  << std::setw(10) << "Change"
                  << std::setw(10) << "%Chg" << "\n";

        for (const auto &[symbol, stock] : market_.allStocks())
        {
            std::cout << std::left << std::setw(8) << stock.symbol()
                      << std::setw(22) << stock.name()
                      << std::setw(12) << market::sectorToString(stock.sector())
                      << std::right << std::setw(10) << stock.price()
                      << std::setw(10) << stock.change()
                      << std::setw(9) << stock.percentChange() << "%" << "\n";
        }
    }

    void Menu::handleViewPortfolio() const
    {
        const risk::AccountRisk account = broker_.accountRisk();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Portfolio ---\n";
        std::cout << "Cash Balance: $" << portfolio_.cash();
        if (portfolio_.cash() < 0.0)
        {
            std::cout << "  (negative cash = margin loan)";
        }
        std::cout << "\n";

        // Cash backing resting limit buys and waiting buy stops is reserved.
        double reserved = broker_.reservedCash();
        if (reserved > 0.0)
        {
            std::cout << "Reserved for open orders: $" << reserved << "\n";
        }
        std::cout << "\n";

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

            bool anyShort = false;
            for (const auto &line : lines)
            {
                if (line.quantity < 0)
                {
                    anyShort = true;
                }
                std::cout << std::left << std::setw(8) << line.symbol
                          << std::right << std::setw(10) << line.quantity
                          << std::setw(12) << line.avgCost
                          << std::setw(12) << line.currentPrice
                          << std::setw(14) << line.marketValue
                          << std::setw(16) << line.unrealizedPnL << "\n";
            }
            if (anyShort)
            {
                std::cout << "(Negative quantity = short position; Avg Cost is the average entry price.)\n";
            }
        }

        double marketValue = portfolio::PortfolioAnalyzer::totalMarketValue(portfolio_, market_);
        double unrealized = portfolio::PortfolioAnalyzer::totalUnrealizedPnL(portfolio_, market_);
        double realized = portfolio::PortfolioAnalyzer::totalRealizedPnL(portfolio_);
        double totalValue = portfolio::PortfolioAnalyzer::totalPortfolioValue(portfolio_, market_);

        std::cout << "\nHoldings Market Value (net): $" << marketValue << "\n";
        std::cout << "Total Unrealized P&L       : $" << unrealized << "\n";
        std::cout << "Total Realized P&L         : $" << realized << "\n";
        std::cout << "Total Portfolio Value      : $" << totalValue << " (equity)\n";
        std::cout << "Buying Power               : $" << broker_.buyingPower() << "\n";
        if (account.marginCall())
        {
            std::cout << "\n*** MARGIN CALL: equity is below the maintenance margin requirement. ***\n";
            std::cout << "*** Only orders that reduce positions are accepted.                   ***\n";
        }
    }

    // ------------------------- order entry -------------------------

    void Menu::printOrderReceipt(const execution::OrderReceipt &receipt) const
    {
        const execution::Order &o = receipt.order;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Order #" << o.id() << ": "
                  << execution::orderSideToString(o.side()) << " "
                  << execution::orderTypeToString(o.type()) << " "
                  << o.symbol() << " ---\n";
        std::cout << "Status    : " << execution::orderStatusToString(o.status()) << "\n";
        std::cout << "Quantity  : " << o.quantity() << "\n";
        std::cout << "Filled    : " << o.filledQuantity() << "\n";
        std::cout << "Remaining : " << o.remainingQuantity() << "\n";
        if (o.isStop())
        {
            std::cout << "Trigger   : $" << o.stopPrice() << " (" << o.triggerDescription() << ")\n";
        }
        if (o.executionType() == execution::OrderType::Limit)
        {
            std::cout << "Limit     : $" << o.limitPrice() << "\n";
        }

        if (!receipt.fills.empty())
        {
            std::cout << "Avg Price : $" << receipt.averagePrice() << "\n";
            std::cout << "Total     : $" << receipt.totalValue() << "\n\n";

            std::cout << std::left << std::setw(10) << "Fill #"
                      << std::right << std::setw(12) << "Price"
                      << std::setw(10) << "Qty"
                      << std::setw(14) << "Value" << "\n";
            for (const auto &f : receipt.fills)
            {
                std::cout << std::left << std::setw(10) << f.fillId
                          << std::right << std::setw(12) << f.price
                          << std::setw(10) << f.quantity
                          << std::setw(14) << f.value() << "\n";
            }
        }
        else
        {
            std::cout << "Fills     : none\n";
        }

        std::cout << "\n";
        switch (o.status())
        {
        case execution::OrderStatus::WaitingForTrigger:
            std::cout << "The order is waiting for its trigger and is NOT in the order book. It will activate when "
                      << o.triggerDescription() << ". Use \"Cancel Order\" with ID " << o.id()
                      << " to cancel.\n";
            break;
        case execution::OrderStatus::New:
        case execution::OrderStatus::PartiallyFilled:
            std::cout << o.remainingQuantity() << " share(s) now rest in the order book at $"
                      << o.limitPrice() << ". Use \"Cancel Order\" with ID " << o.id()
                      << " to cancel.\n";
            break;
        case execution::OrderStatus::Cancelled:
            if (o.filledQuantity() == 0)
            {
                std::cout << "No liquidity was available - order cancelled with no fills.\n";
            }
            else
            {
                std::cout << "Only part of the order could be filled. The unfilled "
                          << o.remainingQuantity()
                          << " share(s) were cancelled (market orders never rest).\n";
            }
            break;
        case execution::OrderStatus::Filled:
            std::cout << "Order fully filled.\n";
            break;
        }
    }

    void Menu::handlePlaceOrder()
    {
        std::cout << "\n--- Place Order ---\n";
        std::cout << "1. Market\n";
        std::cout << "2. Limit\n";
        std::cout << "3. Stop\n";
        std::cout << "4. Stop-Loss (sell)\n";
        std::cout << "5. Take-Profit (sell)\n";
        std::cout << "6. Stop-Limit\n";
        std::cout << "7. Back to Main Menu\n";

        int type = readIntInput("Select order type (1-7): ");
        if (type == 7)
        {
            return;
        }
        if (type < 1 || type > 6)
        {
            std::cout << "Invalid order type.\n";
            return;
        }

        // Stop-loss and take-profit protect shares already held, so they are SELL orders.
        execution::OrderSide side = execution::OrderSide::Sell;
        if (type == 4 || type == 5)
        {
            std::cout << "(Stop-loss and take-profit orders are SELL orders for shares you hold.)\n";
        }
        else
        {
            side = readSideInput();
        }

        std::string symbol = readSymbolInput("Enter stock symbol: ");
        int qty = readIntInput("Enter quantity: ");

        if (market_.exists(symbol))
        {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Current market price: $" << market_.getQuote(symbol).price() << "\n";
        }

        double limitPrice = 0.0;
        double stopPrice = 0.0;
        switch (type)
        {
        case 2:
            limitPrice = readDoubleInput("Enter limit price: $");
            break;
        case 3:
            stopPrice = readDoubleInput("Enter stop (trigger) price: $");
            break;
        case 4:
            stopPrice = readDoubleInput("Enter stop-loss trigger price: $");
            break;
        case 5:
            stopPrice = readDoubleInput("Enter take-profit trigger price: $");
            break;
        case 6:
            stopPrice = readDoubleInput("Enter stop (trigger) price: $");
            limitPrice = readDoubleInput("Enter limit price (used once triggered): $");
            break;
        default:
            break;
        }

        try
        {
            execution::OrderReceipt receipt = [&]() -> execution::OrderReceipt
            {
                switch (type)
                {
                case 1:
                    return broker_.placeMarketOrder(side, symbol, qty);
                case 2:
                    return broker_.placeLimitOrder(side, symbol, qty, limitPrice);
                case 3:
                    return broker_.placeStopOrder(side, symbol, qty, stopPrice);
                case 4:
                    return broker_.placeStopLossOrder(symbol, qty, stopPrice);
                case 5:
                    return broker_.placeTakeProfitOrder(symbol, qty, stopPrice);
                default:
                    return broker_.placeStopLimitOrder(side, symbol, qty, stopPrice, limitPrice);
                }
            }();
            printOrderReceipt(receipt);
        }
        catch (const execution::OrderRejectedError &e)
        {
            std::cout << "Order rejected: " << e.what() << "\n";
        }
        catch (const std::exception &e)
        {
            std::cout << "Order failed: " << e.what() << "\n";
        }
    }

    void Menu::handleOpenOrders() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Open / Pending Orders ---\n";

        std::vector<execution::Order> orders = broker_.openOrders();
        if (orders.empty())
        {
            std::cout << "No open orders.\n";
            return;
        }

        std::cout << std::left << std::setw(6) << "ID"
                  << std::setw(8) << "Symbol"
                  << std::setw(6) << "Side"
                  << std::setw(13) << "Type"
                  << std::right << std::setw(7) << "Qty"
                  << std::setw(8) << "Filled"
                  << std::setw(10) << "Remaining"
                  << std::setw(10) << "Trigger"
                  << std::setw(10) << "Limit"
                  << "  " << std::left << "Status" << "\n";

        for (const auto &o : orders)
        {
            std::cout << std::left << std::setw(6) << o.id()
                      << std::setw(8) << o.symbol()
                      << std::setw(6) << execution::orderSideToString(o.side())
                      << std::setw(13) << execution::orderTypeToString(o.type())
                      << std::right << std::setw(7) << o.quantity()
                      << std::setw(8) << o.filledQuantity()
                      << std::setw(10) << o.remainingQuantity();

            if (o.isStop())
            {
                std::cout << std::setw(10) << o.stopPrice();
            }
            else
            {
                std::cout << std::setw(10) << "-";
            }

            if (o.executionType() == execution::OrderType::Limit)
            {
                std::cout << std::setw(10) << o.limitPrice();
            }
            else
            {
                std::cout << std::setw(10) << "MKT";
            }
            std::cout << "  " << std::left << execution::orderStatusToString(o.status()) << "\n";
        }

        std::cout << "\nCash reserved for open buy orders: $" << broker_.reservedCash() << "\n";
    }

    void Menu::handleCancelOrder()
    {
        std::cout << "\n--- Cancel Order ---\n";
        execution::OrderId id = readOrderIdInput("Enter order ID to cancel: ");

        try
        {
            if (broker_.cancelOrder(id))
            {
                std::cout << "Order #" << id << " cancelled.\n";
                if (auto order = broker_.getOrder(id))
                {
                    std::cout << "  " << execution::orderSideToString(order->side()) << " "
                              << execution::orderTypeToString(order->type()) << " "
                              << order->symbol() << ": " << order->filledQuantity()
                              << " of " << order->quantity() << " share(s) had been filled.\n";
                }
            }
            else
            {
                std::cout << "Order #" << id
                          << " could not be cancelled (not found, not yours, or already finished).\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "Cancel failed: " << e.what() << "\n";
        }
    }

    void Menu::handleViewOrderBook() const
    {
        std::string symbol = readSymbolInput("Enter stock symbol: ");
        if (!market_.exists(symbol))
        {
            std::cout << "Unknown symbol: " << symbol << "\n";
            return;
        }

        constexpr std::size_t kDepth = 10; // levels shown per side

        std::optional<broker::MarketDepth> depth = broker_.marketDepth(symbol, kDepth);
        if (!depth)
        {
            std::cout << "No order book exists yet for " << symbol << ".\n";
            return;
        }

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Order Book: " << symbol << " (" << market_.getInstrument(symbol).name() << ") ---\n";
        std::cout << "Market price: $" << market_.getQuote(symbol).price() << "\n\n";

        std::cout << std::right << std::setw(12) << "Price"
                  << std::setw(12) << "Quantity"
                  << std::setw(10) << "Orders" << "\n";

        // Asks: highest shown at the top so the best (lowest) ask sits next to the spread.
        std::cout << "ASKS";
        if (depth->asks.empty())
        {
            std::cout << "  (none)";
        }
        else if (depth->totalAskLevels > depth->asks.size())
        {
            std::cout << "  (+" << (depth->totalAskLevels - depth->asks.size())
                      << " more level(s) not shown)";
        }
        std::cout << "\n";
        for (std::size_t i = depth->asks.size(); i > 0; --i)
        {
            const auto &level = depth->asks[i - 1];
            std::cout << std::right << std::setw(12) << level.price
                      << std::setw(12) << level.quantity
                      << std::setw(10) << level.orderCount << "\n";
        }

        std::cout << "------------------------------------\n";

        // Bids: best (highest) first.
        std::cout << "BIDS";
        if (depth->bids.empty())
        {
            std::cout << "  (none)";
        }
        else if (depth->totalBidLevels > depth->bids.size())
        {
            std::cout << "  (+" << (depth->totalBidLevels - depth->bids.size())
                      << " more level(s) not shown)";
        }
        std::cout << "\n";
        for (const auto &level : depth->bids)
        {
            std::cout << std::right << std::setw(12) << level.price
                      << std::setw(12) << level.quantity
                      << std::setw(10) << level.orderCount << "\n";
        }

        std::cout << "\nBest Bid: ";
        if (depth->bestBid)
            std::cout << "$" << *depth->bestBid;
        else
            std::cout << "n/a";

        std::cout << "\nBest Ask: ";
        if (depth->bestAsk)
            std::cout << "$" << *depth->bestAsk;
        else
            std::cout << "n/a";

        std::cout << "\nSpread  : ";
        if (auto spread = depth->spread())
            std::cout << "$" << *spread;
        else
            std::cout << "n/a";
        std::cout << "\n";
        std::cout << "(Stop orders waiting for a trigger are not part of the book - see Open / Pending Orders.)\n";
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
                      << portfolio::positionEffectToString(t.effect) << "  "
                      << t.symbol << "  qty=" << t.quantity
                      << "  price=$" << t.price;
            if (t.effect == portfolio::PositionEffect::CloseLong ||
                t.effect == portfolio::PositionEffect::CoverShort)
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
        std::cout << "Holdings Market Value (net): $" << marketValue << "\n";
        std::cout << "Total Portfolio Value      : $" << totalValue << "\n\n";

        auto allocation = portfolio::PortfolioAnalyzer::sectorAllocation(portfolio_, market_);
        if (allocation.empty())
        {
            std::cout << "No holdings to analyze.\n";
            return;
        }

        std::cout << "Sector Allocation (of gross position value, longs and shorts):\n";
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

    // ------------------------- market data -------------------------

    void Menu::printMarketUpdate(const execution::MarketUpdateResult &update) const
    {
        std::cout << std::fixed << std::setprecision(2);

        if (!update.triggers.empty())
        {
            std::cout << "\nStop orders triggered:\n";
            for (const auto &t : update.triggers)
            {
                std::cout << "  Order #" << t.orderId << " " << execution::orderTypeToString(t.type)
                          << " " << execution::orderSideToString(t.side) << " " << t.quantity << " "
                          << t.symbol << " triggered at market price $" << t.marketPrice << " -> ";
                if (t.rejected)
                {
                    std::cout << "REJECTED at trigger (" << t.note << "); order cancelled\n";
                }
                else
                {
                    std::cout << execution::orderStatusToString(t.status) << " (" << t.filledQuantity
                              << "/" << t.quantity << " filled";
                    if (t.filledQuantity > 0)
                    {
                        std::cout << ", avg $" << t.averagePrice;
                    }
                    std::cout << ")\n";
                }
            }
        }

        if (!update.userFills.empty())
        {
            std::cout << "\nYour fills during this update:\n";
            for (const auto &f : update.userFills)
            {
                const bool userBuy = (f.buyParticipant == execution::kUserId);
                std::cout << "  Order #" << (userBuy ? f.buyOrderId : f.sellOrderId) << "  "
                          << (userBuy ? "BUY " : "SELL ") << f.quantity << " " << f.symbol
                          << " @ $" << f.price << "\n";
            }
        }

        if (update.liquidation.marginCallDetected)
        {
            printLiquidationReport(update.liquidation);
        }
    }

    void Menu::handleMarketTick()
    {
        market_.tick();
        index_.update(market_);

        execution::MarketUpdateResult update;
        try
        {
            // Re-centres order-book liquidity on the new prices, fires any stop
            // orders whose trigger was reached, then runs the margin check.
            update = broker_.onMarketUpdate();
        }
        catch (const std::exception &e)
        {
            std::cout << "Warning: market update processing failed: " << e.what() << "\n";
        }

        std::cout << "\nMarket ticked forward. Prices, volume and history updated.\n";
        std::cout << "Order-book liquidity re-centred around the new prices.\n";
        std::cout << "(Holdings are unaffected by a tick unless one of your orders was filled.)\n";
        printMarketUpdate(update);
    }

    void Menu::handleMarketIndex() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- " << index_.name() << " ---\n";
        std::cout << "Value      : " << index_.value() << "\n";
        std::cout << "Change     : " << index_.change() << "\n";
        std::cout << "% Change   : " << index_.percentChange() << "%\n";
        std::cout << "Basket (" << index_.basket().size() << " stocks): ";
        for (std::size_t i = 0; i < index_.basket().size(); ++i)
        {
            std::cout << index_.basket()[i];
            if (i + 1 < index_.basket().size())
            {
                std::cout << ", ";
            }
        }
        std::cout << "\n";
    }

    void Menu::handleMarketStatistics() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Market Statistics ---\n";
        std::cout << "Advancing : " << market::MarketStatistics::advancingCount(market_) << "\n";
        std::cout << "Declining : " << market::MarketStatistics::decliningCount(market_) << "\n";
        std::cout << "Unchanged : " << market::MarketStatistics::unchangedCount(market_) << "\n";
        std::cout << "Total Volume   : " << market::MarketStatistics::totalVolume(market_) << "\n";
        std::cout << "Total Turnover : $" << market::MarketStatistics::totalTurnover(market_) << "\n";

        std::cout << "\nTop Gainers:\n";
        for (const auto &m : market::MarketStatistics::topGainers(market_))
        {
            std::cout << "  " << std::left << std::setw(8) << m.symbol
                      << std::right << std::setw(8) << m.percentChange << "%\n";
        }

        std::cout << "\nTop Losers:\n";
        for (const auto &m : market::MarketStatistics::topLosers(market_))
        {
            std::cout << "  " << std::left << std::setw(8) << m.symbol
                      << std::right << std::setw(8) << m.percentChange << "%\n";
        }

        std::cout << "\nMost Active by Volume:\n";
        for (const auto &a : market::MarketStatistics::mostActiveByVolume(market_))
        {
            std::cout << "  " << std::left << std::setw(8) << a.symbol
                      << std::right << std::setw(10) << a.volume << "\n";
        }

        std::cout << "\nMost Active by Turnover:\n";
        for (const auto &a : market::MarketStatistics::mostActiveByTurnover(market_))
        {
            std::cout << "  " << std::left << std::setw(8) << a.symbol
                      << std::right << std::setw(14) << a.turnover << "\n";
        }
    }

    void Menu::handlePriceHistory() const
    {
        std::string symbol = readSymbolInput("Enter stock symbol: ");
        if (!market_.exists(symbol))
        {
            std::cout << "Unknown symbol: " << symbol << "\n";
            return;
        }

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Price History: " << symbol << " ---\n";
        std::cout << "Highest recorded: " << market_.highestPrice(symbol) << "\n";
        std::cout << "Lowest recorded : " << market_.lowestPrice(symbol) << "\n\n";

        const auto &bars = market_.priceHistory(symbol);
        std::size_t start = bars.size() > 10 ? bars.size() - 10 : 0;
        std::cout << "Last " << (bars.size() - start) << " bar(s) (Open/High/Low/Close/Volume):\n";
        for (std::size_t i = start; i < bars.size(); ++i)
        {
            const auto &bar = bars[i];
            std::cout << "  O:" << bar.open << " H:" << bar.high
                      << " L:" << bar.low << " C:" << bar.close
                      << " V:" << bar.volume << "\n";
        }
    }

    void Menu::printAccountRisk() const
    {
        const risk::AccountRisk account = broker_.accountRisk();
        const risk::RiskConfig &config = broker_.riskConfig();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Risk & Margin ---\n";
        std::cout << "Status                 : "
                  << (account.marginCall() ? "MARGIN CALL" : "HEALTHY") << "\n";
        std::cout << "Cash                   : $" << account.cash << "\n";
        std::cout << "Long Market Value      : $" << account.longMarketValue << "\n";
        std::cout << "Short Market Value     : $" << account.shortMarketValue << "\n";
        std::cout << "Equity                 : $" << account.equity << "\n";
        std::cout << "Gross Exposure         : $" << account.grossExposure << "\n";
        std::cout << "Leverage               : " << account.leverage << "x\n";
        std::cout << "Initial Margin Required: $" << account.initialMarginRequired << "\n";
        std::cout << "Maintenance Required   : $" << account.maintenanceMarginRequired << "\n";
        std::cout << "Available Margin       : $" << account.availableMargin << "\n";
        std::cout << "Buying Power           : $" << broker_.buyingPower() << "\n";
        std::cout << "Reserved Cash          : $" << broker_.reservedCash() << "\n";

        std::cout << "\nRisk Limits:\n";
        std::cout << "  Short selling        : " << (config.shortSellingEnabled ? "ENABLED" : "DISABLED") << "\n";
        std::cout << "  Initial margin       : " << config.initialMarginRate * 100.0 << "%\n";
        std::cout << "  Maintenance margin   : " << config.maintenanceMarginRate * 100.0 << "%\n";
        std::cout << "  Max order quantity   : "
                  << (config.maxOrderQuantity > 0 ? std::to_string(config.maxOrderQuantity) : "unlimited") << "\n";
        std::cout << "  Max position size    : "
                  << (config.maxPositionQuantity > 0 ? std::to_string(config.maxPositionQuantity) : "unlimited") << "\n";
        std::cout << "  Max gross exposure   : $";
        if (config.maxGrossExposure > 0.0)
            std::cout << config.maxGrossExposure;
        else
            std::cout << "unlimited";
        std::cout << "\n";

        if (account.marginCall())
        {
            std::cout << "\n*** MARGIN CALL ACTIVE ***\n";
            std::cout << "Automatic liquidation is "
                      << (config.autoLiquidation ? "enabled" : "disabled") << ".\n";
        }
    }

    void Menu::printLiquidationReport(const execution::LiquidationReport &report) const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Forced Liquidation Report ---\n";
        std::cout << "Margin call detected: " << (report.marginCallDetected ? "YES" : "NO") << "\n";
        if (!report.marginCallDetected)
        {
            return;
        }

        std::cout << "Orders cancelled   : " << report.cancelledOrders << "\n";
        std::cout << "Before liquidation : equity=$" << report.before.equity
                  << ", gross=$" << report.before.grossExposure
                  << ", maintenance=$" << report.before.maintenanceMarginRequired << "\n";

        if (report.events.empty())
        {
            std::cout << "Liquidation trades  : none\n";
        }
        else
        {
            std::cout << "Liquidation trades:\n";
            for (const auto &event : report.events)
            {
                std::cout << "  Order #" << event.orderId << " "
                          << execution::orderSideToString(event.side) << " "
                          << event.filledQuantity << "/" << event.requestedQuantity << " "
                          << event.symbol << " @ avg $" << event.averagePrice << "\n";
            }
        }

        std::cout << "After liquidation  : equity=$" << report.after.equity
                  << ", gross=$" << report.after.grossExposure
                  << ", maintenance=$" << report.after.maintenanceMarginRequired << "\n";
        std::cout << "Status             : " << (report.resolved ? "MARGIN CALL RESOLVED" : "STILL IN MARGIN CALL") << "\n";
    }

    void Menu::handleRiskAndMargin()
    {
        std::cout << "\n--- Risk & Margin ---\n";
        printAccountRisk();
        std::cout << "\n1. Refresh / enforce margin check\n";
        std::cout << "2. Back to Main Menu\n";

        int choice = readIntInput("Select option (1-2): ");
        if (choice != 1)
        {
            return;
        }

        try
        {
            execution::LiquidationReport report = broker_.enforceMargin();
            if (report.marginCallDetected)
            {
                printLiquidationReport(report);
            }
            else
            {
                std::cout << "\nNo margin call. Account is within the configured maintenance requirement.\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "Margin check failed: " << e.what() << "\n";
        }
    }

} // namespace ui
