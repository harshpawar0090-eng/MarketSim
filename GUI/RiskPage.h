#ifndef MARKETSIM_GUI_RISKPAGE_H
#define MARKETSIM_GUI_RISKPAGE_H

#include <QWidget>
#include "AppContext.h"

class QLabel;
class QTableWidget;

namespace gui
{
class RiskPage : public QWidget
{
    Q_OBJECT
public:
    explicit RiskPage(AppContext &context, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    AppContext &context_;
    QLabel *equity_ = nullptr;
    QLabel *buyingPower_ = nullptr;
    QLabel *grossExposure_ = nullptr;
    QLabel *leverage_ = nullptr;
    QLabel *marginUsed_ = nullptr;
    QLabel *maintenance_ = nullptr;
    QLabel *availableMargin_ = nullptr;
    QLabel *status_ = nullptr;
    QLabel *largestPosition_ = nullptr;
    QLabel *largestSector_ = nullptr;
    QTableWidget *warnings_ = nullptr;
};
}

#endif
