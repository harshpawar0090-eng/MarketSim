#include "ReplayPage.h"

#include <QComboBox>
#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <exception>
#include <string>

#include "../Broker/Broker.h"
#include "../Execution/ExecutionService.h"
#include "../Historical/HistoricalData.h"
#include "../Historical/MarketReplay.h"
#include "../Market/Market.h"
#include "../Portfolio/Portfolio.h"
#include "ChartWidgets.h"
#include "DatasetSelector.h"
#include "Format.h"

namespace gui
{

    // ---------------------------------------------------------------------
    // Session: the replay sandbox. Member order is initialization order.
    // Mirrors what BacktestEngine does for its own run (seed a Market from
    // the first bar of each series, build Portfolio -> ExecutionService ->
    // Broker), then hands the pieces to the existing MarketReplay.
    // ---------------------------------------------------------------------
    struct ReplayPage::Session
    {
        historical::HistoricalDataSet data;
        market::Market market;
        portfolio::Portfolio portfolio;
        execution::ExecutionService execution;
        broker::Broker broker;
        historical::MarketReplay replay;

        Session(const historical::HistoricalDataSet &d, const market::Market &liveMarket)
            : data(d),
              market(),
              portfolio(100000.0),
              execution(market, portfolio, execution::LiquidityConfig(), defaultGuiRiskConfig()),
              broker(execution),
              replay(market, broker, data)
        {
            for (const std::string &symbol : data.symbols())
            {
                std::string name = symbol;
                market::Sector sector = market::Sector::Technology;
                if (liveMarket.exists(symbol))
                {
                    name = liveMarket.getInstrument(symbol).name();
                    sector = liveMarket.getInstrument(symbol).sector();
                }
                market.addStock(market::Stock(symbol, name, sector, data.barAt(symbol, 0).open));
            }
            broker.onMarketUpdate(); // re-quote now that the market has stocks
        }
    };

    ReplayPage::ReplayPage(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(10);

        selector_ = new DatasetSelector(context_, this);
        root->addWidget(selector_);

        // ---- controls ----
        auto *controls = new QHBoxLayout();
        symbol_ = new QComboBox(this);
        symbol_->setMinimumWidth(120);
        speed_ = new QComboBox(this);
        speed_->addItem(QStringLiteral("Slow (1 bar / 1 s)"), 1000);
        speed_->addItem(QStringLiteral("Normal (1 bar / 0.4 s)"), 400);
        speed_->addItem(QStringLiteral("Fast (1 bar / 0.1 s)"), 100);
        speed_->addItem(QStringLiteral("Turbo (1 bar / 20 ms)"), 20);
        speed_->setCurrentIndex(1);
        start_ = new QPushButton(QStringLiteral("Start"), this);
        start_->setObjectName(QStringLiteral("primaryButton"));
        pause_ = new QPushButton(QStringLiteral("Pause"), this);
        step_ = new QPushButton(QStringLiteral("Step"), this);
        reset_ = new QPushButton(QStringLiteral("Reset"), this);
        controls->addWidget(new QLabel(QStringLiteral("Symbol:"), this));
        controls->addWidget(symbol_);
        controls->addSpacing(12);
        controls->addWidget(new QLabel(QStringLiteral("Speed:"), this));
        controls->addWidget(speed_);
        controls->addStretch(1);
        controls->addWidget(start_);
        controls->addWidget(pause_);
        controls->addWidget(step_);
        controls->addWidget(reset_);
        root->addLayout(controls);

        // ---- status ----
        auto *statusBox = new QGroupBox(QStringLiteral("Replay status"), this);
        auto *statusLayout = new QVBoxLayout(statusBox);
        auto *statusRow = new QHBoxLayout();
        status_ = new QLabel(statusBox);
        status_->setObjectName(QStringLiteral("heroName"));
        timestamp_ = new QLabel(statusBox);
        price_ = new QLabel(statusBox);
        price_->setObjectName(QStringLiteral("heroPrice"));
        change_ = new QLabel(statusBox);
        statusRow->addWidget(status_);
        statusRow->addSpacing(20);
        statusRow->addWidget(timestamp_);
        statusRow->addStretch(1);
        statusRow->addWidget(price_);
        statusRow->addWidget(change_);
        statusLayout->addLayout(statusRow);
        progress_ = new QProgressBar(statusBox);
        progress_->setTextVisible(true);
        statusLayout->addWidget(progress_);
        events_ = new QLabel(statusBox);
        events_->setObjectName(QStringLiteral("mutedLabel"));
        events_->setWordWrap(true);
        statusLayout->addWidget(events_);
        root->addWidget(statusBox);

        // ---- chart + table ----
        table_ = new QTableWidget(0, 4, this);
        table_->setHorizontalHeaderLabels({QStringLiteral("Symbol"), QStringLiteral("Price"),
                                           QStringLiteral("Change"), QStringLiteral("Change %")});
        table_->verticalHeader()->setVisible(false);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setAlternatingRowColors(true);
        table_->setShowGrid(false);
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        chart_ = new PriceChartWidget(this);
        chart_->setEmptyText(QStringLiteral("Replay not started - press Start or Step."));

        auto *body = new QHBoxLayout();
        body->setSpacing(12);
        body->addWidget(chart_, 3);
        body->addWidget(table_, 2);
        root->addLayout(body, 1);

        auto *note = new QLabel(QStringLiteral("Replay runs in its own sandbox market and account: the live Market, Trading, Portfolio "
                                               "and Risk pages are not affected."),
                                this);
        note->setObjectName(QStringLiteral("mutedLabel"));
        root->addWidget(note);

        timer_ = new QTimer(this);
        timer_->setInterval(speed_->currentData().toInt());

        connect(timer_, &QTimer::timeout, this, &ReplayPage::onTimer);
        connect(speed_, &QComboBox::currentIndexChanged, this, [this](int)
                { onSpeedChanged(); });
        connect(selector_, &DatasetSelector::datasetChanged, this, &ReplayPage::onDatasetChanged);
        connect(start_, &QPushButton::clicked, this, &ReplayPage::startReplay);
        connect(pause_, &QPushButton::clicked, this, &ReplayPage::pauseReplay);
        connect(step_, &QPushButton::clicked, this, &ReplayPage::stepOnce);
        connect(reset_, &QPushButton::clicked, this, &ReplayPage::resetReplay);
        connect(symbol_, &QComboBox::currentTextChanged, this, [this](const QString &)
                { refreshView(); });
        connect(table_, &QTableWidget::cellClicked, this, [this](int row, int)
                {
            if (auto *item = table_->item(row, 0))
            {
                const int idx = symbol_->findText(item->text());
                if (idx >= 0)
                {
                    symbol_->setCurrentIndex(idx);
                }
            } });

        onDatasetChanged();
    }

