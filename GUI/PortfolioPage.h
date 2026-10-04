#ifndef MARKETSIM_GUI_PORTFOLIOPAGE_H
#define MARKETSIM_GUI_PORTFOLIOPAGE_H

#include <QWidget>
#include "AppContext.h"

class QLabel;
class QTableWidget;

namespace gui
{
class PortfolioPage : public QWidget
{
    Q_OBJECT
public:
    explicit PortfolioPage(AppContext &context, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    AppContext &context_;
    QLabel *equity_ = nullptr;
    QLabel *cash_ = nullptr;
    QLabel *pnl_ = nullptr;
    QLabel *return_ = nullptr;
    QLabel *exposure_ = nullptr;
    QLabel *positions_ = nullptr;
    QTableWidget *positionsTable_ = nullptr;
    QTableWidget *allocationTable_ = nullptr;
    QTableWidget *largestTable_ = nullptr;
};
}

#endif
