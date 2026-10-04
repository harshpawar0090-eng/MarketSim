#include "MarketSeed.h"
#include <string>
#include <vector>

namespace app
{

    market::Market createSeededMarket()
    {
        market::Market market;
        using market::Sector;
        using market::Stock;

        // Technology
        market.addStock(Stock("NOVA", "Nova Technologies", Sector::Technology, 150.00));
        market.addStock(Stock("QBIT", "Qubit Systems", Sector::Technology, 320.50));
        market.addStock(Stock("SYNC", "SyncWave Technologies", Sector::Technology, 88.40));
        market.addStock(Stock("VERT", "Vertex Softworks", Sector::Technology, 64.75));
        market.addStock(Stock("HELX", "Helix Computing", Sector::Technology, 210.30));
        market.addStock(Stock("ORBT", "Orbital Systems", Sector::Technology, 45.60));

        // Energy
        market.addStock(Stock("SOLR", "Solaris Energy", Sector::Energy, 75.25));
        market.addStock(Stock("PETR", "Petro Dynamics", Sector::Energy, 62.10));
        market.addStock(Stock("WIND", "WindPeak Energy", Sector::Energy, 38.90));
        market.addStock(Stock("HYDN", "Hydrogen Nexus", Sector::Energy, 54.20));
        market.addStock(Stock("COAL", "Coalbridge Resources", Sector::Energy, 29.75));

        // Finance
        market.addStock(Stock("BNKX", "Bankex Financial", Sector::Finance, 98.40));
        market.addStock(Stock("TRUST", "TrustCore Capital", Sector::Finance, 54.75));
        market.addStock(Stock("CRED", "Credence Holdings", Sector::Finance, 112.60));
        market.addStock(Stock("ASUR", "AssureBank Corp", Sector::Finance, 76.30));
        market.addStock(Stock("CAPV", "Capital Vista", Sector::Finance, 65.90));

        // Healthcare
        market.addStock(Stock("MEDI", "MediCare Plus", Sector::Healthcare, 210.00));
        market.addStock(Stock("CURA", "Cura Biosciences", Sector::Healthcare, 88.60));
        market.addStock(Stock("VITA", "VitaGen Labs", Sector::Healthcare, 142.15));
        market.addStock(Stock("PHRM", "PharmaNova Inc", Sector::Healthcare, 97.80));
        market.addStock(Stock("RXCR", "RxCare Systems", Sector::Healthcare, 61.45));

        // Consumer
        market.addStock(Stock("BREW", "Brewtown Beverages", Sector::Consumer, 42.30));
        market.addStock(Stock("FESH", "Fresh Harvest Foods", Sector::Consumer, 33.75));
        market.addStock(Stock("APRL", "Apparelix Retail", Sector::Consumer, 58.20));
        market.addStock(Stock("GLOW", "GlowMart Cosmetics", Sector::Consumer, 27.90));
        market.addStock(Stock("TSTE", "Tastewell Foods", Sector::Consumer, 36.55));

        // Industrial
        market.addStock(Stock("FORG", "Forgeline Industries", Sector::Industrial, 73.40));
        market.addStock(Stock("STEL", "Steelcore Manufacturing", Sector::Industrial, 91.25));
        market.addStock(Stock("MACH", "Machworks Corp", Sector::Industrial, 48.60));
        market.addStock(Stock("BLDR", "Buildtrust Infrastructure", Sector::Industrial, 66.10));
        market.addStock(Stock("AERO", "Aerofab Industries", Sector::Industrial, 104.75));

        // Telecom
        market.addStock(Stock("CONN", "ConnectWave Telecom", Sector::Telecom, 39.80));
        market.addStock(Stock("SIGN", "SignalPoint Networks", Sector::Telecom, 52.45));
        market.addStock(Stock("LINK", "LinkSphere Communications", Sector::Telecom, 44.30));
        market.addStock(Stock("AIRW", "Airwave Mobile", Sector::Telecom, 30.95));
        market.addStock(Stock("NETC", "NetCore Telecom", Sector::Telecom, 47.60));

        // Materials
        market.addStock(Stock("ORES", "Orebridge Materials", Sector::Materials, 68.30));
        market.addStock(Stock("ALUM", "Alumnova Metals", Sector::Materials, 41.70));
        market.addStock(Stock("CHEM", "ChemCore Industries", Sector::Materials, 59.85));
        market.addStock(Stock("POLY", "Polyform Materials", Sector::Materials, 35.40));
        market.addStock(Stock("ZINX", "Zinex Resources", Sector::Materials, 27.15));

        // Utilities
        market.addStock(Stock("AQUA", "AquaGrid Utilities", Sector::Utilities, 46.90));
        market.addStock(Stock("VOLT", "VoltCore Power", Sector::Utilities, 55.25));
        market.addStock(Stock("GASL", "GasLine Utilities", Sector::Utilities, 38.60));
        market.addStock(Stock("HEAT", "Heatworks Energy", Sector::Utilities, 33.10));
        market.addStock(Stock("PWRG", "PowerGrid Corp", Sector::Utilities, 61.70));

        return market;
    }

    market::MarketIndex createMarketIndex(const market::Market &market)
    {
        std::vector<std::string> basket = {
            "NOVA", "QBIT", "SOLR", "PETR", "BNKX", "TRUST", "MEDI", "CURA",
            "BREW", "FESH", "FORG", "STEL", "CONN", "SIGN", "ORES", "AQUA", "VOLT"};
        market::MarketIndex index("MarketSim 50", basket);
        index.initialize(market);
        return index;
    }

} // namespace app