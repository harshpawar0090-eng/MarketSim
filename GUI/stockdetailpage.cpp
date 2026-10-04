#include "StockDetailPage.h"

#include <QBrush>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <deque>
#include <exception>
#include <string>

#include "Format.h"

namespace gui
{

    QLabel *StockDetailPage::addRow(QFormLayout *form, const QString &label)
    {
        auto *value = new QLabel(QStringLiteral("-"), this);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(label, value);
        return value;
    }

    StockDetailPage::StockDetailPage(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(12);

        // ---- symbol selector ----
        auto *selector = new QHBoxLayout();
        auto *selectorLabel = new QLabel(QStringLiteral("Stock:"), this);
        selectorLabel->setObjectName(QStringLiteral("mutedLabel"));
        combo_ = new QComboBox(this);
        combo_->setMinimumWidth(140);
        selector->addWidget(selectorLabel);
        selector->addWidget(combo_);
        selector->addStretch(1);
        root->addLayout(selector);

        // ---- hero header ----
        heroName_ = new QLabel(this);
        heroName_->setObjectName(QStringLiteral("heroName"));
        heroPrice_ = new QLabel(this);
        heroPrice_->setObjectName(QStringLiteral("heroPrice"));
        heroChange_ = new QLabel(this);
        auto *hero = new QHBoxLayout();
        hero->setSpacing(18);
        hero->addWidget(heroPrice_);
        hero->addWidget(heroChange_);
        hero->addStretch(1);
        root->addWidget(heroName_);
        root->addLayout(hero);

        // ---- quote box ----
        auto *quoteBox = new QGroupBox(QStringLiteral("Quote"), this);
        auto *quoteForm = new QFormLayout(quoteBox);
        quoteForm->setLabelAlignment(Qt::AlignLeft);
        symbolValue_ = addRow(quoteForm, QStringLiteral("Symbol"));
        companyValue_ = addRow(quoteForm, QStringLiteral("Company"));
        sectorValue_ = addRow(quoteForm, QStringLiteral("Sector"));
        priceValue_ = addRow(quoteForm, QStringLiteral("Current price"));
        previousValue_ = addRow(quoteForm, QStringLiteral("Previous price"));
        changeValue_ = addRow(quoteForm, QStringLiteral("Change"));
        changePercentValue_ = addRow(quoteForm, QStringLiteral("Change %"));
        bidValue_ = addRow(quoteForm, QStringLiteral("Bid"));
        askValue_ = addRow(quoteForm, QStringLiteral("Ask"));
        volumeValue_ = addRow(quoteForm, QStringLiteral("Volume"));
        turnoverValue_ = addRow(quoteForm, QStringLiteral("Turnover"));
        executedVolumeValue_ = addRow(quoteForm, QStringLiteral("Executed volume (real fills)"));
        executedTurnoverValue_ = addRow(quoteForm, QStringLiteral("Executed turnover (real fills)"));

        // ---- history box ----
        auto *historyBox = new QGroupBox(QStringLiteral("Price history (recorded bars)"), this);
        auto *historyLayout = new QVBoxLayout(historyBox);
        auto *summaryForm = new QFormLayout();
        highValue_ = addRow(summaryForm, QStringLiteral("Highest recorded"));
        lowValue_ = addRow(summaryForm, QStringLiteral("Lowest recorded"));
        barsValue_ = addRow(summaryForm, QStringLiteral("Bars recorded"));
        historyLayout->addLayout(summaryForm);

        history_ = new QTableWidget(0, 6, this);
        history_->setHorizontalHeaderLabels({QStringLiteral("Time"), QStringLiteral("Open"),
                                             QStringLiteral("High"), QStringLiteral("Low"),
                                             QStringLiteral("Close"), QStringLiteral("Volume")});
        history_->verticalHeader()->setVisible(false);
        history_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        history_->setSelectionMode(QAbstractItemView::NoSelection);
        history_->setAlternatingRowColors(true);
        history_->setShowGrid(false);
        history_->setFocusPolicy(Qt::NoFocus);
        history_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        historyLayout->addWidget(history_, 1);

        auto *columns = new QHBoxLayout();
        columns->setSpacing(12);
        columns->addWidget(quoteBox, 1);
        columns->addWidget(historyBox, 2);
        root->addLayout(columns, 1);

        // ---- populate the selector from the real instruments ----
        combo_->blockSignals(true);
        for (const std::string &symbol : context_.getMarket().listSymbols())
        {
            combo_->addItem(QString::fromStdString(symbol));
        }
        combo_->blockSignals(false);

        connect(combo_, &QComboBox::currentTextChanged, this, [this](const QString &)
                { refresh(); });
        connect(&context_, &AppContext::stateChanged, this, &StockDetailPage::refresh);

        refresh();
    }

