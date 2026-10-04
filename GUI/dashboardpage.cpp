#include "DashboardPage.h"

#include <QBrush>
#include <QDebug>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <exception>

#include "../Market/MarketStatistics.h"
#include "Format.h"

namespace gui
{

    namespace
    {

        QTableWidget *makeListTable(const QString &valueHeader, QWidget *parent)
        {
            auto *table = new QTableWidget(0, 2, parent);
            table->setHorizontalHeaderLabels({QStringLiteral("Symbol"), valueHeader});
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->setAlternatingRowColors(true);
            table->setShowGrid(false);
            table->setFocusPolicy(Qt::NoFocus);
            return table;
        }

        void setCell(QTableWidget *table, int row, int column, const QString &text,
                     const QColor &color = QColor(), bool rightAlign = false)
        {
            auto *item = new QTableWidgetItem(text);
            if (color.isValid())
            {
                item->setForeground(QBrush(color));
            }
            if (rightAlign)
            {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
            table->setItem(row, column, item);
        }

        // MarketStatistics rows expose .symbol and .percentChange.
        template <class Range>
        void fillPercentList(QTableWidget *table, const Range &rows, int limit)
        {
            table->setRowCount(0);
            int row = 0;
            for (const auto &m : rows)
            {
                if (row >= limit)
                {
                    break;
                }
                table->insertRow(row);
                setCell(table, row, 0, QString::fromStdString(m.symbol));
                setCell(table, row, 1, fmt::percent(m.percentChange), fmt::changeColor(m.percentChange), true);
                ++row;
            }
        }

        // MarketStatistics rows expose .symbol and .volume.
        template <class Range>
        void fillVolumeList(QTableWidget *table, const Range &rows, int limit)
        {
            table->setRowCount(0);
            int row = 0;
            for (const auto &a : rows)
            {
                if (row >= limit)
                {
                    break;
                }
                table->insertRow(row);
                setCell(table, row, 0, QString::fromStdString(a.symbol));
                setCell(table, row, 1, fmt::integer(static_cast<long long>(a.volume)), QColor(), true);
                ++row;
            }
        }

        QGroupBox *wrap(const QString &title, QWidget *content, QWidget *parent)
        {
            auto *box = new QGroupBox(title, parent);
            auto *layout = new QVBoxLayout(box);
            layout->addWidget(content);
            return box;
        }

    } // namespace

    DashboardPage::DashboardPage(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(14);

        // ---- metric cards (4 x 3) ----
        equity_ = new MetricCard(QStringLiteral("PORTFOLIO EQUITY"), this);
        cash_ = new MetricCard(QStringLiteral("CASH"), this);
        totalPnL_ = new MetricCard(QStringLiteral("TOTAL P&L"), this);
        totalReturn_ = new MetricCard(QStringLiteral("TOTAL RETURN"), this);
        buyingPower_ = new MetricCard(QStringLiteral("BUYING POWER"), this);
        grossExposure_ = new MetricCard(QStringLiteral("GROSS EXPOSURE"), this);
        leverage_ = new MetricCard(QStringLiteral("LEVERAGE"), this);
        marginStatus_ = new MetricCard(QStringLiteral("MARGIN STATUS"), this);
        index_ = new MetricCard(QStringLiteral("MARKET INDEX"), this);
        positions_ = new MetricCard(QStringLiteral("POSITIONS"), this);
        openOrders_ = new MetricCard(QStringLiteral("OPEN / PENDING ORDERS"), this);
        breadth_ = new MetricCard(QStringLiteral("MARKET BREADTH"), this);

        auto *grid = new QGridLayout();
        grid->setSpacing(12);
        MetricCard *cards[] = {equity_, cash_, totalPnL_, totalReturn_,
                               buyingPower_, grossExposure_, leverage_, marginStatus_,
                               index_, positions_, openOrders_, breadth_};
        for (int i = 0; i < 12; ++i)
        {
            grid->addWidget(cards[i], i / 4, i % 4);
        }
        for (int c = 0; c < 4; ++c)
        {
            grid->setColumnStretch(c, 1);
        }
        root->addLayout(grid);

        // ---- market overview ----
        gainers_ = makeListTable(QStringLiteral("Change %"), this);
        losers_ = makeListTable(QStringLiteral("Change %"), this);
        active_ = makeListTable(QStringLiteral("Volume"), this);

        auto *lists = new QGridLayout();
        lists->setSpacing(12);
        lists->addWidget(wrap(QStringLiteral("Top Gainers"), gainers_, this), 0, 0);
        lists->addWidget(wrap(QStringLiteral("Top Losers"), losers_, this), 0, 1);
        lists->addWidget(wrap(QStringLiteral("Most Active (Volume)"), active_, this), 0, 2);
        for (int c = 0; c < 3; ++c)
        {
            lists->setColumnStretch(c, 1);
        }
        root->addLayout(lists, 1);

        totals_ = new QLabel(this);
        totals_->setObjectName(QStringLiteral("mutedLabel"));
        root->addWidget(totals_);

        connect(&context_, &AppContext::stateChanged, this, &DashboardPage::refresh);
        refresh();
    }

