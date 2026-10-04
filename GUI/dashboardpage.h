#ifndef MARKETSIM_GUI_DASHBOARDPAGE_H
#define MARKETSIM_GUI_DASHBOARDPAGE_H

#include <QWidget>
#include "AppContext.h"
#include "MetricCard.h"

class QLabel;
class QTableWidget;

namespace gui
{

    // Live account + market summary. Every number comes from the existing
    // PortfolioIntelligence / Broker / MarketIndex / MarketStatistics APIs.
    class DashboardPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit DashboardPage(AppContext &context, QWidget *parent = nullptr);

    public slots:
        void refresh();

    private:
        AppContext &context_;

        MetricCard *equity_ = nullptr;
        MetricCard *cash_ = nullptr;
        MetricCard *totalPnL_ = nullptr;
        MetricCard *totalReturn_ = nullptr;
        MetricCard *buyingPower_ = nullptr;
        MetricCard *grossExposure_ = nullptr;
        MetricCard *leverage_ = nullptr;
        MetricCard *marginStatus_ = nullptr;
        MetricCard *index_ = nullptr;
        MetricCard *positions_ = nullptr;
        MetricCard *openOrders_ = nullptr;
        MetricCard *breadth_ = nullptr;

        QTableWidget *gainers_ = nullptr;
        QTableWidget *losers_ = nullptr;
        QTableWidget *active_ = nullptr;
        QLabel *totals_ = nullptr;
    };

} // namespace gui

#endif // MARKETSIM_GUI_DASHBOARDPAGE_H