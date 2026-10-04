#include "MarketPage.h"

#include <QBrush>
#include <QDebug>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <exception>
#include <map>
#include <string>

#include "Format.h"

namespace gui
{

    namespace
    {

        // Table item that displays formatted text but sorts by a numeric value.
        class SortableItem : public QTableWidgetItem
        {
        public:
            SortableItem(const QString &text, double sortValue) : QTableWidgetItem(text)
            {
                setData(Qt::UserRole, sortValue);
            }

            bool operator<(const QTableWidgetItem &other) const override
            {
                return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
            }
        };

        QTableWidgetItem *numericItem(const QString &text, double sortValue, const QColor &color = QColor())
        {
            auto *item = new SortableItem(text, sortValue);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (color.isValid())
            {
                item->setForeground(QBrush(color));
            }
            return item;
        }

        enum Column
        {
            ColSymbol = 0,
            ColCompany,
            ColSector,
            ColPrice,
            ColChange,
            ColChangePercent,
            ColVolume,
            ColumnCount
        };

    } // namespace

    MarketPage::MarketPage(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(10);

        auto *toolbar = new QHBoxLayout();
        filter_ = new QLineEdit(this);
        filter_->setPlaceholderText(QStringLiteral("Filter by symbol, company or sector..."));
        filter_->setClearButtonEnabled(true);
        filter_->setMaximumWidth(360);
        countLabel_ = new QLabel(this);
        countLabel_->setObjectName(QStringLiteral("mutedLabel"));
        openButton_ = new QPushButton(QStringLiteral("Open Stock Detail"), this);
        openButton_->setEnabled(false);
        toolbar->addWidget(filter_);
        toolbar->addWidget(countLabel_);
        toolbar->addStretch(1);
        toolbar->addWidget(openButton_);
        root->addLayout(toolbar);

        table_ = new QTableWidget(0, ColumnCount, this);
        table_->setHorizontalHeaderLabels({QStringLiteral("Symbol"), QStringLiteral("Company"),
                                           QStringLiteral("Sector"), QStringLiteral("Price"),
                                           QStringLiteral("Change"), QStringLiteral("Change %"),
                                           QStringLiteral("Volume")});
        table_->verticalHeader()->setVisible(false);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setAlternatingRowColors(true);
        table_->setShowGrid(false);
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(ColCompany, QHeaderView::Stretch);
        table_->horizontalHeader()->setSortIndicator(ColSymbol, Qt::AscendingOrder);
        table_->setSortingEnabled(true);
        root->addWidget(table_, 1);

        connect(filter_, &QLineEdit::textChanged, this, &MarketPage::applyFilter);
        connect(table_, &QTableWidget::itemSelectionChanged, this, &MarketPage::updateOpenButton);
        connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int)
                {
            if (QTableWidgetItem *item = table_->item(row, ColSymbol))
            {
                emit stockSelected(item->text());
            } });
        connect(openButton_, &QPushButton::clicked, this, &MarketPage::openSelected);
        connect(&context_, &AppContext::stateChanged, this, &MarketPage::refresh);

        refresh();
    }

    QString MarketPage::selectedSymbol() const
    {
        const QList<QTableWidgetItem *> items = table_->selectedItems();
        if (items.isEmpty())
        {
            return QString();
        }
        QTableWidgetItem *first = table_->item(items.first()->row(), ColSymbol);
        return first != nullptr ? first->text() : QString();
    }

    void MarketPage::refresh()
    {
        try
        {
            const QString keep = selectedSymbol();

            table_->setSortingEnabled(false);
            const std::map<std::string, market::Stock> stocks = context_.getMarket().allStocks();
            table_->setRowCount(static_cast<int>(stocks.size()));

            int row = 0;
            for (const auto &entry : stocks)
            {
                const market::Stock &s = entry.second;
                const QColor changeColor = fmt::changeColor(s.change());

                table_->setItem(row, ColSymbol, new QTableWidgetItem(QString::fromStdString(s.symbol())));
                table_->setItem(row, ColCompany, new QTableWidgetItem(QString::fromStdString(s.name())));
                table_->setItem(row, ColSector,
                                new QTableWidgetItem(QString::fromStdString(market::sectorToString(s.sector()))));
                table_->setItem(row, ColPrice, numericItem(fmt::number(s.price()), s.price()));
                table_->setItem(row, ColChange,
                                numericItem(fmt::signedNumber(s.change()), s.change(), changeColor));
                table_->setItem(row, ColChangePercent,
                                numericItem(fmt::percent(s.percentChange()), s.percentChange(), changeColor));
                table_->setItem(row, ColVolume,
                                numericItem(fmt::integer(static_cast<long long>(s.volume())),
                                            static_cast<double>(s.volume())));
                ++row;
            }
            table_->setSortingEnabled(true); // re-applies the current sort indicator

            applyFilter();

            if (!keep.isEmpty())
            {
                for (int r = 0; r < table_->rowCount(); ++r)
                {
                    QTableWidgetItem *item = table_->item(r, ColSymbol);
                    if (item != nullptr && item->text() == keep)
                    {
                        table_->selectRow(r);
                        break;
                    }
                }
            }
            updateOpenButton();
        }
        catch (const std::exception &e)
        {
            qWarning("Market page refresh failed: %s", e.what());
        }
    }

    void MarketPage::applyFilter()
    {
        const QString needle = filter_->text().trimmed();
        int visible = 0;
        for (int r = 0; r < table_->rowCount(); ++r)
        {
            bool match = needle.isEmpty();
            if (!match)
            {
                for (int c : {static_cast<int>(ColSymbol), static_cast<int>(ColCompany), static_cast<int>(ColSector)})
                {
                    QTableWidgetItem *item = table_->item(r, c);
                    if (item != nullptr && item->text().contains(needle, Qt::CaseInsensitive))
                    {
                        match = true;
                        break;
                    }
                }
            }
            table_->setRowHidden(r, !match);
            if (match)
            {
                ++visible;
            }
        }
        countLabel_->setText(QStringLiteral("%1 of %2 stocks").arg(visible).arg(table_->rowCount()));
    }

    void MarketPage::updateOpenButton()
    {
        openButton_->setEnabled(!selectedSymbol().isEmpty());
    }

    void MarketPage::openSelected()
    {
        const QString symbol = selectedSymbol();
        if (!symbol.isEmpty())
        {
            emit stockSelected(symbol);
        }
    }

} // namespace gui