#include "Simulation.h"

namespace simulation
{

    Scenario::Scenario(std::string name) : name_(std::move(name)) {}

    const std::string &Scenario::name() const { return name_; }

    BullScenario::BullScenario(double growthPercent) : Scenario("Bull Scenario"), growthPercent_(growthPercent) {}

    std::unordered_map<std::string, double> BullScenario::apply(const market::Market &realMarket) const
    {
        std::unordered_map<std::string, double> result;
        for (const auto &[symbol, stock] : realMarket.allStocks())
        {
            result[symbol] = stock.price() * (1.0 + growthPercent_ / 100.0);
        }
        return result;
    }

    BearScenario::BearScenario(double declinePercent) : Scenario("Bear Scenario"), declinePercent_(declinePercent) {}

    std::unordered_map<std::string, double> BearScenario::apply(const market::Market &realMarket) const
    {
        std::unordered_map<std::string, double> result;
        for (const auto &[symbol, stock] : realMarket.allStocks())
        {
            result[symbol] = stock.price() * (1.0 - declinePercent_ / 100.0);
        }
        return result;
    }

    CustomScenario::CustomScenario(std::unordered_map<std::string, double> percentChangesBySymbol)
        : Scenario("Custom Scenario"), percentChangesBySymbol_(std::move(percentChangesBySymbol)) {}

    std::unordered_map<std::string, double> CustomScenario::apply(const market::Market &realMarket) const
    {
        std::unordered_map<std::string, double> result;
        for (const auto &[symbol, stock] : realMarket.allStocks())
        {
            auto it = percentChangesBySymbol_.find(symbol);
            double percentChange = (it != percentChangesBySymbol_.end()) ? it->second : 0.0;
            result[symbol] = stock.price() * (1.0 + percentChange / 100.0);
        }
        return result;
    }

    double SimulationResult::delta() const
    {
        return hypotheticalValue - baselineValue;
    }

    double SimulationResult::deltaPercent() const
    {
        if (baselineValue == 0.0)
        {
            return 0.0;
        }
        return (delta() / baselineValue) * 100.0;
    }

    SimulationResult SimulationEngine::run(const portfolio::Portfolio &realPortfolio,
                                           const market::Market &realMarket,
                                           const Scenario &scenario)
    {
        SimulationResult result;
        result.scenarioName = scenario.name();

        std::unordered_map<std::string, double> hypotheticalPrices = scenario.apply(realMarket);

        double baselineTotal = 0.0;
        double hypotheticalTotal = 0.0;

        for (const auto &[symbol, holding] : realPortfolio.holdings())
        {
            double baselinePrice = realMarket.getStock(symbol).price();

            double hypotheticalPrice = baselinePrice;
            auto it = hypotheticalPrices.find(symbol);
            if (it != hypotheticalPrices.end())
            {
                hypotheticalPrice = it->second;
            }

            double baselineValue = baselinePrice * holding.quantity();
            double hypotheticalValue = hypotheticalPrice * holding.quantity();

            baselineTotal += baselineValue;
            hypotheticalTotal += hypotheticalValue;

            HoldingComparisonLine line;
            line.symbol = symbol;
            line.quantity = holding.quantity();
            line.baselinePrice = baselinePrice;
            line.hypotheticalPrice = hypotheticalPrice;
            line.baselineValue = baselineValue;
            line.hypotheticalValue = hypotheticalValue;
            result.lines.push_back(line);
        }

        result.baselineValue = baselineTotal;
        result.hypotheticalValue = hypotheticalTotal;
        return result;
    }

} // namespace simulation