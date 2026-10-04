#ifndef MARKETSIM_GUI_THEME_H
#define MARKETSIM_GUI_THEME_H

#include <QString>

namespace gui
{

    // Dark trading-terminal style sheet applied once to the whole application.
    inline QString darkStyleSheet()
    {
        return QString::fromUtf8(R"(
QWidget { background-color: #0d1117; color: #c9d1d9; font-size: 13px; }
QLabel { background: transparent; }
QMainWindow, QStackedWidget { background-color: #0d1117; }

QFrame#sidebar { background-color: #010409; border-right: 1px solid #21262d; }
QLabel#appTitle { font-size: 18px; font-weight: bold; color: #f0f6fc; padding: 16px 18px 0px 18px; }
QLabel#appSubtitle { color: #6e7681; font-size: 11px; padding: 0px 18px 14px 18px; }
QLabel#sidebarFooter { color: #484f58; font-size: 11px; padding: 10px 18px; }
QListWidget#nav { background: transparent; border: none; outline: 0; }
QListWidget#nav::item { padding: 11px 18px; color: #8b949e; border-left: 3px solid transparent; }
QListWidget#nav::item:hover { background-color: #161b22; color: #c9d1d9; }
QListWidget#nav::item:selected { background-color: #161b22; color: #f0f6fc; border-left: 3px solid #2f81f7; }

QFrame#topBar { background-color: #0d1117; border-bottom: 1px solid #21262d; }
QLabel#pageTitle { font-size: 19px; font-weight: bold; color: #f0f6fc; }
QLabel#clockLabel { color: #8b949e; }

QPushButton { background-color: #21262d; border: 1px solid #30363d; border-radius: 6px; padding: 7px 14px; color: #c9d1d9; }
QPushButton:hover { background-color: #30363d; }
QPushButton:disabled { color: #484f58; }
QPushButton#primaryButton { background-color: #238636; border: 1px solid #2ea043; color: #ffffff; font-weight: bold; }
QPushButton#primaryButton:hover { background-color: #2ea043; }

QFrame#card { background-color: #161b22; border: 1px solid #21262d; border-radius: 8px; }
QLabel#cardTitle { color: #8b949e; font-size: 11px; font-weight: bold; }
QLabel#cardValue { font-size: 21px; font-weight: bold; color: #f0f6fc; }
QLabel#cardSub { color: #6e7681; font-size: 11px; }

QLabel#heroPrice { font-size: 30px; font-weight: bold; color: #f0f6fc; }
QLabel#heroName { font-size: 16px; color: #c9d1d9; }
QLabel#mutedLabel { color: #8b949e; }
QLabel#placeholderTitle { font-size: 22px; font-weight: bold; color: #f0f6fc; }
QLabel#placeholderText { color: #8b949e; font-size: 14px; }

QGroupBox { border: 1px solid #21262d; border-radius: 8px; margin-top: 14px; padding-top: 8px; background-color: #161b22; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #8b949e; font-weight: bold; }

QTableWidget { background-color: #0d1117; alternate-background-color: #11161d; border: 1px solid #21262d; gridline-color: #21262d; selection-background-color: #1f6feb; selection-color: #ffffff; }
QHeaderView::section { background-color: #161b22; color: #8b949e; padding: 6px; border: none; border-bottom: 1px solid #30363d; font-weight: bold; }

QLineEdit, QComboBox { background-color: #0d1117; border: 1px solid #30363d; border-radius: 6px; padding: 6px 8px; color: #c9d1d9; }
QLineEdit:focus, QComboBox:focus { border: 1px solid #2f81f7; }
QComboBox QAbstractItemView { background-color: #161b22; selection-background-color: #1f6feb; }

QStatusBar { background-color: #010409; color: #8b949e; border-top: 1px solid #21262d; }

QScrollBar:vertical { background: #0d1117; width: 10px; }
QScrollBar::handle:vertical { background: #30363d; border-radius: 5px; min-height: 24px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QScrollBar:horizontal { background: #0d1117; height: 10px; }
QScrollBar::handle:horizontal { background: #30363d; border-radius: 5px; min-width: 24px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
)");
    }

} // namespace gui

#endif // MARKETSIM_GUI_THEME_H