#ifndef MARKETSIM_GUI_MARKETPAGE_H
#define MARKETSIM_GUI_MARKETPAGE_H

#include <QString>
#include <QWidget>
#include "AppContext.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace gui
{

    // Market Explorer: one row per instrument, read straight from Market.
    class MarketPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit MarketPage(AppContext &context, QWidget *parent = nullptr);

        // Symbol of the selected row, or an empty string.
        QString selectedSymbol() const;

    public slots:
        void refresh();

    signals:
        // Double-click on a row, or the "Open Stock Detail" button.
        void stockSelected(const QString &symbol);

    private slots:
        void applyFilter();
        void updateOpenButton();
        void openSelected();

    private:
        AppContext &context_;
        QLineEdit *filter_ = nullptr;
        QLabel *countLabel_ = nullptr;
        QPushButton *openButton_ = nullptr;
        QTableWidget *table_ = nullptr;
    };

} // namespace gui

#endif // MARKETSIM_GUI_MARKETPAGE_H