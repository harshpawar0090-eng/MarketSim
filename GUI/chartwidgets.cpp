#include "ChartWidgets.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <algorithm>
#include <cmath>

namespace gui
{

    namespace
    {
        const QColor kGain(46, 158, 91);
        const QColor kLoss(214, 69, 69);
        const QColor kAccent(66, 133, 244);

        QString priceText(double v)
        {
            return QLocale().toString(v, 'f', 2);
        }

        QString timeText(qint64 t, bool withClock)
        {
            return QDateTime::fromSecsSinceEpoch(t).toString(withClock ? QStringLiteral("MM-dd HH:mm")
                                                                       : QStringLiteral("yyyy-MM-dd"));
        }

        QString fullTimeText(qint64 t)
        {
            return QDateTime::fromSecsSinceEpoch(t).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }

        QColor textColor(const QWidget *w) { return w->palette().color(QPalette::WindowText); }

        QColor gridColor(const QWidget *w)
        {
            QColor c = textColor(w);
            c.setAlpha(40);
            return c;
        }

        void drawEmpty(QPainter &p, const QWidget *w, const QString &text)
        {
            p.setPen(textColor(w));
            p.drawText(w->rect(), Qt::AlignCenter, text);
        }

        // Draws a small read-out box in the top-left of the plot area.
        void drawReadout(QPainter &p, const QWidget *w, const QRectF &plot, const QStringList &lines)
        {
            QFontMetrics fm(p.font());
            int width = 0;
            for (const QString &l : lines)
            {
                width = std::max(width, fm.horizontalAdvance(l));
            }
            const int lineH = fm.height();
            QRectF box(plot.left() + 8, plot.top() + 8, width + 16, lineH * lines.size() + 10);
            QColor bg = w->palette().color(QPalette::Window);
            bg.setAlpha(225);
            p.setPen(gridColor(w));
            p.setBrush(bg);
            p.drawRoundedRect(box, 4, 4);
            p.setPen(textColor(w));
            for (int i = 0; i < lines.size(); ++i)
            {
                p.drawText(QPointF(box.left() + 8, box.top() + 5 + lineH * (i + 1) - fm.descent()), lines[i]);
            }
        }

        // Evenly spaced time labels along the bottom axis.
        void drawTimeAxis(QPainter &p, const QWidget *w, const QRectF &plot, const QVector<qint64> &times)
        {
            if (times.isEmpty())
            {
                return;
            }
            const bool withClock = (times.last() - times.first()) < 3 * 86400;
            const int n = times.size();
            const int labels = std::min(n, 5);
            QFontMetrics fm(p.font());
            p.setPen(textColor(w));
            for (int i = 0; i < labels; ++i)
            {
                const int idx = labels == 1 ? 0 : static_cast<int>(static_cast<double>(i) * (n - 1) / (labels - 1));
                const double x = plot.left() + (n == 1 ? plot.width() / 2 : plot.width() * (idx + 0.5) / n);
                const QString text = timeText(times[idx], withClock);
                const int tw = fm.horizontalAdvance(text);
                double left = std::clamp(x - tw / 2.0, plot.left(), plot.right() - tw);
                p.drawText(QPointF(left, plot.bottom() + fm.ascent() + 4), text);
            }
        }

        void drawPriceAxis(QPainter &p, const QWidget *w, const QRectF &plot, double lo, double hi)
        {
            QFontMetrics fm(p.font());
            const int ticks = 5;
            for (int i = 0; i <= ticks; ++i)
            {
                const double f = static_cast<double>(i) / ticks;
                const double y = plot.bottom() - f * plot.height();
                const double v = lo + f * (hi - lo);
                p.setPen(gridColor(w));
                p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
                p.setPen(textColor(w));
                p.drawText(QPointF(plot.right() + 6, y + fm.ascent() / 2.0 - 1), priceText(v));
            }
        }
    } // namespace

    // =====================================================================
    // PriceChartWidget
    // =====================================================================

