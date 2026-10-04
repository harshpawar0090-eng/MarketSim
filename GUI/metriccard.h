#ifndef MARKETSIM_GUI_METRICCARD_H
#define MARKETSIM_GUI_METRICCARD_H

#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>

namespace gui
{

    // Small read-only "title / big value / subtitle" tile used by the dashboard
    // (and reusable by later pages). Pure display widget.
    class MetricCard : public QFrame
    {
    public:
        explicit MetricCard(const QString &title, QWidget *parent = nullptr) : QFrame(parent)
        {
            setObjectName(QStringLiteral("card"));

            title_ = new QLabel(title, this);
            title_->setObjectName(QStringLiteral("cardTitle"));
            value_ = new QLabel(QStringLiteral("-"), this);
            value_->setObjectName(QStringLiteral("cardValue"));
            subtitle_ = new QLabel(this);
            subtitle_->setObjectName(QStringLiteral("cardSub"));

            auto *layout = new QVBoxLayout(this);
            layout->setContentsMargins(14, 12, 14, 12);
            layout->setSpacing(4);
            layout->addWidget(title_);
            layout->addWidget(value_);
            layout->addWidget(subtitle_);
        }

        void setTitle(const QString &title) { title_->setText(title); }

        // An invalid color keeps the default text color.
        void setValue(const QString &text, const QColor &color = QColor())
        {
            value_->setText(text);
            if (color.isValid())
            {
                value_->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
            }
            else
            {
                value_->setStyleSheet(QString());
            }
        }

        void setSubtitle(const QString &text) { subtitle_->setText(text); }

    private:
        QLabel *title_ = nullptr;
        QLabel *value_ = nullptr;
        QLabel *subtitle_ = nullptr;
    };

} // namespace gui

#endif // MARKETSIM_GUI_METRICCARD_H