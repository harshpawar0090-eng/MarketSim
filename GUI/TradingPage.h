#ifndef MARKETSIM_GUI_TRADINGPAGE_H
#define MARKETSIM_GUI_TRADINGPAGE_H

#include <QWidget>
#include "AppContext.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QPushButton;
class QTableWidget;

namespace gui
{
class TradingPage : public QWidget
{
    Q_OBJECT
public:
    explicit TradingPage(AppContext &context, QWidget *parent = nullptr);

public slots:
    void refresh();

signals:
    void accountChanged();

private slots:
    void onOrderTypeChanged(int index);
    void onSymbolChanged(int index);
    void placeOrder();
    void cancelSelectedOrder();

private:
    void rebuildSymbols();
    void updatePriceFields();
    void refreshOrderBook();
    void refreshOrders();
    QString selectedSymbol() const;
    bool readPositiveQuantity(int &quantity);
    bool readPositivePrice(QDoubleSpinBox *box, double &value);

    AppContext &context_;
    QComboBox *symbol_ = nullptr;
    QComboBox *side_ = nullptr;
    QComboBox *type_ = nullptr;
    QSpinBox *quantity_ = nullptr;
    QDoubleSpinBox *limitPrice_ = nullptr;
    QDoubleSpinBox *stopPrice_ = nullptr;
    QPushButton *placeButton_ = nullptr;
    QLabel *quote_ = nullptr;
    QLabel *feedback_ = nullptr;
    QTableWidget *orders_ = nullptr;
    QTableWidget *bids_ = nullptr;
    QTableWidget *asks_ = nullptr;
    QTableWidget *selectedOrderTable_ = nullptr;
};
}

#endif
