#ifndef MARKETSIM_GUI_UIHELPERS_H
#define MARKETSIM_GUI_UIHELPERS_H

#include <QBrush>
#include <QColor>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>

// Tiny header-only helpers shared by the GUI pages so every table shows
// "nothing here yet" the same way. No business logic.
namespace gui
{
    namespace ui
    {

        // Removes all rows and any column span left over from an empty-state row.
        inline void resetRows(QTableWidget *table)
        {
            table->clearSpans();
            table->setRowCount(0);
        }

        // Shows one muted, non-selectable, full-width message row. Call only when
        // the table has no real rows.
        inline void showEmptyRow(QTableWidget *table, const QString &message)
        {
            table->clearSpans();
            table->setRowCount(1);
            auto *item = new QTableWidgetItem(message);
            item->setFlags(Qt::NoItemFlags);
            item->setForeground(QBrush(QColor(QStringLiteral("#9aa4b2"))));
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(0, 0, item);
            if (table->columnCount() > 1)
            {
                table->setSpan(0, 0, 1, table->columnCount());
            }
        }

    } // namespace ui
} // namespace gui

#endif // MARKETSIM_GUI_UIHELPERS_H
