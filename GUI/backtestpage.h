#ifndef MARKETSIM_GUI_BACKTESTPAGE_H
#define MARKETSIM_GUI_BACKTESTPAGE_H

#include <QWidget>
#include "AppContext.h"

class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace gui
{
    class DatasetSelector;
    class LineChartWidget;

    // Backtesting page. Runs the EXISTING backtest::Strategy implementations
    // through backtest::BacktestEngine. BacktestEngine builds its own Market,
    // Portfolio, ExecutionService and Broker, so a backtest never touches the
    // live account. This page only collects parameters and displays the
    // returned backtest::BacktestResult.
    class BacktestPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit BacktestPage(AppContext &context, QWidget *parent = nullptr);

    private slots:
        void onDatasetChanged();
        void onStrategyChanged();
        void runBacktest();

    private:
        QLabel *addMetric(QGridLayout *grid, int row, int column, const QString &title);
        void clearResults(const QString &message);

        AppContext &context_;
        DatasetSelector *selector_ = nullptr;

        QComboBox *strategy_ = nullptr;
        QComboBox *symbol_ = nullptr;
        QSpinBox *quantity_ = nullptr;
        QSpinBox *shortWindow_ = nullptr;
        QSpinBox *longWindow_ = nullptr;
        QDoubleSpinBox *startingCash_ = nullptr;
        QDoubleSpinBox *commission_ = nullptr;
        QDoubleSpinBox *slippage_ = nullptr;
        QPushButton *run_ = nullptr;
        QLabel *message_ = nullptr;

        QLabel *strategyName_ = nullptr;
        QLabel *startingEquity_ = nullptr;
        QLabel *endingEquity_ = nullptr;
        QLabel *strategyReturn_ = nullptr;
        QLabel *benchmarkReturn_ = nullptr;
        QLabel *excessReturn_ = nullptr;
        QLabel *trades_ = nullptr;
        QLabel *winLoss_ = nullptr;
        QLabel *winRate_ = nullptr;
        QLabel *maxDrawdown_ = nullptr;
        QLabel *costs_ = nullptr;

        LineChartWidget *equityChart_ = nullptr;
        QTableWidget *tradeTable_ = nullptr;
    };

} // namespace gui

#endif // MARKETSIM_GUI_BACKTESTPAGE_H