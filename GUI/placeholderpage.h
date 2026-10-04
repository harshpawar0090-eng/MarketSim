#ifndef MARKETSIM_GUI_PLACEHOLDERPAGE_H
#define MARKETSIM_GUI_PLACEHOLDERPAGE_H

#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace gui
{

    // "Coming in a later phase" page for navigation entries that are not built yet.
    class PlaceholderPage : public QWidget
    {
    public:
        PlaceholderPage(const QString &title, const QString &message, QWidget *parent = nullptr)
            : QWidget(parent)
        {
            auto *layout = new QVBoxLayout(this);
            layout->setAlignment(Qt::AlignCenter);
            layout->setSpacing(8);

            auto *titleLabel = new QLabel(title, this);
            titleLabel->setObjectName(QStringLiteral("placeholderTitle"));
            titleLabel->setAlignment(Qt::AlignCenter);

            auto *textLabel = new QLabel(message, this);
            textLabel->setObjectName(QStringLiteral("placeholderText"));
            textLabel->setAlignment(Qt::AlignCenter);
            textLabel->setWordWrap(true);

            layout->addWidget(titleLabel);
            layout->addWidget(textLabel);
        }
    };

} // namespace gui

#endif // MARKETSIM_GUI_PLACEHOLDERPAGE_H