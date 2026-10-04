#include "PortfolioPage.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <cmath>

#include "../Market/Market.h"

namespace gui
{
namespace
{
QString money(double value) { return QStringLiteral("$%1").arg(value, 0, 'f', 2); }
QString percent(double value) { return QStringLiteral("%1%").arg(value, 0, 'f', 2); }
QColor pnlColor(double value)
{
    if (value > 1e-9) return QColor(QStringLiteral("#35c759"));
    if (value < -1e-9) return QColor(QStringLiteral("#ff453a"));
    return QColor(QStringLiteral("#9aa4b2"));
}
void put(QTableWidget *table, int row, int column, const QString &text,
         const QColor &color = QColor(), bool right = false)
{
    auto *item = new QTableWidgetItem(text);
    if (color.isValid()) item->setForeground(QBrush(color));
    if (right) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, column, item);
}
void setupTable(QTableWidget *table, const QStringList &headers)
{
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
}
}

PortfolioPage::PortfolioPage(AppContext &context, QWidget *parent)
    : QWidget(parent), context_(context)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 20);
    root->setSpacing(14);

    auto *summary = new QGroupBox(QStringLiteral("Portfolio Summary"), this);
    auto *grid = new QGridLayout(summary);
    const QStringList labels = {
        QStringLiteral("Equity"), QStringLiteral("Cash"), QStringLiteral("Total P&L"),
        QStringLiteral("Total Return"), QStringLiteral("Gross Exposure"), QStringLiteral("Positions")};
    QLabel **values[] = {&equity_, &cash_, &pnl_, &return_, &exposure_, &positions_};
    for (int i = 0; i < labels.size(); ++i)
    {
        auto *label = new QLabel(labels[i], summary);
        label->setObjectName(QStringLiteral("cardTitle"));
        auto *value = new QLabel(summary);
        value->setObjectName(QStringLiteral("metricValue"));
        *values[i] = value;
        const int row = i / 3;
        const int col = i % 3;
        grid->addWidget(label, row * 2, col);
        grid->addWidget(value, row * 2 + 1, col);
    }
    root->addWidget(summary);

    auto *positionsBox = new QGroupBox(QStringLiteral("Positions"), this);
    auto *positionsLayout = new QVBoxLayout(positionsBox);
    positionsTable_ = new QTableWidget(0, 0, positionsBox);
    setupTable(positionsTable_, {QStringLiteral("Symbol"), QStringLiteral("Side"), QStringLiteral("Qty"),
                                QStringLiteral("Avg Entry"), QStringLiteral("Current"), QStringLiteral("Exposure"),
                                QStringLiteral("P&L"), QStringLiteral("P&L %")});
    positionsLayout->addWidget(positionsTable_);
    root->addWidget(positionsBox, 2);

    auto *bottom = new QHBoxLayout();
    auto *allocationBox = new QGroupBox(QStringLiteral("Sector Allocation"), this);
    auto *allocationLayout = new QVBoxLayout(allocationBox);
    allocationTable_ = new QTableWidget(0, 0, allocationBox);
    setupTable(allocationTable_, {QStringLiteral("Sector"), QStringLiteral("Long"), QStringLiteral("Short"),
                                  QStringLiteral("Gross"), QStringLiteral("Weight")});
    allocationLayout->addWidget(allocationTable_);
    bottom->addWidget(allocationBox, 1);

    auto *largestBox = new QGroupBox(QStringLiteral("Largest Positions"), this);
    auto *largestLayout = new QVBoxLayout(largestBox);
    largestTable_ = new QTableWidget(0, 0, largestBox);
    setupTable(largestTable_, {QStringLiteral("Symbol"), QStringLiteral("Side"),
                              QStringLiteral("Exposure"), QStringLiteral("Weight")});
    largestLayout->addWidget(largestTable_);
    bottom->addWidget(largestBox, 1);
    root->addLayout(bottom, 1);

    connect(&context_, &AppContext::stateChanged, this, &PortfolioPage::refresh);
    refresh();
}

void PortfolioPage::refresh()
{
    const analytics::PortfolioReport report = context_.getIntelligence().report();
    equity_->setText(money(report.overview.equity));
    cash_->setText(money(report.overview.cash));
    pnl_->setText(money(report.overview.totalPnL));
    pnl_->setStyleSheet(QStringLiteral("color: %1;").arg(pnlColor(report.overview.totalPnL).name()));
    return_->setText(percent(report.overview.totalReturnPercent));
    return_->setStyleSheet(QStringLiteral("color: %1;").arg(pnlColor(report.overview.totalReturnPercent).name()));
    exposure_->setText(money(report.overview.grossExposure));
    positions_->setText(QStringLiteral("%1  (%2 long / %3 short)")
        .arg(static_cast<qulonglong>(report.overview.positionCount))
        .arg(static_cast<qulonglong>(report.overview.longPositionCount))
        .arg(static_cast<qulonglong>(report.overview.shortPositionCount)));

    positionsTable_->setRowCount(0);
    for (const auto &p : report.positions)
    {
        const int row = positionsTable_->rowCount();
        positionsTable_->insertRow(row);
        put(positionsTable_, row, 0, QString::fromStdString(p.symbol));
        put(positionsTable_, row, 1, p.side == analytics::PositionSide::Long ? QStringLiteral("LONG") : QStringLiteral("SHORT"));
        put(positionsTable_, row, 2, QString::number(std::abs(p.signedQuantity)), QColor(), true);
        put(positionsTable_, row, 3, money(p.avgEntryPrice), QColor(), true);
        put(positionsTable_, row, 4, money(p.currentPrice), QColor(), true);
        put(positionsTable_, row, 5, money(p.exposure), QColor(), true);
        put(positionsTable_, row, 6, money(p.unrealizedPnL), pnlColor(p.unrealizedPnL), true);
        put(positionsTable_, row, 7, percent(p.unrealizedPnLPercent), pnlColor(p.unrealizedPnLPercent), true);
    }

    allocationTable_->setRowCount(0);
    for (const auto &s : report.allocation.sectors)
    {
        const int row = allocationTable_->rowCount();
        allocationTable_->insertRow(row);
        put(allocationTable_, row, 0, QString::fromStdString(market::sectorToString(s.sector)));
        put(allocationTable_, row, 1, money(s.longValue), QColor(), true);
        put(allocationTable_, row, 2, money(s.shortValue), QColor(), true);
        put(allocationTable_, row, 3, money(s.grossValue), QColor(), true);
        put(allocationTable_, row, 4, percent(s.grossPercent), QColor(), true);
    }

    largestTable_->setRowCount(0);
    for (const auto &p : report.allocation.largestPositions)
    {
        const int row = largestTable_->rowCount();
        largestTable_->insertRow(row);
        put(largestTable_, row, 0, QString::fromStdString(p.symbol));
        put(largestTable_, row, 1, p.side == analytics::PositionSide::Long ? QStringLiteral("LONG") : QStringLiteral("SHORT"));
        put(largestTable_, row, 2, money(p.exposure), QColor(), true);
        put(largestTable_, row, 3, percent(p.grossWeightPercent), QColor(), true);
    }
}
}
