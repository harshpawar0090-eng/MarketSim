#include "TradingPage.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <optional>
#include <stdexcept>

#include "../Broker/Broker.h"
#include "../Execution/Order.h"
#include "../Market/Market.h"

namespace gui
{

    namespace
    {

        QString money(double v)
        {
            return QStringLiteral("$%1").arg(v, 0, 'f', 2);
        }

        QString sideText(execution::OrderSide s)
        {
            return s == execution::OrderSide::Buy
                       ? QStringLiteral("BUY")
                       : QStringLiteral("SELL");
        }

        QString statusText(execution::OrderStatus s)
        {
            return QString::fromStdString(execution::orderStatusToString(s));
        }

        QString typeText(execution::OrderType t)
        {
            switch (t)
            {
            case execution::OrderType::Market:
                return QStringLiteral("MARKET");

            case execution::OrderType::Limit:
                return QStringLiteral("LIMIT");

            case execution::OrderType::Stop:
                return QStringLiteral("STOP");

            case execution::OrderType::StopLoss:
                return QStringLiteral("STOP-LOSS");

            case execution::OrderType::TakeProfit:
                return QStringLiteral("TAKE-PROFIT");

            case execution::OrderType::StopLimit:
                return QStringLiteral("STOP-LIMIT");
            }

            return QStringLiteral("UNKNOWN");
        }

        void setupTable(QTableWidget *t, const QStringList &headers)
        {
            t->setColumnCount(headers.size());
            t->setHorizontalHeaderLabels(headers);
            t->verticalHeader()->setVisible(false);
            t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
            t->horizontalHeader()->setStretchLastSection(true);
            t->setEditTriggers(QAbstractItemView::NoEditTriggers);
            t->setAlternatingRowColors(true);
            t->setShowGrid(false);
        }

        void cell(QTableWidget *t, int r, int c, const QString &s)
        {
            auto *item = new QTableWidgetItem(s);
            t->setItem(r, c, item);
        }

    } // namespace

