#include "ChartsPage.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>
#include <deque>
#include <exception>
#include <string>

#include "ChartWidgets.h"
#include "Format.h"

namespace gui
{

    ChartsPage::ChartsPage(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(12);

        auto *controls = new QHBoxLayout();
        symbol_ = new QComboBox(this);
        symbol_->setMinimumWidth(140);
        style_ = new QComboBox(this);
        style_->addItem(QStringLiteral("Candlesticks"));
        style_->addItem(QStringLiteral("Line"));
        range_ = new QComboBox(this);
        range_->addItem(QStringLiteral("Last 30 bars"), 30);
        range_->addItem(QStringLiteral("Last 60 bars"), 60);
        range_->addItem(QStringLiteral("Last 120 bars"), 120);
        range_->addItem(QStringLiteral("All recorded bars"), 0);
        range_->setCurrentIndex(3);
        controls->addWidget(new QLabel(QStringLiteral("Stock:"), this));
        controls->addWidget(symbol_);
        controls->addSpacing(12);
        controls->addWidget(new QLabel(QStringLiteral("Style:"), this));
        controls->addWidget(style_);
        controls->addSpacing(12);
        controls->addWidget(new QLabel(QStringLiteral("Range:"), this));
        controls->addWidget(range_);
        controls->addStretch(1);
        root->addLayout(controls);

        title_ = new QLabel(this);
        title_->setObjectName(QStringLiteral("heroName"));
        summary_ = new QLabel(this);
        summary_->setObjectName(QStringLiteral("mutedLabel"));
        root->addWidget(title_);
        root->addWidget(summary_);

        chart_ = new PriceChartWidget(this);
        chart_->setEmptyText(QStringLiteral("No price history yet - advance the market to record bars."));
        root->addWidget(chart_, 1);

        auto *hint = new QLabel(QStringLiteral("Live market history: one bar per market tick (the most recent 500 bars are kept). "
                                               "Hover the chart for OHLC and volume."),
                                this);
        hint->setObjectName(QStringLiteral("mutedLabel"));
        root->addWidget(hint);

        symbol_->blockSignals(true);
        for (const std::string &s : context_.getMarket().listSymbols())
        {
            symbol_->addItem(QString::fromStdString(s));
        }
        symbol_->blockSignals(false);

        connect(symbol_, &QComboBox::currentTextChanged, this, [this](const QString &)
                { refresh(); });
        connect(range_, &QComboBox::currentIndexChanged, this, [this](int)
                { refresh(); });
        connect(style_, &QComboBox::currentIndexChanged, this, [this](int)
                { refresh(); });
        connect(&context_, &AppContext::stateChanged, this, &ChartsPage::refresh);

        refresh();
    }

    void ChartsPage::setSymbol(const QString &symbol)
    {
        const int index = symbol_->findText(symbol);
        if (index >= 0)
        {
            symbol_->setCurrentIndex(index);
            refresh();
        }
    }

    void ChartsPage::refresh()
    {
        try
        {
            const std::string symbol = symbol_->currentText().toStdString();
            const market::Market &mkt = context_.getMarket();
            if (symbol.empty() || !mkt.exists(symbol))
            {
                return;
            }

            const std::deque<market::OhlcBar> &all = mkt.priceHistory(symbol);
            const int limit = range_->currentData().toInt();
            const std::size_t count = limit > 0 ? std::min<std::size_t>(all.size(), static_cast<std::size_t>(limit)) : all.size();

            QVector<ChartBar> bars;
            bars.reserve(static_cast<int>(count));
            for (std::size_t i = all.size() - count; i < all.size(); ++i)
            {
                const market::OhlcBar &b = all[i];
                bars.push_back(ChartBar{static_cast<qint64>(b.timestamp), b.open, b.high, b.low, b.close,
                                        static_cast<qlonglong>(b.volume)});
            }
            chart_->setStyle(style_->currentIndex() == 0 ? PriceChartWidget::Style::Candles
                                                         : PriceChartWidget::Style::Line);
            chart_->setBars(bars);

            const market::Instrument &instrument = mkt.getInstrument(symbol);
            title_->setText(QStringLiteral("%1  \u2014  %2")
                                .arg(QString::fromStdString(instrument.symbol()),
                                     QString::fromStdString(instrument.name())));

            if (bars.isEmpty())
            {
                summary_->setText(QStringLiteral("No bars recorded yet."));
            }
            else
            {
                const double first = bars.first().open;
                const double last = bars.last().close;
                const double pct = first != 0.0 ? (last - first) / first * 100.0 : 0.0;
                summary_->setText(QStringLiteral("%1 bars  \u00B7  Last %2  \u00B7  Range change %3  \u00B7  High %4  \u00B7  Low %5")
                                      .arg(bars.size())
                                      .arg(fmt::money(last), fmt::percent(pct), fmt::money(mkt.highestPrice(symbol)),
                                           fmt::money(mkt.lowestPrice(symbol))));
            }
        }
        catch (const std::exception &e)
        {
            qWarning("Charts refresh failed: %s", e.what());
        }
    }

} // namespace gui