    ReplayPage::~ReplayPage() = default;

    void ReplayPage::onDatasetChanged()
    {
        stopTimer();
        if (!selector_->hasDataset())
        {
            session_.reset();
            symbol_->clear();
            refreshView();
            return;
        }

        const QString previous = symbol_->currentText();
        symbol_->blockSignals(true);
        symbol_->clear();
        for (const std::string &s : selector_->dataset().symbols())
        {
            symbol_->addItem(QString::fromStdString(s));
        }
        const int keep = symbol_->findText(previous);
        symbol_->setCurrentIndex(keep >= 0 ? keep : 0);
        symbol_->blockSignals(false);

        lastEvents_.clear();
        rebuildSession();
    }

    void ReplayPage::rebuildSession()
    {
        stopTimer();
        session_.reset();
        if (selector_->hasDataset())
        {
            try
            {
                session_ = std::make_unique<Session>(selector_->dataset(), context_.getMarket());
            }
            catch (const std::exception &e)
            {
                lastEvents_ = QStringLiteral("Could not prepare replay: %1").arg(QString::fromUtf8(e.what()));
            }
        }
        refreshView();
    }

    void ReplayPage::stopTimer()
    {
        if (timer_ && timer_->isActive())
        {
            timer_->stop();
        }
        updateButtons();
    }

    void ReplayPage::onSpeedChanged()
    {
        timer_->setInterval(speed_->currentData().toInt());
    }

    bool ReplayPage::advance()
    {
        if (!session_ || !session_->replay.hasNext())
        {
            return false;
        }
        try
        {
            const execution::MarketUpdateResult update = session_->replay.step();
            QStringList parts;
            if (!update.triggers.empty())
            {
                parts << QStringLiteral("%1 stop order(s) triggered").arg(static_cast<qlonglong>(update.triggers.size()));
            }
            if (!update.userFills.empty())
            {
                parts << QStringLiteral("%1 fill(s)").arg(static_cast<qlonglong>(update.userFills.size()));
            }
            if (update.liquidation.marginCallDetected)
            {
                parts << QStringLiteral("margin call");
            }
            lastEvents_ = parts.isEmpty() ? QString() : parts.join(QStringLiteral("  \u00B7  "));
            return true;
        }
        catch (const std::exception &e)
        {
            lastEvents_ = QStringLiteral("Replay error: %1").arg(QString::fromUtf8(e.what()));
            stopTimer();
            return false;
        }
    }

    void ReplayPage::startReplay()
    {
        if (!session_ || !session_->replay.hasNext())
        {
            return;
        }
        timer_->start();
        updateButtons();
        refreshView();
    }

    void ReplayPage::pauseReplay()
    {
        stopTimer();
        refreshView();
    }

    void ReplayPage::stepOnce()
    {
        stopTimer();
        advance();
        refreshView();
    }