    void DashboardPage::refresh()
    {
        try
        {
            const AppContext &ctx = context_;
            const analytics::PortfolioIntelligence &intel = ctx.getIntelligence();
            const analytics::PortfolioOverview o = intel.overview();
            const analytics::RiskAnalytics r = intel.riskAnalytics();
            const market::Market &mkt = ctx.getMarket();

            equity_->setValue(fmt::money(o.equity));
            equity_->setSubtitle(QStringLiteral("Cash + net positions"));

            cash_->setValue(fmt::money(o.cash), o.cash < 0.0 ? fmt::warnColor() : QColor());
            cash_->setSubtitle(o.cash < 0.0 ? QStringLiteral("Negative = margin loan")
                                            : QStringLiteral("Available balance"));

            totalPnL_->setValue(fmt::signedMoney(o.totalPnL), fmt::changeColor(o.totalPnL));
            totalPnL_->setSubtitle(QStringLiteral("Realized %1  |  Unrealized %2")
                                       .arg(fmt::signedMoney(o.realizedPnL), fmt::signedMoney(o.unrealizedPnL)));

            totalReturn_->setValue(fmt::percent(o.totalReturnPercent), fmt::changeColor(o.totalReturnPercent));
            totalReturn_->setSubtitle(QStringLiteral("vs. starting equity %1").arg(fmt::money(o.initialEquity)));

            buyingPower_->setValue(fmt::money(ctx.getBroker().buyingPower()));
            buyingPower_->setSubtitle(QStringLiteral("After open orders"));

            grossExposure_->setValue(fmt::money(o.grossExposure));
            grossExposure_->setSubtitle(QStringLiteral("Long %1  |  Short %2")
                                            .arg(fmt::money(o.longExposure), fmt::money(o.shortExposure)));

            leverage_->setValue(fmt::leverage(r.leverage));
            leverage_->setSubtitle(QStringLiteral("Gross exposure / equity"));

            if (r.marginCall)
            {
                marginStatus_->setValue(QStringLiteral("MARGIN CALL"), fmt::lossColor());
            }
            else
            {
                marginStatus_->setValue(QStringLiteral("Healthy"), fmt::gainColor());
            }
            marginStatus_->setSubtitle(QStringLiteral("Used %1  |  Available %2")
                                           .arg(fmt::money(r.marginUsed), fmt::money(r.availableMargin)));

            const market::MarketIndex &idx = ctx.getIndex();
            index_->setTitle(QString::fromStdString(idx.name()).toUpper());
            index_->setValue(fmt::number(idx.value()), fmt::changeColor(idx.change()));
            index_->setSubtitle(QStringLiteral("%1  (%2)")
                                    .arg(fmt::signedNumber(idx.change()), fmt::percent(idx.percentChange())));

            positions_->setValue(QString::number(static_cast<qlonglong>(o.positionCount)));
            positions_->setSubtitle(QStringLiteral("%1 long  |  %2 short")
                                        .arg(static_cast<qlonglong>(o.longPositionCount))
                                        .arg(static_cast<qlonglong>(o.shortPositionCount)));

            openOrders_->setValue(QString::number(static_cast<qlonglong>(ctx.getBroker().openOrders().size())));
            openOrders_->setSubtitle(QStringLiteral("Resting orders + waiting stops"));

            breadth_->setValue(QStringLiteral("%1 / %2 / %3")
                                   .arg(static_cast<qlonglong>(market::MarketStatistics::advancingCount(mkt)))
                                   .arg(static_cast<qlonglong>(market::MarketStatistics::decliningCount(mkt)))
                                   .arg(static_cast<qlonglong>(market::MarketStatistics::unchangedCount(mkt))));
            breadth_->setSubtitle(QStringLiteral("Advancing / Declining / Unchanged"));

            fillPercentList(gainers_, market::MarketStatistics::topGainers(mkt), 5);
            fillPercentList(losers_, market::MarketStatistics::topLosers(mkt), 5);
            fillVolumeList(active_, market::MarketStatistics::mostActiveByVolume(mkt), 5);

            totals_->setText(QStringLiteral("Market totals:  volume %1   \u00B7   turnover %2")
                                 .arg(fmt::integer(static_cast<long long>(market::MarketStatistics::totalVolume(mkt))),
                                      fmt::money(static_cast<double>(market::MarketStatistics::totalTurnover(mkt)))));
        }
        catch (const std::exception &e)
        {
            qWarning("Dashboard refresh failed: %s", e.what());
        }
    }

} // namespace gui