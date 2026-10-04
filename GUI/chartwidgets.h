#ifndef MARKETSIM_GUI_CHARTWIDGETS_H
#define MARKETSIM_GUI_CHARTWIDGETS_H

#include <QString>
#include <QVector>
#include <QWidget>

namespace gui
{

    // One bar for the chart widget. Plain data - pages fill it from
    // market::OhlcBar or historical::HistoricalBar; the widget does no
    // financial calculation, it only draws what it is given.
    struct ChartBar
    {
        qint64 time = 0; // seconds since epoch
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
        qlonglong volume = 0;
    };

    // Qt-native price chart (QPainter, no chart library): candlesticks or a
    // close-price line on top, volume bars underneath, price axis on the right,
    // time axis at the bottom and a hover crosshair with an OHLCV read-out.
    class PriceChartWidget : public QWidget
    {
        Q_OBJECT

    public:
        enum class Style
        {
            Candles,
            Line
        };

        explicit PriceChartWidget(QWidget *parent = nullptr);

        void setBars(const QVector<ChartBar> &bars);
        void setStyle(Style style);
        Style style() const { return style_; }
        void setEmptyText(const QString &text);

        QSize minimumSizeHint() const override { return QSize(320, 200); }
        QSize sizeHint() const override { return QSize(640, 320); }

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;

    private:
        QVector<ChartBar> bars_;
        Style style_ = Style::Candles;
        QString emptyText_;
        int hoverIndex_ = -1;
    };

    // Simple time-series line chart (used for the backtest equity curve), with
    // an optional dashed baseline (e.g. starting capital) and hover read-out.
    class LineChartWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit LineChartWidget(QWidget *parent = nullptr);

        void setSeries(const QVector<qint64> &times, const QVector<double> &values);
        void setBaseline(double value, const QString &label);
        void clearBaseline();
        void setEmptyText(const QString &text);

        QSize minimumSizeHint() const override { return QSize(320, 160); }
        QSize sizeHint() const override { return QSize(640, 260); }

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;

    private:
        QVector<qint64> times_;
        QVector<double> values_;
        bool hasBaseline_ = false;
        double baseline_ = 0.0;
        QString baselineLabel_;
        QString emptyText_;
        int hoverIndex_ = -1;
    };

} // namespace gui

#endif // MARKETSIM_GUI_CHARTWIDGETS_H