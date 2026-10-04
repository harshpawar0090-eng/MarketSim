#ifndef MARKETSIM_GUI_CHARTSPAGE_H
#define MARKETSIM_GUI_CHARTSPAGE_H

#include <QString>
#include <QWidget>
#include "AppContext.h"

class QComboBox;
class QLabel;

namespace gui
{
    class PriceChartWidget;

    // Full-size price chart for any live-market symbol, drawn from the
    // existing Market::priceHistory() (OHLC + volume per tick).
    class ChartsPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit ChartsPage(AppContext &context, QWidget *parent = nullptr);

    public slots:
        void setSymbol(const QString &symbol);
        void refresh();

    private:
        AppContext &context_;
        QComboBox *symbol_ = nullptr;
        QComboBox *style_ = nullptr;
        QComboBox *range_ = nullptr;
        QLabel *title_ = nullptr;
        QLabel *summary_ = nullptr;
        PriceChartWidget *chart_ = nullptr;
    };

} // namespace gui

#endif // MARKETSIM_GUI_CHARTSPAGE_H