    TradingPage::TradingPage(AppContext &context, QWidget *parent)
        : QWidget(parent), context_(context)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 18, 20, 20);
        root->setSpacing(12);

        auto *entry = new QGroupBox(QStringLiteral("Order Entry"), this);
        auto *form = new QGridLayout(entry);

        symbol_ = new QComboBox(entry);

        side_ = new QComboBox(entry);
        side_->addItems({QStringLiteral("BUY"),
                         QStringLiteral("SELL")});

        type_ = new QComboBox(entry);
        type_->addItems({QStringLiteral("MARKET"),
                         QStringLiteral("LIMIT"),
                         QStringLiteral("STOP"),
                         QStringLiteral("STOP-LOSS"),
                         QStringLiteral("TAKE-PROFIT"),
                         QStringLiteral("STOP-LIMIT")});

        quantity_ = new QSpinBox(entry);
        quantity_->setRange(1, 500000);
        quantity_->setValue(10);

        limitPrice_ = new QDoubleSpinBox(entry);
        limitPrice_->setRange(0.01, 100000000.0);
        limitPrice_->setDecimals(2);
        limitPrice_->setSingleStep(0.01);

        stopPrice_ = new QDoubleSpinBox(entry);
        stopPrice_->setRange(0.01, 100000000.0);
        stopPrice_->setDecimals(2);
        stopPrice_->setSingleStep(0.01);

        placeButton_ = new QPushButton(QStringLiteral("Place Order"), entry);
        placeButton_->setObjectName(QStringLiteral("primaryButton"));

        quote_ = new QLabel(entry);
        quote_->setObjectName(QStringLiteral("cardTitle"));

        feedback_ = new QLabel(entry);
        feedback_->setWordWrap(true);

        form->addWidget(new QLabel(QStringLiteral("Symbol"), entry), 0, 0);
        form->addWidget(symbol_, 0, 1);

        form->addWidget(new QLabel(QStringLiteral("Side"), entry), 0, 2);
        form->addWidget(side_, 0, 3);

        form->addWidget(new QLabel(QStringLiteral("Order Type"), entry), 0, 4);
        form->addWidget(type_, 0, 5);

        form->addWidget(new QLabel(QStringLiteral("Quantity"), entry), 1, 0);
        form->addWidget(quantity_, 1, 1);

        form->addWidget(new QLabel(QStringLiteral("Limit Price"), entry), 1, 2);
        form->addWidget(limitPrice_, 1, 3);

        form->addWidget(new QLabel(QStringLiteral("Stop Price"), entry), 1, 4);
        form->addWidget(stopPrice_, 1, 5);

        form->addWidget(quote_, 2, 0, 1, 4);
        form->addWidget(placeButton_, 2, 4, 1, 2);

        form->addWidget(feedback_, 3, 0, 1, 6);

        root->addWidget(entry);

        auto *middle = new QHBoxLayout();

        auto *bidBox = new QGroupBox(QStringLiteral("Bids"), this);
        auto *bidLayout = new QVBoxLayout(bidBox);

        bids_ = new QTableWidget(0, 3, bidBox);

        setupTable(
            bids_,
            {QStringLiteral("Price"),
             QStringLiteral("Qty"),
             QStringLiteral("Orders")});

        bidLayout->addWidget(bids_);
        middle->addWidget(bidBox, 1);

        auto *askBox = new QGroupBox(QStringLiteral("Asks"), this);
        auto *askLayout = new QVBoxLayout(askBox);

        asks_ = new QTableWidget(0, 3, askBox);

        setupTable(
            asks_,
            {QStringLiteral("Price"),
             QStringLiteral("Qty"),
             QStringLiteral("Orders")});

        askLayout->addWidget(asks_);
        middle->addWidget(askBox, 1);

        root->addLayout(middle, 1);

        auto *ordersBox =
            new QGroupBox(QStringLiteral("Open / Pending Orders"), this);

        auto *ordersLayout = new QVBoxLayout(ordersBox);

        orders_ = new QTableWidget(0, 9, ordersBox);

        setupTable(
            orders_,
            {QStringLiteral("ID"),
             QStringLiteral("Symbol"),
             QStringLiteral("Side"),
             QStringLiteral("Type"),
             QStringLiteral("Qty"),
             QStringLiteral("Filled"),
             QStringLiteral("Limit"),
             QStringLiteral("Stop"),
             QStringLiteral("Status")});

        orders_->setSelectionBehavior(QAbstractItemView::SelectRows);
        orders_->setSelectionMode(QAbstractItemView::SingleSelection);

        ordersLayout->addWidget(orders_);

        auto *cancel =
            new QPushButton(QStringLiteral("Cancel Selected Order"), ordersBox);

        ordersLayout->addWidget(cancel, 0, Qt::AlignRight);

        root->addWidget(ordersBox, 2);

        connect(
            type_,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TradingPage::onOrderTypeChanged);

        connect(
            symbol_,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TradingPage::onSymbolChanged);

        connect(
            placeButton_,
            &QPushButton::clicked,
            this,
            &TradingPage::placeOrder);

        connect(
            cancel,
            &QPushButton::clicked,
            this,
            &TradingPage::cancelSelectedOrder);

        connect(
            &context_,
            &AppContext::stateChanged,
            this,
            &TradingPage::refresh);

        rebuildSymbols();
        updatePriceFields();
        refresh();
    }

    void TradingPage::rebuildSymbols()
    {
        const QString previous = selectedSymbol();

        symbol_->clear();

        for (const auto &s : context_.getMarket().listSymbols())
        {
            symbol_->addItem(QString::fromStdString(s));
        }

        const int idx = symbol_->findText(previous);

        if (idx >= 0)
        {
            symbol_->setCurrentIndex(idx);
        }
    }

    QString TradingPage::selectedSymbol() const
    {
        return symbol_ ? symbol_->currentText() : QString();
    }

    void TradingPage::onOrderTypeChanged(int)
    {
        updatePriceFields();
    }

    void TradingPage::onSymbolChanged(int)
    {
        updatePriceFields();
        refreshOrderBook();
    }

    void TradingPage::updatePriceFields()
    {
        const int type = type_->currentIndex();

        const bool limit =
            type == 1 || type == 5;

        const bool stop =
            type >= 2;

        limitPrice_->setEnabled(limit);
        stopPrice_->setEnabled(stop);

        if (
            !selectedSymbol().isEmpty() &&
            context_.getMarket().exists(
                selectedSymbol().toStdString()))
        {
            const auto q =
                context_.getMarket().getQuote(
                    selectedSymbol().toStdString());

            quote_->setText(
                QStringLiteral(
                    "Last %1   ·   Bid %2   ·   Ask %3   ·   Spread %4")
                    .arg(money(q.price()))
                    .arg(money(q.bidPrice()))
                    .arg(money(q.askPrice()))
                    .arg(money(q.askPrice() - q.bidPrice())));

            if (
                limit &&
                limitPrice_->value() == 0.01)
            {
                limitPrice_->setValue(q.price());
            }

            if (
                stop &&
                stopPrice_->value() == 0.01)
            {
                stopPrice_->setValue(q.price());
            }
        }
    }

    void TradingPage::refreshOrderBook()
    {
        bids_->setRowCount(0);
        asks_->setRowCount(0);

        const auto depth =
            context_.getBroker().marketDepth(
                selectedSymbol().toStdString(),
                10);

        if (!depth)
        {
            return;
        }

        for (const auto &level : depth->bids)
        {
            const int r = bids_->rowCount();

            bids_->insertRow(r);

            cell(
                bids_,
                r,
                0,
                money(level.price));

            cell(
                bids_,
                r,
                1,
                QString::number(level.quantity));

            cell(
                bids_,
                r,
                2,
                QString::number(level.orderCount));
        }

        for (const auto &level : depth->asks)
        {
            const int r = asks_->rowCount();

            asks_->insertRow(r);

            cell(
                asks_,
                r,
                0,
                money(level.price));

            cell(
                asks_,
                r,
                1,
                QString::number(level.quantity));

            cell(
                asks_,
                r,
                2,
                QString::number(level.orderCount));
        }
    }

    void TradingPage::refreshOrders()
    {
        orders_->setRowCount(0);

        const auto open =
            context_.getBroker().openOrders();

        for (const auto &o : open)
        {
            const int r = orders_->rowCount();

            orders_->insertRow(r);

            cell(
                orders_,
                r,
                0,
                QString::number(
                    static_cast<qulonglong>(o.id())));

            cell(
                orders_,
                r,
                1,
                QString::fromStdString(o.symbol()));

            cell(
                orders_,
                r,
                2,
                sideText(o.side()));

            cell(
                orders_,
                r,
                3,
                typeText(o.type()));

            cell(
                orders_,
                r,
                4,
                QString::number(o.quantity()));

            cell(
                orders_,
                r,
                5,
                QString::number(o.filledQuantity()));

            cell(
                orders_,
                r,
                6,
                o.limitPrice() > 0.0
                    ? money(o.limitPrice())
                    : QStringLiteral("—"));

            cell(
                orders_,
                r,
                7,
                o.stopPrice() > 0.0
                    ? money(o.stopPrice())
                    : QStringLiteral("—"));

            cell(
                orders_,
                r,
                8,
                statusText(o.status()));
        }
    }

    void TradingPage::refresh()
    {
        rebuildSymbols();
        updatePriceFields();
        refreshOrderBook();
        refreshOrders();
    }

    void TradingPage::placeOrder()
    {
        try
        {
            const std::string symbol =
                selectedSymbol().toStdString();

            const int quantity =
                quantity_->value();

            const int type =
                type_->currentIndex();

            const execution::OrderSide side =
                side_->currentIndex() == 0
                    ? execution::OrderSide::Buy
                    : execution::OrderSide::Sell;

            execution::OrderReceipt receipt =
                [&]() -> execution::OrderReceipt
            {
                switch (type)
                {
                case 0:
                    return context_.getBroker().placeMarketOrder(
                        side,
                        symbol,
                        quantity);

                case 1:
                    return context_.getBroker().placeLimitOrder(
                        side,
                        symbol,
                        quantity,
                        limitPrice_->value());

                case 2:
                    return context_.getBroker().placeStopOrder(
                        side,
                        symbol,
                        quantity,
                        stopPrice_->value());

                case 3:
                    return context_.getBroker().placeStopLossOrder(
                        symbol,
                        quantity,
                        stopPrice_->value());

                case 4:
                    return context_.getBroker().placeTakeProfitOrder(
                        symbol,
                        quantity,
                        stopPrice_->value());

                case 5:
                    return context_.getBroker().placeStopLimitOrder(
                        side,
                        symbol,
                        quantity,
                        stopPrice_->value(),
                        limitPrice_->value());

                default:
                    throw std::runtime_error(
                        "Unsupported order type");
                }
            }();

            QString message =
                QStringLiteral(
                    "Order #%1: %2 %3 %4")
                    .arg(
                        static_cast<qulonglong>(
                            receipt.order.id()))
                    .arg(
                        sideText(receipt.order.side()))
                    .arg(
                        typeText(receipt.order.type()))
                    .arg(
                        statusText(receipt.order.status()));

            if (receipt.filledQuantity() > 0)
            {
                message +=
                    QStringLiteral(
                        " · filled %1 @ %2")
                        .arg(
                            receipt.filledQuantity())
                        .arg(
                            money(
                                receipt.averagePrice()));
            }
            else if (receipt.order.isPending())
            {
                message +=
                    QStringLiteral(
                        " · waiting for trigger");
            }

            feedback_->setText(message);

            feedback_->setStyleSheet(
                QStringLiteral(
                    "color: #35c759;"));

            refresh();

            emit accountChanged();
        }
        catch (const std::exception &e)
        {
            feedback_->setText(
                QStringLiteral(
                    "Order rejected: %1")
                    .arg(
                        QString::fromUtf8(e.what())));

            feedback_->setStyleSheet(
                QStringLiteral(
                    "color: #ff453a;"));
        }
    }

    void TradingPage::cancelSelectedOrder()
    {
        const auto ranges =
            orders_->selectedRanges();

        if (ranges.isEmpty())
        {
            feedback_->setText(
                QStringLiteral(
                    "Select an open order first."));

            return;
        }

        const int row =
            ranges.first().topRow();

        auto *idItem =
            orders_->item(row, 0);

        if (!idItem)
        {
            return;
        }

        bool ok = false;

        const qulonglong id =
            idItem->text().toULongLong(&ok);

        if (!ok)
        {
            return;
        }

        if (
            context_.getBroker().cancelOrder(
                static_cast<execution::OrderId>(id)))
        {
            feedback_->setText(
                QStringLiteral(
                    "Order #%1 cancelled.")
                    .arg(id));

            feedback_->setStyleSheet(
                QStringLiteral(
                    "color: #35c759;"));

            refresh();

            emit accountChanged();
        }
        else
        {
            feedback_->setText(
                QStringLiteral(
                    "Order could not be cancelled."));

            feedback_->setStyleSheet(
                QStringLiteral(
                    "color: #ff453a;"));
        }
    }

} // namespace gui