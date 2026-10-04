#ifndef MARKETSIM_GUI_DATASETSELECTOR_H
#define MARKETSIM_GUI_DATASETSELECTOR_H

#include <QString>
#include <QWidget>
#include <memory>
#include "AppContext.h"
#include "../Historical/HistoricalData.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace gui
{

    // Shared "where does the historical data come from" control used by the
    // Historical Replay and Backtesting pages. It only BUILDS a
    // historical::HistoricalDataSet using the existing backend functions:
    //   - Synthetic: historical::generateSyntheticSeries() for every symbol in
    //     the live market (starting at that symbol's current price), or
    //   - CSV file: historical::loadHistoricalSeriesCsv() for one symbol.
    // It never runs a replay or a backtest itself.
    class DatasetSelector : public QWidget
    {
        Q_OBJECT

    public:
        explicit DatasetSelector(AppContext &context, QWidget *parent = nullptr);

        bool hasDataset() const { return static_cast<bool>(dataset_); }
        // Only valid if hasDataset().
        const historical::HistoricalDataSet &dataset() const { return *dataset_; }
        QString description() const { return description_; }

    public slots:
        // Builds the dataset from the current controls. On failure the previous
        // dataset is kept and the error is shown in the status line.
        void build();

    signals:
        void datasetChanged();

    private slots:
        void onSourceChanged();
        void browseCsv();

    private:
        AppContext &context_;
        QComboBox *source_ = nullptr;
        QSpinBox *bars_ = nullptr;
        QDoubleSpinBox *volatility_ = nullptr;
        QSpinBox *seed_ = nullptr;
        QLineEdit *csvPath_ = nullptr;
        QLineEdit *csvSymbol_ = nullptr;
        QPushButton *browse_ = nullptr;
        QPushButton *build_ = nullptr;
        QLabel *status_ = nullptr;
        QWidget *syntheticRow_ = nullptr;
        QWidget *csvRow_ = nullptr;

        std::unique_ptr<historical::HistoricalDataSet> dataset_;
        QString description_;
    };

} // namespace gui

#endif // MARKETSIM_GUI_DATASETSELECTOR_H