#ifndef MARKETSIM_GUI_MAINWINDOW_H
#define MARKETSIM_GUI_MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include "AppContext.h"

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace gui
{
    class DashboardPage;
    class MarketPage;
    class StockDetailPage;
    class TradingPage;
    class PortfolioPage;
    class RiskPage;
    class ChartsPage;
    class ReplayPage;
    class BacktestPage;

    class MainWindow : public QMainWindow
    {
        Q_OBJECT
    public:
        explicit MainWindow(AppContext &context, QWidget *parent = nullptr);

    private slots:
        void onNavChanged(int row);
        void showStockDetail(const QString &symbol);
        void updateClock();
        void onTickAdvanced(const QString &summary);

    private:
        enum PageIndex
        {
            PageDashboard = 0,
            PageMarket,
            PageStockDetail,
            PageTrading,
            PagePortfolio,
            PageRisk,
            PageCharts,
            PageReplay,
            PageBacktesting
        };
        void refreshCurrentPage();

        AppContext &context_;
        QListWidget *nav_ = nullptr;
        QStackedWidget *stack_ = nullptr;
        QLabel *pageTitle_ = nullptr;
        QLabel *clockLabel_ = nullptr;
        QPushButton *tickButton_ = nullptr;
        DashboardPage *dashboardPage_ = nullptr;
        MarketPage *marketPage_ = nullptr;
        StockDetailPage *stockDetailPage_ = nullptr;
        TradingPage *tradingPage_ = nullptr;
        PortfolioPage *portfolioPage_ = nullptr;
        RiskPage *riskPage_ = nullptr;
        ChartsPage *chartsPage_ = nullptr;
        ReplayPage *replayPage_ = nullptr;
        BacktestPage *backtestPage_ = nullptr;
    };
}

#endif
