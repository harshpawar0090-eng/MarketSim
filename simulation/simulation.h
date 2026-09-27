#ifndef MARKETSIM_SIMULATION_SIMULATION_H
#define MARKETSIM_SIMULATION_SIMULATION_H

#include <string>
#include <vector>
#include <unordered_map>
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"

// Simulation module: computes hypothetical "what-if" outcomes without ever
// mutating the real Market or Portfolio. Every method here only ever
// receives const references to real state - this is enforced by the
// compiler, not just by convention.
namespace simulation
{

    // Abstract base for a price-shift scenario. Real behavioral variance
    // between bull/bear/custom shifts is what justifies runtime polymorphism
    // here, same as with Order in the Trading module.
    class Scenario
    {
    public:
        explicit Scenario(std::string name);
        virtual ~Scenario() = default;

        const std::string &name() const;

        // Returns a hypothetical price for every symbol in realMarket. Does
        // NOT modify realMarket in any way - it only reads from it.
        virtual std::unordered_map<std::string, double> apply(const market::Market &realMarket) const = 0;

    protected:
        std::string name_;
    };

    // Applies a uniform percentage increase to every stock in the market.
    class BullScenario : public Scenario
    {
    public:
        explicit BullScenario(double growthPercent);
        std::unordered_map<std::string, double> apply(const market::Market &realMarket) const override;

    private:
        double growthPercent_;
    };

    // Applies a uniform percentage decrease to every stock in the market.
    class BearScenario : public Scenario
    {
    public:
        explicit BearScenario(double declinePercent);
        std::unordered_map<std::string, double> apply(const market::Market &realMarket) const override;

    private:
        double declinePercent_;
    };

    // Applies user-specified percentage changes to specific symbols. Any
    // symbol not explicitly listed keeps its real, unchanged price.
    class CustomScenario : public Scenario
    {
    public:
        explicit CustomScenario(std::unordered_map<std::string, double> percentChangesBySymbol);
        std::unordered_map<std::string, double> apply(const market::Market &realMarket) const override;

    private:
        std::unordered_map<std::string, double> percentChangesBySymbol_;
    };

    // One line of a baseline-vs-hypothetical comparison for a single holding.
    struct HoldingComparisonLine
    {
        std::string symbol;
        int quantity;
        double baselinePrice;
        double hypotheticalPrice;
        double baselineValue;
        double hypotheticalValue;
    };

    // The full result of running a scenario against the real portfolio.
    // A pure value object - it owns its own data and has no references back
    // into the real Market or Portfolio.
    struct SimulationResult
    {
        std::string scenarioName;
        double baselineValue = 0.0;
        double hypotheticalValue = 0.0;
        std::vector<HoldingComparisonLine> lines;

        double delta() const;
        double deltaPercent() const;
    };

    // Orchestrates running a Scenario against the real (but const-only) Market
    // and Portfolio, producing a SimulationResult. Never has a non-const
    // handle to either, so it is structurally impossible for this class to
    // modify real state.
    class SimulationEngine
    {
    public:
        static SimulationResult run(const portfolio::Portfolio &realPortfolio,
                                    const market::Market &realMarket,
                                    const Scenario &scenario);
    };

} // namespace simulation

#endif // MARKETSIM_SIMULATION_SIMULATION_H