    void StockDetailPage::setSymbol(const QString &symbol)
    {
        const int index = combo_->findText(symbol);
        if (index >= 0)
        {
            combo_->setCurrentIndex(index); // triggers refresh() if the symbol changed
            refresh();
        }
    }

    void StockDetailPage::refresh()
    {
        try
        {
            const std::string symbol = combo_->currentText().toStdString();
            const market::Market &mkt = context_.getMarket();
            if (symbol.empty() || !mkt.exists(symbol))
            {
                return;
            }

            const market::Instrument &instrument = mkt.getInstrument(symbol);
            const market::Quote &quote = mkt.getQuote(symbol);
            const QColor color = fmt::changeColor(quote.change());
            const QString colorStyle = QStringLiteral("color: %1;").arg(color.name());

            heroName_->setText(QStringLiteral("%1  \u2014  %2")
                                   .arg(QString::fromStdString(instrument.symbol()),
                                        QString::fromStdString(instrument.name())));
            heroPrice_->setText(fmt::money(quote.price()));
            heroChange_->setText(QStringLiteral("%1  (%2)")
                                     .arg(fmt::signedNumber(quote.change()), fmt::percent(quote.percentChange())));
            heroChange_->setStyleSheet(colorStyle + QStringLiteral(" font-size: 16px; font-weight: bold;"));

            symbolValue_->setText(QString::fromStdString(instrument.symbol()));
            companyValue_->setText(QString::fromStdString(instrument.name()));
            sectorValue_->setText(QString::fromStdString(market::sectorToString(instrument.sector())));
            priceValue_->setText(fmt::money(quote.price()));
            previousValue_->setText(fmt::money(quote.previousPrice()));
            changeValue_->setText(fmt::signedNumber(quote.change()));
            changeValue_->setStyleSheet(colorStyle);
            changePercentValue_->setText(fmt::percent(quote.percentChange()));
            changePercentValue_->setStyleSheet(colorStyle);
            bidValue_->setText(fmt::money(quote.bidPrice()));
            askValue_->setText(fmt::money(quote.askPrice()));
            volumeValue_->setText(fmt::integer(static_cast<long long>(quote.volume())));
            turnoverValue_->setText(fmt::money(quote.turnover()));
            executedVolumeValue_->setText(fmt::integer(static_cast<long long>(mkt.executedVolume(symbol))));
            executedTurnoverValue_->setText(fmt::money(mkt.executedTurnover(symbol)));

            highValue_->setText(fmt::money(mkt.highestPrice(symbol)));
            lowValue_->setText(fmt::money(mkt.lowestPrice(symbol)));

            const std::deque<market::OhlcBar> &bars = mkt.priceHistory(symbol);
            barsValue_->setText(fmt::integer(static_cast<long long>(bars.size())));

            // Most recent 10 bars, newest first.
            const std::size_t shown = std::min<std::size_t>(bars.size(), 10);
            history_->setRowCount(static_cast<int>(shown));
            for (std::size_t i = 0; i < shown; ++i)
            {
                const market::OhlcBar &bar = bars[bars.size() - 1 - i];
                const int row = static_cast<int>(i);
                const QString time = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(bar.timestamp))
                                         .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

                const QString cells[] = {time,
                                         fmt::number(bar.open),
                                         fmt::number(bar.high),
                                         fmt::number(bar.low),
                                         fmt::number(bar.close),
                                         fmt::integer(static_cast<long long>(bar.volume))};
                for (int c = 0; c < 6; ++c)
                {
                    auto *item = new QTableWidgetItem(cells[c]);
                    if (c > 0)
                    {
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    }
                    history_->setItem(row, c, item);
                }
            }
        }
        catch (const std::exception &e)
        {
            qWarning("Stock detail refresh failed: %s", e.what());
        }
    }

} // namespace gui