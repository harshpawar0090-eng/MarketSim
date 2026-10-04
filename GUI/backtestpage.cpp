#include "BacktestPage.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <exception>
#include <memory>
#include <string>

#include "../Backtest/BacktestEngine.h"
#include "../Backtest/Strategy.h"
#include "ChartWidgets.h"
#include "DatasetSelector.h"
#include "Format.h"

namespace gui
{

    QLabel *BacktestPage::addMetric(QGridLayout *grid, int row, int column, const QString &title)
    {
        auto *name = new QLabel(title, this);
        name->setObjectName(QStringLiteral("mutedLabel"));
        auto *value = new QLabel(QStringLiteral("-"), this);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QFont f = value->font();
        f.setBold(true);
        value->setFont(f);
        grid->addWidget(name, row, column * 2);
        grid->addWidget(value, row, column * 2 + 1);
        return value;
    }

    BacktestPage::BacktestPage(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *pageLayout = new QVBoxLayout(this);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        pageLayout->addWidget(scroll);

        auto *inner = new QWidget(scroll);
        scroll->setWidget(inner);
        auto *root = new QVBoxLayout(inner);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(10);

        selector_ = new DatasetSelector(context_, inner);
        root->addWidget(selector_);

        // ---- strategy parameters ----
        auto *paramBox = new QGroupBox(QStringLiteral("Strategy"), inner);
        auto *form = new QFormLayout(paramBox);
        strategy_ = new QComboBox(paramBox);
        strategy_->addItem(QStringLiteral("Buy & Hold"));
        strategy_->addItem(QStringLiteral("Moving Average Cross"));
        symbol_ = new QComboBox(paramBox);
        quantity_ = new QSpinBox(paramBox);
        quantity_->setRange(1, 5000);
        quantity_->setValue(100);
        shortWindow_ = new QSpinBox(paramBox);
        shortWindow_->setRange(2, 200);
        shortWindow_->setValue(5);
        longWindow_ = new QSpinBox(paramBox);
        longWindow_->setRange(3, 400);
        longWindow_->setValue(20);
        startingCash_ = new QDoubleSpinBox(paramBox);
        startingCash_->setRange(1000.0, 100000000.0);
        startingCash_->setDecimals(2);
        startingCash_->setSingleStep(10000.0);
        startingCash_->setValue(100000.0);
        startingCash_->setPrefix(QStringLiteral("$ "));
        commission_ = new QDoubleSpinBox(paramBox);
        commission_->setRange(0.0, 1000.0);
        commission_->setDecimals(2);
        commission_->setValue(0.0);
        commission_->setPrefix(QStringLiteral("$ "));
        slippage_ = new QDoubleSpinBox(paramBox);
        slippage_->setRange(0.0, 500.0);
        slippage_->setDecimals(1);
        slippage_->setValue(0.0);
        slippage_->setSuffix(QStringLiteral(" bps"));
        run_ = new QPushButton(QStringLiteral("Run backtest"), paramBox);
        run_->setObjectName(QStringLiteral("primaryButton"));

        form->addRow(QStringLiteral("Strategy"), strategy_);
        form->addRow(QStringLiteral("Symbol (also the benchmark)"), symbol_);
        form->addRow(QStringLiteral("Order quantity"), quantity_);
        form->addRow(QStringLiteral("Short MA window"), shortWindow_);
        form->addRow(QStringLiteral("Long MA window"), longWindow_);
        form->addRow(QStringLiteral("Starting capital"), startingCash_);
        form->addRow(QStringLiteral("Commission per order"), commission_);
        form->addRow(QStringLiteral("Market-order slippage"), slippage_);
        form->addRow(run_);

        // ---- results ----
        auto *resultBox = new QGroupBox(QStringLiteral("Results"), inner);
        auto *resultLayout = new QVBoxLayout(resultBox);
        message_ = new QLabel(resultBox);
        message_->setObjectName(QStringLiteral("mutedLabel"));
        message_->setWordWrap(true);
        resultLayout->addWidget(message_);
        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(8);
        strategyName_ = addMetric(grid, 0, 0, QStringLiteral("Strategy"));
        startingEquity_ = addMetric(grid, 1, 0, QStringLiteral("Starting capital"));
        endingEquity_ = addMetric(grid, 2, 0, QStringLiteral("Ending equity"));
        strategyReturn_ = addMetric(grid, 3, 0, QStringLiteral("Strategy return"));
        benchmarkReturn_ = addMetric(grid, 4, 0, QStringLiteral("Benchmark (buy & hold)"));
        excessReturn_ = addMetric(grid, 5, 0, QStringLiteral("Strategy vs benchmark"));
        trades_ = addMetric(grid, 0, 1, QStringLiteral("Trades"));
        winLoss_ = addMetric(grid, 1, 1, QStringLiteral("Winning / losing"));
        winRate_ = addMetric(grid, 2, 1, QStringLiteral("Win rate"));
        maxDrawdown_ = addMetric(grid, 3, 1, QStringLiteral("Max drawdown"));
        costs_ = addMetric(grid, 4, 1, QStringLiteral("Transaction costs"));
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(3, 1);
        resultLayout->addLayout(grid);
        resultLayout->addStretch(1);

        auto *top = new QHBoxLayout();
        top->setSpacing(12);
        top->addWidget(paramBox, 1);
        top->addWidget(resultBox, 2);
        root->addLayout(top);

        // ---- equity curve + trade log ----
        auto *tabs = new QTabWidget(inner);
        equityChart_ = new LineChartWidget(tabs);
        equityChart_->setEmptyText(QStringLiteral("Run a backtest to see the equity curve."));
        equityChart_->setMinimumHeight(240);
        tradeTable_ = new QTableWidget(0, 6, tabs);
        tradeTable_->setHorizontalHeaderLabels({QStringLiteral("Time"), QStringLiteral("Symbol"), QStringLiteral("Side"),
                                                QStringLiteral("Quantity"), QStringLiteral("Price"),
                                                QStringLiteral("Realized P&L")});
        tradeTable_->verticalHeader()->setVisible(false);
        tradeTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tradeTable_->setSelectionMode(QAbstractItemView::NoSelection);
        tradeTable_->setAlternatingRowColors(true);
        tradeTable_->setShowGrid(false);
        tradeTable_->setFocusPolicy(Qt::NoFocus);
        tradeTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        tradeTable_->setMinimumHeight(240);
        tabs->addTab(equityChart_, QStringLiteral("Equity curve"));
        tabs->addTab(tradeTable_, QStringLiteral("Trade log"));
        root->addWidget(tabs, 1);

        auto *note = new QLabel(QStringLiteral("Backtests run on their own isolated Market, Portfolio and Broker built by BacktestEngine; "
                                               "your live account is never touched. Orders still pass through the normal risk checks "
                                               "(margin account, shorting enabled)."),
                                inner);
        note->setObjectName(QStringLiteral("mutedLabel"));
        note->setWordWrap(true);
        root->addWidget(note);

        connect(selector_, &DatasetSelector::datasetChanged, this, &BacktestPage::onDatasetChanged);
        connect(strategy_, &QComboBox::currentIndexChanged, this, [this](int)
                { onStrategyChanged(); });
        connect(run_, &QPushButton::clicked, this, &BacktestPage::runBacktest);

        onStrategyChanged();
        onDatasetChanged();
        clearResults(QStringLiteral("Choose a strategy and press \"Run backtest\"."));
    }