    void ReplayPage::resetReplay()
    {
        lastEvents_.clear();
        rebuildSession();
    }

    void ReplayPage::onTimer()
    {
        if (!advance() || !session_->replay.hasNext())
        {
            stopTimer();
        }
        refreshView();
    }

    void ReplayPage::updateButtons()
    {
        const bool ready = session_ != nullptr;
        const bool hasNext = ready && session_->replay.hasNext();
        const bool running = timer_ && timer_->isActive();
        start_->setEnabled(hasNext && !running);
        pause_->setEnabled(running);
        step_->setEnabled(hasNext);
        reset_->setEnabled(ready);
        symbol_->setEnabled(ready);
    }

    void ReplayPage::refreshView()
    {
        updateButtons();

        if (!session_)
        {
            status_->setText(QStringLiteral("No dataset"));
            timestamp_->setText(QStringLiteral("Build a dataset above to begin."));
            price_->setText(QStringLiteral("-"));
            change_->clear();
            progress_->setRange(0, 1);
            progress_->setValue(0);
            progress_->setFormat(QStringLiteral("-"));
            events_->setText(lastEvents_);
            table_->setRowCount(0);
            chart_->setBars({});
            return;
        }

        try
        {
            const historical::HistoricalDataSet &data = session_->data;
            const std::size_t total = session_->replay.stepCount();
            const std::size_t applied = session_->replay.stepsApplied();
            const bool running = timer_->isActive();

            QString state;
            if (applied == 0)
            {
                state = QStringLiteral("Ready");
            }
            else if (!session_->replay.hasNext())
            {
                state = QStringLiteral("Finished");
            }
            else
            {
                state = running ? QStringLiteral("Running") : QStringLiteral("Paused");
            }
            status_->setText(state);

            progress_->setRange(0, static_cast<int>(total));
            progress_->setValue(static_cast<int>(applied));
            progress_->setFormat(QStringLiteral("Step %1 / %2").arg(static_cast<qlonglong>(applied)).arg(static_cast<qlonglong>(total)));

            const std::string symbol = symbol_->currentText().toStdString();
            if (applied == 0)
            {
                timestamp_->setText(QStringLiteral("Not started"));
            }
            else if (data.hasSymbol(symbol))
            {
                const std::time_t ts = data.barAt(symbol, applied - 1).timestamp;
                timestamp_->setText(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(ts))
                                        .toString(QStringLiteral("yyyy-MM-dd HH:mm")));
            }

            events_->setText(lastEvents_);

            // ---- all symbols (from the replay's own Market) ----
            const market::Market &mkt = session_->market;
            const std::vector<std::string> symbols = data.symbols();
            table_->setRowCount(static_cast<int>(symbols.size()));
            int selectedRow = -1;
            for (std::size_t i = 0; i < symbols.size(); ++i)
            {
                const market::Quote &q = mkt.getQuote(symbols[i]);
                const QColor color = fmt::changeColor(q.change());
                const QString cells[] = {QString::fromStdString(symbols[i]), fmt::money(q.price()),
                                         fmt::signedNumber(q.change()), fmt::percent(q.percentChange())};
                for (int c = 0; c < 4; ++c)
                {
                    auto *item = new QTableWidgetItem(cells[c]);
                    if (c > 0)
                    {
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    }
                    if (c >= 2)
                    {
                        item->setForeground(color);
                    }
                    table_->setItem(static_cast<int>(i), c, item);
                }
                if (symbols[i] == symbol)
                {
                    selectedRow = static_cast<int>(i);
                }
            }
            if (selectedRow >= 0)
            {
                table_->selectRow(selectedRow);
            }

            // ---- selected symbol: current price + chart ----
            if (mkt.exists(symbol))
            {
                const market::Quote &q = mkt.getQuote(symbol);
                price_->setText(fmt::money(q.price()));
                change_->setText(QStringLiteral("%1  (%2)").arg(fmt::signedNumber(q.change()), fmt::percent(q.percentChange())));
                change_->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: bold;")
                                           .arg(fmt::changeColor(q.change()).name()));
            }

            // The chart shows the bars MarketReplay has applied so far
            // (dataset bars 0..applied-1), i.e. exactly what the replay Market
            // was fed; nothing beyond the current step is ever drawn.
            QVector<ChartBar> bars;
            if (data.hasSymbol(symbol))
            {
                bars.reserve(static_cast<int>(applied));
                for (std::size_t i = 0; i < applied; ++i)
                {
                    const historical::HistoricalBar &b = data.barAt(symbol, i);
                    bars.push_back(ChartBar{static_cast<qint64>(b.timestamp), b.open, b.high, b.low, b.close,
                                            static_cast<qlonglong>(b.volume)});
                }
            }
            chart_->setBars(bars);
        }
        catch (const std::exception &e)
        {
            qWarning("Replay refresh failed: %s", e.what());
        }
    }

} // namespace gui