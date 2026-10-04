#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <exception>

#include "AppContext.h"
#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char *argv[])
{
    QApplication qtApp(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MarketSim"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    qtApp.setStyleSheet(gui::darkStyleSheet());

    try
    {
        // One backend for the whole process; declared before the window so it outlives it.
        gui::AppContext context;
        gui::MainWindow window(context);
        window.show();
        return qtApp.exec();
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(nullptr, QStringLiteral("MarketSim"),
                              QStringLiteral("MarketSim failed to start:\n%1").arg(QString::fromUtf8(e.what())));
        return 1;
    }
}