    void BacktestPage::onStrategyChanged()
    {
        const bool ma = strategy_->currentIndex() == 1;
        shortWindow_->setEnabled(ma);
        longWindow_->setEnabled(ma);
    }

    void BacktestPage::onDatasetChanged()
    {
        const QString previous = symbol_->currentText();
        symbol_->clear();
        if (selector_->hasDataset())
        {
            for (const std::string &s : selector_->dataset().symbols())
            {
                symbol_->addItem(QString::fromStdString(s));
            }
            const int keep = symbol_->findText(previous);
            if (keep >= 0)
            {
                symbol_->setCurrentIndex(keep);
            }
        }
        run_->setEnabled(selector_->hasDataset());
    }

    void BacktestPage::clearResults(const QString &message)
    {
        message_->setText(message);
        for (QLabel *l : {strategyName_, startingEquity_, endingEquity_, strategyReturn_, benchmarkReturn_, excessReturn_,
                          trades_, winLoss_, winRate_, maxDrawdown_, costs_})
        {
            l->setText(QStringLiteral("-"));
            l->setStyleSheet(QString());
        }
        equityChart_->setSeries({}, {});
        equityChart_->clearBaseline();
        tradeTable_->setRowCount(0);
    }

    void BacktestPage::runBacktest()
    {
        if (!selector_->hasDataset() || symbol_->currentText().isEmpty())
        {
            clearResults(QStringLiteral("Build a dataset first."));
            return;
        }

        const std::string symbol = symbol_->currentText().toStdString();
        if (strategy_->currentIndex() == 1 && shortWindow_->value() >= longWindow_->value())
        {
            clearResults(QStringLiteral("The short moving-average window must be smaller than the long window."));
            return;
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        try
        {
            const historical::HistoricalDataSet &data = selector_->dataset();

            backtest::BacktestConfig config;
            config.startingCash = startingCash_->value();
            config.commissionPerOrder = commission_->value();
            config.slippageBps = slippage_->value();
            config.benchmarkSymbol = symbol;
            config.riskConfig = defaultGuiRiskConfig();
            for (const std::string &s : data.symbols())
            {
                if (context_.getMarket().exists(s))
                {
                    backtest::InstrumentMeta meta;
                    meta.name = context_.getMarket().getInstrument(s).name();
                    meta.sector = context_.getMarket().getInstrument(s).sector();
                    config.instrumentMeta[s] = meta;
                }
            }

            std::unique_ptr<backtest::Strategy> strategy;
            if (strategy_->currentIndex() == 0)
            {
                strategy = std::make_unique<backtest::BuyAndHoldStrategy>(symbol, quantity_->value());
            }
            else
            {
                strategy = std::make_unique<backtest::MovingAverageCrossStrategy>(symbol, shortWindow_->value(),
                                                                                  longWindow_->value(), quantity_->value());
            }

            backtest::BacktestEngine engine(data, config);
            const backtest::BacktestResult r = engine.run(*strategy);

            strategyName_->setText(QString::fromStdString(r.strategyName));
            startingEquity_->setText(fmt::money(r.startingEquity));
            endingEquity_->setText(fmt::money(r.endingEquity));
            endingEquity_->setStyleSheet(QStringLiteral("color: %1;").arg(fmt::changeColor(r.endingEquity - r.startingEquity).name()));
            strategyReturn_->setText(fmt::percent(r.totalReturnPercent));
            strategyReturn_->setStyleSheet(QStringLiteral("color: %1;").arg(fmt::changeColor(r.totalReturnPercent).name()));
            benchmarkReturn_->setText(QStringLiteral("%1  (%2)").arg(fmt::percent(r.benchmarkReturnPercent), QString::fromStdString(symbol)));
            benchmarkReturn_->setStyleSheet(QStringLiteral("color: %1;").arg(fmt::changeColor(r.benchmarkReturnPercent).name()));
            const double excess = r.totalReturnPercent - r.benchmarkReturnPercent;
            excessReturn_->setText(fmt::percent(excess));
            excessReturn_->setStyleSheet(QStringLiteral("color: %1;").arg(fmt::changeColor(excess).name()));
            trades_->setText(fmt::integer(r.numberOfTrades));
            winLoss_->setText(QStringLiteral("%1 / %2").arg(r.winningTrades).arg(r.losingTrades));
            if (r.winningTrades + r.losingTrades > 0)
            {
                winRate_->setText(QStringLiteral("%1%").arg(r.winRate, 0, 'f', 1));
            }
            else
            {
                winRate_->setText(QStringLiteral("n/a (no closed trades)"));
            }
            maxDrawdown_->setText(QStringLiteral("%1%").arg(r.maxDrawdownPercent, 0, 'f', 2));
            maxDrawdown_->setStyleSheet(r.maxDrawdownPercent > 0.0 ? QStringLiteral("color: %1;").arg(fmt::changeColor(-1.0).name()) : QString());
            costs_->setText(fmt::money(r.totalTransactionCosts));

            QString msg = QStringLiteral("Backtest finished: %1 steps over %2.")
                              .arg(static_cast<qlonglong>(r.equityCurve.size()))
                              .arg(selector_->description());
            if (r.numberOfTrades == 0)
            {
                msg += QStringLiteral(" The strategy placed no trades (for a moving-average cross, try shorter windows "
                                      "or more bars).");
            }
            message_->setText(msg);

            QVector<qint64> times;
            QVector<double> values;
            times.reserve(static_cast<int>(r.equityCurve.size()));
            values.reserve(static_cast<int>(r.equityCurve.size()));
            for (const backtest::EquityPoint &pt : r.equityCurve)
            {
                times.push_back(static_cast<qint64>(pt.timestamp));
                values.push_back(pt.equity);
            }
            equityChart_->setSeries(times, values);
            equityChart_->setBaseline(r.startingEquity, QStringLiteral("Starting capital"));

            tradeTable_->setRowCount(static_cast<int>(r.trades.size()));
            for (std::size_t i = 0; i < r.trades.size(); ++i)
            {
                const backtest::TradeRecord &t = r.trades[i];
                const bool buy = t.side == portfolio::TransactionSide::Buy;
                const QString cells[] = {
                    QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t.timestamp)).toString(QStringLiteral("yyyy-MM-dd HH:mm")),
                    QString::fromStdString(t.symbol),
                    buy ? QStringLiteral("BUY") : QStringLiteral("SELL"),
                    fmt::integer(t.quantity),
                    fmt::money(t.price),
                    t.realizedPnL == 0.0 ? QStringLiteral("-") : fmt::signedMoney(t.realizedPnL)};
                for (int c = 0; c < 6; ++c)
                {
                    auto *item = new QTableWidgetItem(cells[c]);
                    if (c >= 3)
                    {
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    }
                    if (c == 5 && t.realizedPnL != 0.0)
                    {
                        item->setForeground(fmt::changeColor(t.realizedPnL));
                    }
                    tradeTable_->setItem(static_cast<int>(i), c, item);
                }
            }
        }
        catch (const std::exception &e)
        {
            clearResults(QStringLiteral("Backtest failed: %1").arg(QString::fromUtf8(e.what())));
        }
        QApplication::restoreOverrideCursor();
    }

} // namespace gui