    PriceChartWidget::PriceChartWidget(QWidget *parent)
        : QWidget(parent), emptyText_(QStringLiteral("No price history yet"))
    {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void PriceChartWidget::setBars(const QVector<ChartBar> &bars)
    {
        bars_ = bars;
        if (hoverIndex_ >= bars_.size())
        {
            hoverIndex_ = -1;
        }
        update();
    }

    void PriceChartWidget::setStyle(Style style)
    {
        style_ = style;
        update();
    }

    void PriceChartWidget::setEmptyText(const QString &text)
    {
        emptyText_ = text;
        update();
    }

    void PriceChartWidget::leaveEvent(QEvent *)
    {
        hoverIndex_ = -1;
        update();
    }

    void PriceChartWidget::mouseMoveEvent(QMouseEvent *event)
    {
        if (bars_.isEmpty())
        {
            return;
        }
        const double plotLeft = 8;
        const double plotRight = width() - 64;
        const double slot = (plotRight - plotLeft) / bars_.size();
        int idx = static_cast<int>((event->position().x() - plotLeft) / slot);
        idx = std::clamp(idx, 0, static_cast<int>(bars_.size()) - 1);
        if (idx != hoverIndex_)
        {
            hoverIndex_ = idx;
            update();
        }
    }

    void PriceChartWidget::paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), palette().color(QPalette::Base));

        if (bars_.isEmpty())
        {
            drawEmpty(p, this, emptyText_);
            return;
        }

        const int n = bars_.size();
        const double left = 8, right = width() - 64, top = 8, bottom = height() - 24;
        if (right - left < 40 || bottom - top < 60)
        {
            return;
        }
        const double volumeH = (bottom - top) * 0.22;
        const double gap = 6;
        const QRectF pricePlot(left, top, right - left, (bottom - top) - volumeH - gap);
        const QRectF volPlot(left, pricePlot.bottom() + gap, right - left, volumeH);
        const QRectF fullPlot(left, top, right - left, bottom - top);

        double lo = bars_[0].low, hi = bars_[0].high;
        qlonglong maxVol = 1;
        QVector<qint64> times;
        times.reserve(n);
        for (const ChartBar &b : bars_)
        {
            lo = std::min(lo, std::min(b.low, b.close));
            hi = std::max(hi, std::max(b.high, b.close));
            maxVol = std::max(maxVol, b.volume);
            times.push_back(b.time);
        }
        if (hi - lo < 1e-9)
        {
            hi += 1.0;
            lo -= 1.0;
        }
        const double pad = (hi - lo) * 0.06;
        lo -= pad;
        hi += pad;

        auto xAt = [&](int i)
        { return pricePlot.left() + pricePlot.width() * (i + 0.5) / n; };
        auto yAt = [&](double v)
        { return pricePlot.bottom() - (v - lo) / (hi - lo) * pricePlot.height(); };

        drawPriceAxis(p, this, pricePlot, lo, hi);
        const double slot = pricePlot.width() / n;

        // ---- volume bars ----
        for (int i = 0; i < n; ++i)
        {
            const ChartBar &b = bars_[i];
            QColor c = b.close >= b.open ? kGain : kLoss;
            c.setAlpha(120);
            const double h = volPlot.height() * static_cast<double>(b.volume) / maxVol;
            const double w = std::max(1.0, slot * 0.7);
            p.fillRect(QRectF(xAt(i) - w / 2, volPlot.bottom() - h, w, h), c);
        }
        p.setPen(textColor(this));
        p.drawText(QPointF(volPlot.right() + 6, volPlot.top() + 10), QStringLiteral("Vol"));

        // ---- price ----
        if (style_ == Style::Candles)
        {
            for (int i = 0; i < n; ++i)
            {
                const ChartBar &b = bars_[i];
                const QColor c = b.close >= b.open ? kGain : kLoss;
                const double x = xAt(i);
                p.setPen(QPen(c, 1));
                p.drawLine(QPointF(x, yAt(b.high)), QPointF(x, yAt(b.low)));
                const double w = std::max(1.0, slot * 0.7);
                const double yTop = yAt(std::max(b.open, b.close));
                const double yBot = yAt(std::min(b.open, b.close));
                p.setBrush(c);
                p.drawRect(QRectF(x - w / 2, yTop, w, std::max(1.0, yBot - yTop)));
            }
        }
        else
        {
            const QColor c = bars_.last().close >= bars_.first().close ? kGain : kLoss;
            QPainterPath path;
            for (int i = 0; i < n; ++i)
            {
                const QPointF pt(xAt(i), yAt(bars_[i].close));
                if (i == 0)
                {
                    path.moveTo(pt);
                }
                else
                {
                    path.lineTo(pt);
                }
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(c, 2));
            p.drawPath(path);
        }

        // ---- last price marker ----
        {
            const double y = yAt(bars_.last().close);
            QPen dash(kAccent, 1, Qt::DashLine);
            p.setPen(dash);
            p.drawLine(QPointF(pricePlot.left(), y), QPointF(pricePlot.right(), y));
            const QString text = priceText(bars_.last().close);
            QFontMetrics fm(p.font());
            QRectF tag(pricePlot.right() + 2, y - fm.height() / 2.0 - 1, 60, fm.height() + 2);
            p.setPen(Qt::NoPen);
            p.setBrush(kAccent);
            p.drawRoundedRect(tag, 3, 3);
            p.setPen(Qt::white);
            p.drawText(tag, Qt::AlignCenter, text);
        }

        drawTimeAxis(p, this, fullPlot, times);

        // ---- hover crosshair + read-out ----
        if (hoverIndex_ >= 0 && hoverIndex_ < n)
        {
            const ChartBar &b = bars_[hoverIndex_];
            const double x = xAt(hoverIndex_);
            p.setPen(QPen(textColor(this), 1, Qt::DotLine));
            p.drawLine(QPointF(x, top), QPointF(x, bottom));
            drawReadout(p, this, pricePlot,
                        {fullTimeText(b.time),
                         QStringLiteral("O %1   H %2").arg(priceText(b.open), priceText(b.high)),
                         QStringLiteral("L %1   C %2").arg(priceText(b.low), priceText(b.close)),
                         QStringLiteral("Vol %1").arg(QLocale().toString(b.volume))});
        }
    }

    // =====================================================================
    // LineChartWidget
    // =====================================================================

    LineChartWidget::LineChartWidget(QWidget *parent)
        : QWidget(parent), emptyText_(QStringLiteral("No data yet"))
    {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void LineChartWidget::setSeries(const QVector<qint64> &times, const QVector<double> &values)
    {
        const int n = std::min(times.size(), values.size());
        times_ = times.mid(0, n);
        values_ = values.mid(0, n);
        hoverIndex_ = -1;
        update();
    }

    void LineChartWidget::setBaseline(double value, const QString &label)
    {
        hasBaseline_ = true;
        baseline_ = value;
        baselineLabel_ = label;
        update();
    }

    void LineChartWidget::clearBaseline()
    {
        hasBaseline_ = false;
        update();
    }

    void LineChartWidget::setEmptyText(const QString &text)
    {
        emptyText_ = text;
        update();
    }

    void LineChartWidget::leaveEvent(QEvent *)
    {
        hoverIndex_ = -1;
        update();
    }

    void LineChartWidget::mouseMoveEvent(QMouseEvent *event)
    {
        if (values_.isEmpty())
        {
            return;
        }
        const double plotLeft = 8;
        const double plotRight = width() - 80;
        const int n = values_.size();
        int idx = n == 1 ? 0 : static_cast<int>(std::round((event->position().x() - plotLeft) / (plotRight - plotLeft) * (n - 1)));
        idx = std::clamp(idx, 0, n - 1);
        if (idx != hoverIndex_)
        {
            hoverIndex_ = idx;
            update();
        }
    }

    void LineChartWidget::paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), palette().color(QPalette::Base));

        if (values_.isEmpty())
        {
            drawEmpty(p, this, emptyText_);
            return;
        }

        const int n = values_.size();
        const QRectF plot(8, 8, width() - 88, height() - 32);
        if (plot.width() < 40 || plot.height() < 40)
        {
            return;
        }

        double lo = *std::min_element(values_.begin(), values_.end());
        double hi = *std::max_element(values_.begin(), values_.end());
        if (hasBaseline_)
        {
            lo = std::min(lo, baseline_);
            hi = std::max(hi, baseline_);
        }
        if (hi - lo < 1e-9)
        {
            hi += 1.0;
            lo -= 1.0;
        }
        const double pad = (hi - lo) * 0.08;
        lo -= pad;
        hi += pad;

        auto xAt = [&](int i)
        { return n == 1 ? plot.center().x() : plot.left() + plot.width() * i / (n - 1); };
        auto yAt = [&](double v)
        { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };

        drawPriceAxis(p, this, plot, lo, hi);

        if (hasBaseline_)
        {
            const double y = yAt(baseline_);
            p.setPen(QPen(textColor(this), 1, Qt::DashLine));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            p.drawText(QPointF(plot.left() + 4, y - 4), baselineLabel_);
        }

        const QColor c = values_.last() >= (hasBaseline_ ? baseline_ : values_.first()) ? kGain : kLoss;
        QPainterPath path;
        for (int i = 0; i < n; ++i)
        {
            const QPointF pt(xAt(i), yAt(values_[i]));
            if (i == 0)
            {
                path.moveTo(pt);
            }
            else
            {
                path.lineTo(pt);
            }
        }
        QPainterPath area = path;
        area.lineTo(QPointF(xAt(n - 1), plot.bottom()));
        area.lineTo(QPointF(xAt(0), plot.bottom()));
        area.closeSubpath();
        QColor fill = c;
        fill.setAlpha(28);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(area);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c, 2));
        p.drawPath(path);

        // time axis: reuse the shared helper with a plot rect that maps labels to the same x range
        {
            const bool withClock = (times_.last() - times_.first()) < 3 * 86400;
            QFontMetrics fm(p.font());
            p.setPen(textColor(this));
            const int labels = std::min(n, 5);
            for (int i = 0; i < labels; ++i)
            {
                const int idx = labels == 1 ? 0 : static_cast<int>(static_cast<double>(i) * (n - 1) / (labels - 1));
                const QString text = timeText(times_[idx], withClock);
                const int tw = fm.horizontalAdvance(text);
                const double x = std::clamp(xAt(idx) - tw / 2.0, plot.left(), plot.right() - tw);
                p.drawText(QPointF(x, plot.bottom() + fm.ascent() + 4), text);
            }
        }

        if (hoverIndex_ >= 0 && hoverIndex_ < n)
        {
            const double x = xAt(hoverIndex_);
            p.setPen(QPen(textColor(this), 1, Qt::DotLine));
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(x, yAt(values_[hoverIndex_])), 3.5, 3.5);
            drawReadout(p, this, plot,
                        {fullTimeText(times_[hoverIndex_]),
                         QStringLiteral("Value %1").arg(priceText(values_[hoverIndex_]))});
        }
    }

} // namespace gui