#ifndef MARKETSIM_GUI_REPLAYPAGE_H
#define MARKETSIM_GUI_REPLAYPAGE_H

#include <QWidget>
#include <memory>
#include "AppContext.h"

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;

namespace gui
{
    class DatasetSelector;
    class PriceChartWidget;

    // Historical Replay page. Drives the EXISTING historical::MarketReplay over
    // a historical::HistoricalDataSet. The replay runs on its own sandbox
    // (a dedicated Market + Portfolio + ExecutionService + Broker, exactly like
    // BacktestEngine builds), so replaying never touches the live account or
    // the live market. A QTimer only decides WHEN MarketReplay::step() is called.
    class ReplayPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit ReplayPage(AppContext &context, QWidget *parent = nullptr);
        ~ReplayPage() override;

    private slots:
        void onDatasetChanged();
        void startReplay();
        void pauseReplay();
        void stepOnce();
        void resetReplay();
        void onTimer();
        void onSpeedChanged();
        void refreshView();

    private:
        struct Session; // dedicated Market/Portfolio/Execution/Broker/MarketReplay

        void rebuildSession();
        void stopTimer();
        void updateButtons();
        bool advance(); // one MarketReplay::step(); returns false if nothing was left

        AppContext &context_;
        DatasetSelector *selector_ = nullptr;
        QComboBox *symbol_ = nullptr;
        QComboBox *speed_ = nullptr;
        QPushButton *start_ = nullptr;
        QPushButton *pause_ = nullptr;
        QPushButton *step_ = nullptr;
        QPushButton *reset_ = nullptr;
        QLabel *status_ = nullptr;
        QLabel *timestamp_ = nullptr;
        QLabel *price_ = nullptr;
        QLabel *change_ = nullptr;
        QLabel *events_ = nullptr;
        QProgressBar *progress_ = nullptr;
        QTableWidget *table_ = nullptr;
        PriceChartWidget *chart_ = nullptr;
        QTimer *timer_ = nullptr;

        std::unique_ptr<Session> session_;
        QString lastEvents_;
    };

} // namespace gui

#endif // MARKETSIM_GUI_REPLAYPAGE_H