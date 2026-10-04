#ifndef MARKETSIM_GUI_STOCKDETAILPAGE_H
#define MARKETSIM_GUI_STOCKDETAILPAGE_H

#include <QString>
#include <QWidget>
#include "AppContext.h"
#include <QFormLayout>

class QComboBox;
class QLabel;
class QTableWidget;

namespace gui
{
    class PriceChartWidget;

    // Read-only detail view for one instrument: quote, bid/ask, volume,
    // turnover, a price/volume chart and the recorded OHLC history table.
    class StockDetailPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit StockDetailPage(AppContext &context, QWidget *parent = nullptr);

    public slots:
        // No-op if the symbol is unknown.
        void setSymbol(const QString &symbol);
        void refresh();

    private:
        QLabel *addRow(QFormLayout *form, const QString &label);

        AppContext &context_;
        QComboBox *combo_ = nullptr;
        QComboBox *chartStyle_ = nullptr;
        PriceChartWidget *chart_ = nullptr;

        QLabel *heroName_ = nullptr;
        QLabel *heroPrice_ = nullptr;
        QLabel *heroChange_ = nullptr;

        QLabel *symbolValue_ = nullptr;
        QLabel *companyValue_ = nullptr;
        QLabel *sectorValue_ = nullptr;
        QLabel *priceValue_ = nullptr;
        QLabel *previousValue_ = nullptr;
        QLabel *changeValue_ = nullptr;
        QLabel *changePercentValue_ = nullptr;
        QLabel *bidValue_ = nullptr;
        QLabel *askValue_ = nullptr;
        QLabel *volumeValue_ = nullptr;
        QLabel *turnoverValue_ = nullptr;
        QLabel *executedVolumeValue_ = nullptr;
        QLabel *executedTurnoverValue_ = nullptr;

        QLabel *highValue_ = nullptr;
        QLabel *lowValue_ = nullptr;
        QLabel *barsValue_ = nullptr;
        QTableWidget *history_ = nullptr;
    };

} // namespace gui

#endif // MARKETSIM_GUI_STOCKDETAILPAGE_H