#include "MainWindow.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include "DashboardPage.h"
#include "MarketPage.h"
#include "PlaceholderPage.h"
#include "StockDetailPage.h"
#include "TradingPage.h"
#include "PortfolioPage.h"
#include "RiskPage.h"

namespace gui
{
MainWindow::MainWindow(AppContext &context, QWidget *parent)
    : QMainWindow(parent), context_(context)
{
    setWindowTitle(QStringLiteral("MarketSim"));
    resize(1280, 820);
    setMinimumSize(1020, 660);

    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(220);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    auto *appTitle = new QLabel(QStringLiteral("MarketSim"), sidebar);
    appTitle->setObjectName(QStringLiteral("appTitle"));
    auto *appSubtitle = new QLabel(QStringLiteral("Paper-trading terminal"), sidebar);
    appSubtitle->setObjectName(QStringLiteral("appSubtitle"));
    nav_ = new QListWidget(sidebar);
    nav_->setObjectName(QStringLiteral("nav"));
    nav_->setFocusPolicy(Qt::NoFocus);
    nav_->addItem(QStringLiteral("Dashboard"));
    nav_->addItem(QStringLiteral("Market"));
    nav_->addItem(QStringLiteral("Stock Detail"));
    nav_->addItem(QStringLiteral("Trading"));
    nav_->addItem(QStringLiteral("Portfolio"));
    nav_->addItem(QStringLiteral("Risk"));
    nav_->addItem(QStringLiteral("Backtesting"));
    auto *footer = new QLabel(QStringLiteral("Fictional stocks · simulated data"), sidebar);
    footer->setObjectName(QStringLiteral("sidebarFooter"));
    sideLayout->addWidget(appTitle);
    sideLayout->addWidget(appSubtitle);
    sideLayout->addWidget(nav_, 1);
    sideLayout->addWidget(footer);

    auto *content = new QWidget(central);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    auto *topBar = new QFrame(content);
    topBar->setObjectName(QStringLiteral("topBar"));
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 12, 20, 12);
    pageTitle_ = new QLabel(topBar);
    pageTitle_->setObjectName(QStringLiteral("pageTitle"));
    clockLabel_ = new QLabel(topBar);
    clockLabel_->setObjectName(QStringLiteral("clockLabel"));
    tickButton_ = new QPushButton(QStringLiteral("Advance Market Tick"), topBar);
    tickButton_->setObjectName(QStringLiteral("primaryButton"));
    tickButton_->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    tickButton_->setToolTip(QStringLiteral("Advance the simulated market by one tick (Ctrl+T)"));
    topLayout->addWidget(pageTitle_);
    topLayout->addStretch(1);
    topLayout->addWidget(clockLabel_);
    topLayout->addSpacing(16);
    topLayout->addWidget(tickButton_);

    stack_ = new QStackedWidget(content);
    dashboardPage_ = new DashboardPage(context_, stack_);
    marketPage_ = new MarketPage(context_, stack_);
    stockDetailPage_ = new StockDetailPage(context_, stack_);
    tradingPage_ = new TradingPage(context_, stack_);
    portfolioPage_ = new PortfolioPage(context_, stack_);
    riskPage_ = new RiskPage(context_, stack_);

    stack_->addWidget(dashboardPage_);
    stack_->addWidget(marketPage_);
    stack_->addWidget(stockDetailPage_);
    stack_->addWidget(tradingPage_);
    stack_->addWidget(portfolioPage_);
    stack_->addWidget(riskPage_);
    stack_->addWidget(new PlaceholderPage(QStringLiteral("Backtesting"),
        QStringLiteral("Historical replay and backtesting arrive in Phase 5B.3."), stack_));

    contentLayout->addWidget(topBar);
    contentLayout->addWidget(stack_, 1);
    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(content, 1);
    setCentralWidget(central);

    connect(nav_, &QListWidget::currentRowChanged, this, &MainWindow::onNavChanged);
    connect(tickButton_, &QPushButton::clicked, &context_, &AppContext::advanceTick);
    connect(marketPage_, &MarketPage::stockSelected, this, &MainWindow::showStockDetail);
    connect(&context_, &AppContext::stateChanged, this, &MainWindow::updateClock);
    connect(&context_, &AppContext::tickAdvanced, this, &MainWindow::onTickAdvanced);
    connect(tradingPage_, &TradingPage::accountChanged, dashboardPage_, &DashboardPage::refresh);
    connect(tradingPage_, &TradingPage::accountChanged, portfolioPage_, &PortfolioPage::refresh);
    connect(tradingPage_, &TradingPage::accountChanged, riskPage_, &RiskPage::refresh);

    nav_->setCurrentRow(PageDashboard);
    updateClock();
    statusBar()->showMessage(QStringLiteral("Ready. Advance the market with the button above or Ctrl+T."));
}

void MainWindow::onNavChanged(int row)
{
    if (row < 0 || row >= stack_->count()) return;
    stack_->setCurrentIndex(row);
    pageTitle_->setText(nav_->item(row)->text());
    refreshCurrentPage();
}

void MainWindow::refreshCurrentPage()
{
    switch (stack_->currentIndex())
    {
    case PageDashboard: dashboardPage_->refresh(); break;
    case PageMarket: marketPage_->refresh(); break;
    case PageStockDetail: stockDetailPage_->refresh(); break;
    case PagePortfolio: portfolioPage_->refresh(); break;
    case PageRisk: riskPage_->refresh(); break;
    default: break;
    }
}

void MainWindow::showStockDetail(const QString &symbol)
{
    stockDetailPage_->setSymbol(symbol);
    nav_->setCurrentRow(PageStockDetail);
}

void MainWindow::updateClock()
{
    const QDateTime now = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(context_.getMarket().clock().now()));
    clockLabel_->setText(QStringLiteral("Market time: %1   ·   Tick %2")
        .arg(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
        .arg(context_.tickCount()));
    refreshCurrentPage();
}

void MainWindow::onTickAdvanced(const QString &summary)
{
    statusBar()->showMessage(summary, 10000);
}
}
