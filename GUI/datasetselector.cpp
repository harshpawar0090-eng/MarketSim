#include "DatasetSelector.h"

#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <exception>
#include <stdexcept>
#include <string>

namespace gui
{

    DatasetSelector::DatasetSelector(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        auto *box = new QGroupBox(QStringLiteral("Historical dataset"), this);
        auto *layout = new QVBoxLayout(box);

        // ---- row 1: source + build ----
        auto *top = new QHBoxLayout();
        source_ = new QComboBox(box);
        source_->addItem(QStringLiteral("Synthetic (all market symbols)"));
        source_->addItem(QStringLiteral("CSV file (one symbol)"));
        build_ = new QPushButton(QStringLiteral("Build dataset"), box);
        build_->setObjectName(QStringLiteral("primaryButton"));
        top->addWidget(new QLabel(QStringLiteral("Source:"), box));
        top->addWidget(source_);
        top->addStretch(1);
        top->addWidget(build_);
        layout->addLayout(top);

        // ---- synthetic parameters ----
        syntheticRow_ = new QWidget(box);
        auto *syn = new QHBoxLayout(syntheticRow_);
        syn->setContentsMargins(0, 0, 0, 0);
        bars_ = new QSpinBox(syntheticRow_);
        bars_->setRange(20, 500);
        bars_->setValue(120);
        volatility_ = new QDoubleSpinBox(syntheticRow_);
        volatility_->setRange(0.1, 10.0);
        volatility_->setDecimals(1);
        volatility_->setSingleStep(0.5);
        volatility_->setValue(2.0);
        volatility_->setSuffix(QStringLiteral(" %"));
        seed_ = new QSpinBox(syntheticRow_);
        seed_->setRange(1, 999999);
        seed_->setValue(1);
        syn->addWidget(new QLabel(QStringLiteral("Bars (daily):"), syntheticRow_));
        syn->addWidget(bars_);
        syn->addSpacing(12);
        syn->addWidget(new QLabel(QStringLiteral("Volatility:"), syntheticRow_));
        syn->addWidget(volatility_);
        syn->addSpacing(12);
        syn->addWidget(new QLabel(QStringLiteral("Seed:"), syntheticRow_));
        syn->addWidget(seed_);
        syn->addStretch(1);
        layout->addWidget(syntheticRow_);

        // ---- CSV parameters ----
        csvRow_ = new QWidget(box);
        auto *csv = new QHBoxLayout(csvRow_);
        csv->setContentsMargins(0, 0, 0, 0);
        csvPath_ = new QLineEdit(csvRow_);
        csvPath_->setPlaceholderText(QStringLiteral("timestamp,open,high,low,close,volume CSV"));
        browse_ = new QPushButton(QStringLiteral("Browse..."), csvRow_);
        csvSymbol_ = new QLineEdit(csvRow_);
        csvSymbol_->setPlaceholderText(QStringLiteral("Symbol"));
        csvSymbol_->setMaximumWidth(100);
        csv->addWidget(new QLabel(QStringLiteral("File:"), csvRow_));
        csv->addWidget(csvPath_, 1);
        csv->addWidget(browse_);
        csv->addWidget(new QLabel(QStringLiteral("Symbol:"), csvRow_));
        csv->addWidget(csvSymbol_);
        layout->addWidget(csvRow_);

        status_ = new QLabel(box);
        status_->setObjectName(QStringLiteral("mutedLabel"));
        status_->setWordWrap(true);
        layout->addWidget(status_);

        outer->addWidget(box);

        connect(source_, &QComboBox::currentIndexChanged, this, &DatasetSelector::onSourceChanged);
        connect(browse_, &QPushButton::clicked, this, &DatasetSelector::browseCsv);
        connect(build_, &QPushButton::clicked, this, &DatasetSelector::build);

        onSourceChanged();
        build(); // start with a usable synthetic dataset so the pages are never empty
    }

    void DatasetSelector::onSourceChanged()
    {
        const bool synthetic = source_->currentIndex() == 0;
        syntheticRow_->setVisible(synthetic);
        csvRow_->setVisible(!synthetic);
    }

    void DatasetSelector::browseCsv()
    {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open historical CSV"), QString(),
                                                          QStringLiteral("CSV files (*.csv);;All files (*)"));
        if (path.isEmpty())
        {
            return;
        }
        csvPath_->setText(path);
        if (csvSymbol_->text().trimmed().isEmpty())
        {
            csvSymbol_->setText(QFileInfo(path).completeBaseName().toUpper());
        }
    }

    void DatasetSelector::build()
    {
        try
        {
            auto data = std::make_unique<historical::HistoricalDataSet>();
            QString desc;

            if (source_->currentIndex() == 0)
            {
                const int bars = bars_->value();
                const double vol = volatility_->value() / 100.0;
                const unsigned int seed = static_cast<unsigned int>(seed_->value());
                const std::time_t start = context_.getMarket().clock().now() - static_cast<std::time_t>(bars) * 86400;

                unsigned int offset = 0;
                for (const std::string &symbol : context_.getMarket().listSymbols())
                {
                    const double startPrice = context_.getMarket().getQuote(symbol).price();
                    data->addSeries(historical::generateSyntheticSeries(symbol, startPrice, bars, start, 86400, vol,
                                                                        seed + offset * 7919u));
                    ++offset;
                }
                desc = QStringLiteral("Synthetic: %1 symbols x %2 daily bars, volatility %3%, seed %4")
                           .arg(static_cast<qlonglong>(data->symbols().size()))
                           .arg(bars)
                           .arg(volatility_->value(), 0, 'f', 1)
                           .arg(seed);
            }
            else
            {
                const QString path = csvPath_->text().trimmed();
                const QString symbol = csvSymbol_->text().trimmed().toUpper();
                if (path.isEmpty() || symbol.isEmpty())
                {
                    throw std::runtime_error("Choose a CSV file and enter its symbol.");
                }
                data->addSeries(historical::loadHistoricalSeriesCsv(symbol.toStdString(), path.toStdString()));
                desc = QStringLiteral("CSV: %1 (%2 bars) from %3")
                           .arg(symbol)
                           .arg(static_cast<qlonglong>(data->stepCount()))
                           .arg(QFileInfo(path).fileName());
            }

            dataset_ = std::move(data);
            description_ = desc;
            status_->setStyleSheet(QString());
            status_->setText(desc);
            emit datasetChanged();
        }
        catch (const std::exception &e)
        {
            status_->setStyleSheet(QStringLiteral("color: #d64545;"));
            status_->setText(QStringLiteral("Could not build dataset: %1").arg(QString::fromUtf8(e.what())));
        }
    }

} // namespace gui