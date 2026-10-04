#include "RiskPage.h"

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

#include <cmath>

#include "../Market/Market.h"

namespace gui
{
namespace
{
QString money(double value) { return QStringLiteral("$%1").arg(value, 0, 'f', 2); }
QString percent(double value)
{
    return std::isfinite(value) ? QStringLiteral("%1%").arg(value, 0, 'f', 2) : QStringLiteral("∞");
}
QString multiple(double value)
{
    return std::isfinite(value) ? QStringLiteral("%1x").arg(value, 0, 'f', 2) : QStringLiteral("∞");
}
void put(QTableWidget *table, int row, int column, const QString &text,
         const QColor &color = QColor(), bool right = false)
{
    auto *item = new QTableWidgetItem(text);
    if (color.isValid()) item->setForeground(QBrush(color));
    if (right) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, column, item);
}
QString warningType(analytics::WarningType type)
{
    switch (type)
    {
    case analytics::WarningType::MarginCall: return QStringLiteral("Margin Call");
    case analytics::WarningType::PositionConcentration: return QStringLiteral("Position Concentration");
    case analytics::WarningType::SectorConcentration: return QStringLiteral("Sector Concentration");
    }
    return QStringLiteral("Unknown");
}
QString warningTarget(const analytics::RiskWarning &w)
{
    if (w.type == analytics::WarningType::PositionConcentration)
        return QString::fromStdString(w.symbol);
    if (w.type == analytics::WarningType::SectorConcentration)
        return QString::fromStdString(market::sectorToString(w.sector));
    return QStringLiteral("ACCOUNT");
}
}

RiskPage::RiskPage(AppContext &context, QWidget *parent)
    : QWidget(parent), context_(context)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 20);
    root->setSpacing(14);

    auto *summary = new QGroupBox(QStringLiteral("Risk & Margin"), this);
    auto *grid = new QGridLayout(summary);
    const QStringList labels = {
        QStringLiteral("Equity"), QStringLiteral("Buying Power"), QStringLiteral("Gross Exposure"),
        QStringLiteral("Leverage"), QStringLiteral("Margin Used"), QStringLiteral("Maintenance Margin"),
        QStringLiteral("Available Margin"), QStringLiteral("Margin Status"), QStringLiteral("Largest Position"),
        QStringLiteral("Largest Sector")};
    QLabel **values[] = {&equity_, &buyingPower_, &grossExposure_, &leverage_, &marginUsed_,
                         &maintenance_, &availableMargin_, &status_, &largestPosition_, &largestSector_};
    for (int i = 0; i < labels.size(); ++i)
    {
        auto *label = new QLabel(labels[i], summary);
        label->setObjectName(QStringLiteral("cardTitle"));
        auto *value = new QLabel(summary);
        value->setObjectName(QStringLiteral("metricValue"));
        *values[i] = value;
        const int row = i / 5;
        const int col = i % 5;
        grid->addWidget(label, row * 2, col);
        grid->addWidget(value, row * 2 + 1, col);
    }
    root->addWidget(summary);

    auto *warningBox = new QGroupBox(QStringLiteral("Risk Warnings"), this);
    auto *warningLayout = new QVBoxLayout(warningBox);
    warnings_ = new QTableWidget(0, 5, warningBox);
    warnings_->setHorizontalHeaderLabels({QStringLiteral("Severity"), QStringLiteral("Type"),
                                          QStringLiteral("Target"), QStringLiteral("Value"), QStringLiteral("Threshold")});
    warnings_->verticalHeader()->setVisible(false);
    warnings_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    warnings_->horizontalHeader()->setStretchLastSection(true);
    warnings_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    warnings_->setSelectionMode(QAbstractItemView::NoSelection);
    warnings_->setAlternatingRowColors(true);
    warnings_->setShowGrid(false);
    warningLayout->addWidget(warnings_);
    root->addWidget(warningBox, 1);

    connect(&context_, &AppContext::stateChanged, this, &RiskPage::refresh);
    refresh();
}

void RiskPage::refresh()
{
    const analytics::RiskAnalytics r = context_.getIntelligence().riskAnalytics();
    equity_->setText(money(r.equity));
    buyingPower_->setText(money(r.buyingPower));
    grossExposure_->setText(money(r.grossExposure));
    leverage_->setText(multiple(r.leverage));
    marginUsed_->setText(money(r.marginUsed));
    maintenance_->setText(money(r.maintenanceMarginRequired));
    availableMargin_->setText(money(r.availableMargin));

    if (r.marginCall)
    {
        status_->setText(QStringLiteral("MARGIN CALL"));
        status_->setStyleSheet(QStringLiteral("color: #ff453a; font-weight: 700;"));
    }
    else
    {
        status_->setText(QStringLiteral("Healthy"));
        status_->setStyleSheet(QStringLiteral("color: #35c759; font-weight: 700;"));
    }

    if (r.largestPositionSymbol.empty())
        largestPosition_->setText(QStringLiteral("None"));
    else
        largestPosition_->setText(QStringLiteral("%1 · %2 (%3)")
            .arg(QString::fromStdString(r.largestPositionSymbol))
            .arg(money(r.largestPositionValue))
            .arg(percent(r.largestPositionPercent)));

    if (!r.largestSector.has_value())
        largestSector_->setText(QStringLiteral("None"));
    else
        largestSector_->setText(QStringLiteral("%1 · %2 (%3)")
            .arg(QString::fromStdString(market::sectorToString(*r.largestSector)))
            .arg(money(r.largestSectorValue))
            .arg(percent(r.largestSectorPercent)));

    warnings_->setRowCount(0);
    for (const auto &w : r.warnings)
    {
        const int row = warnings_->rowCount();
        warnings_->insertRow(row);
        const bool critical = w.severity == analytics::WarningSeverity::Critical;
        const QColor color = critical ? QColor(QStringLiteral("#ff453a")) : QColor(QStringLiteral("#ffcc00"));
        put(warnings_, row, 0, critical ? QStringLiteral("CRITICAL") : QStringLiteral("WARNING"), color);
        put(warnings_, row, 1, warningType(w.type));
        put(warnings_, row, 2, warningTarget(w));
        put(warnings_, row, 3, percent(w.value), QColor(), true);
        put(warnings_, row, 4, percent(w.threshold), QColor(), true);
    }
}
}
