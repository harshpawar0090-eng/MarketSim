#ifndef MARKETSIM_GUI_FORMAT_H
#define MARKETSIM_GUI_FORMAT_H

#include <QColor>
#include <QLocale>
#include <QString>
#include <cmath>

// Display formatting only. No financial logic lives here.
namespace gui
{
    namespace fmt
    {

        inline const QLocale &locale()
        {
            static const QLocale loc(QStringLiteral("en_US"));
            return loc;
        }

        inline double roundCents(double v) { return std::round(v * 100.0) / 100.0; }

        // 1,234.56
        inline QString number(double v, int decimals = 2)
        {
            return locale().toString(v, 'f', decimals);
        }

        // +1,234.56 / -1,234.56 / 0.00
        inline QString signedNumber(double v)
        {
            const double r = roundCents(v);
            if (r > 0.0)
            {
                return QStringLiteral("+") + number(r);
            }
            return number(r == 0.0 ? 0.0 : r);
        }

        inline QString integer(long long v)
        {
            return locale().toString(static_cast<qlonglong>(v));
        }

        // $1,234.56 / -$1,234.56
        inline QString money(double v)
        {
            const double r = roundCents(v);
            const QString body = number(std::fabs(r));
            return (r < 0.0) ? QStringLiteral("-$") + body : QStringLiteral("$") + body;
        }

        // +$12.34 / -$12.34 / $0.00
        inline QString signedMoney(double v)
        {
            const double r = roundCents(v);
            const QString body = number(std::fabs(r));
            if (r > 0.0)
            {
                return QStringLiteral("+$") + body;
            }
            if (r < 0.0)
            {
                return QStringLiteral("-$") + body;
            }
            return QStringLiteral("$") + body;
        }

        // +1.23% / -1.23% / 0.00%
        inline QString percent(double v)
        {
            return signedNumber(v) + QStringLiteral("%");
        }

        inline QString plainPercent(double v)
        {
            return number(v) + QStringLiteral("%");
        }

        // 1.50x, or the infinity sign when equity <= 0.
        inline QString leverage(double v)
        {
            if (std::isnan(v))
            {
                return QStringLiteral("n/a");
            }
            if (std::isinf(v))
            {
                return QStringLiteral("\u221E");
            }
            return number(v) + QStringLiteral("x");
        }

        inline QColor gainColor() { return QColor(QStringLiteral("#3fb950")); }
        inline QColor lossColor() { return QColor(QStringLiteral("#f85149")); }
        inline QColor neutralColor() { return QColor(QStringLiteral("#8b949e")); }
        inline QColor warnColor() { return QColor(QStringLiteral("#d29922")); }

        inline QColor changeColor(double v)
        {
            if (v > 0.0)
            {
                return gainColor();
            }
            if (v < 0.0)
            {
                return lossColor();
            }
            return neutralColor();
        }

    } // namespace fmt
} // namespace gui

#endif // MARKETSIM_GUI_FORMAT_H