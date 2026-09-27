#include "Market/Market.h"
#include "Portfolio/Portfolio.h"
#include "UI/Menu.h"

// Seeds the simulated market with fictional stocks across several sectors.
static market::Market createSeededMarket()
{
    market::Market market;

    market.addStock(market::Stock("NOVA", "Nova Technologies", market::Sector::Technology, 150.00));
    market.addStock(market::Stock("QBIT", "Qubit Systems", market::Sector::Technology, 320.50));
    market.addStock(market::Stock("SOLR", "Solaris Energy", market::Sector::Energy, 75.25));
    market.addStock(market::Stock("PETR", "Petro Dynamics", market::Sector::Energy, 62.10));
    market.addStock(market::Stock("BNKX", "Bankex Financial", market::Sector::Finance, 98.40));
    market.addStock(market::Stock("TRUST", "TrustCore Capital", market::Sector::Finance, 54.75));
    market.addStock(market::Stock("MEDI", "MediCare Plus", market::Sector::Healthcare, 210.00));
    market.addStock(market::Stock("CURA", "Cura Biosciences", market::Sector::Healthcare, 88.60));

    return market;
}

int main()
{
    market::Market market = createSeededMarket();
    portfolio::Portfolio portfolio(100000.00); // starting virtual cash

    ui::Menu menu(market, portfolio);
    menu.run();

    return 0;
}