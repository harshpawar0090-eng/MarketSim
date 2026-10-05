# MarketSim

**MarketSim** is a C++17 educational stock-market and paper-trading simulator. It models a fictional equity market, simulates price movement, accepts and matches trading orders, manages a virtual portfolio, applies risk and margin rules, replays historical data, runs backtests, and provides both a console interface and an optional Qt 6 desktop GUI.

> **Important:** MarketSim uses fictional/simulated market data. It is an educational software project and is **not** connected to a real stock exchange or live market-data provider.

---

## What the Project Does

MarketSim is built as a layered trading system rather than a single monolithic program.

```text
                    +----------------------+
                    |   Console UI / GUI   |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |       Broker          |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |     RiskManager       |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |  ExecutionService    |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |  MatchingEngine       |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |      OrderBook        |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |      Fills/Trades    |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |      Portfolio        |
                    +----------------------+
```

Market simulation, historical replay, backtesting, analytics, and portfolio intelligence use the same core market/trading architecture where appropriate.

---

## Main Features

### Simulated Market

- 46 fictional stocks across 9 sectors
- Instrument and quote separation
- Simulated market clock
- Dynamic price engine
- Price changes and percentage changes
- Bid/ask prices
- Volume and turnover
- Bounded OHLC price history
- Highest/lowest historical price queries
- **MarketSim 50** representative market index
- Market gainers/losers and market statistics

### Trading and Order Execution

Supported order types:

- Market
- Limit
- Stop
- Stop-Loss
- Take-Profit
- Stop-Limit

The execution layer includes:

- Order IDs and order lifecycle
- Pending trigger orders
- Order cancellation
- Partial fills
- Per-symbol order books
- Bid/ask sides
- Price-time priority
- FIFO ordering within a price level
- Best bid/ask and market depth
- Multi-level execution
- Self-trade prevention
- Simulated market-maker liquidity
- Fill/trade history

Order flow:

```text
Order
  -> OrderBook
  -> MatchingEngine
  -> Fill / Trade
  -> Portfolio
```

### Broker Layer

The `Broker` acts as the client-facing trading interface.

It provides:

- Order placement
- Order cancellation
- Open/pending order access
- Order-book depth
- Buying-power information
- Account-risk information
- Risk configuration
- Market-update processing
- Margin enforcement

The Broker deliberately does **not** contain matching, order-book, settlement, or risk logic.

### Portfolio

The virtual portfolio supports:

- Cash balance
- Long positions
- Short positions
- Signed position quantities
- Average entry price
- Holdings and market value
- Realized P&L
- Unrealized P&L
- Total portfolio value/equity
- Transaction history

A positive signed quantity represents a **long** position; a negative signed quantity represents a **short** position.

### Risk and Margin

MarketSim includes a simplified educational margin model.

Configurable controls include:

- Buying power
- Maximum order quantity
- Maximum position quantity
- Maximum gross exposure
- Short selling on/off
- Initial margin rate
- Maintenance margin rate
- Liquidation buffer
- Automatic liquidation

The default console configuration uses:

- Virtual starting cash: **$500,000**
- Short selling: **enabled**
- Initial margin: **50%** (2x leverage)
- Maintenance margin: **25%**
- Automatic liquidation: **enabled**
- Maximum order quantity: **5,000 shares**
- Maximum position quantity: **20,000 shares per symbol**
- Maximum gross exposure: **$1,000,000**

> These are simplified simulator rules, not exchange or brokerage rules.

### What-If Portfolio Simulation

The `simulation` module lets the user test hypothetical scenarios without changing the real market or portfolio.

Included scenarios:

- Bull market
- Bear market
- Custom symbol-by-symbol price changes

The result compares the real portfolio with the hypothetical scenario.

```text
Real Portfolio
      |
      v
Hypothetical Scenario
      |
      v
Baseline vs Hypothetical
      |
      v
Simulation Result
```

The simulation module uses `const` access to the real market and portfolio so the hypothetical calculation cannot mutate the live state through its public interface.

### Historical Data and Replay

The `historical` module provides:

- Historical OHLCV bars
- Per-symbol historical series
- Multi-symbol historical datasets
- CSV loading/saving support
- Synthetic historical-series generation
- Step-by-step market replay
- Multi-step replay

During replay, historical bars are applied to the market and market-update processing can trigger normal broker/execution behavior.

### Backtesting

The `backtest` module runs trading strategies over historical data using the same execution architecture used by live paper trading.

Included strategies:

- **Buy & Hold**
- **Moving Average Crossover**

Backtests produce:

- Starting equity
- Ending equity
- Total return
- Optional benchmark return
- Number of trades
- Winning/losing trades
- Win rate
- Maximum drawdown
- Transaction costs
- Trade log
- Equity curve

A `MarketView` exposes only historical data available up to the current replay step, preventing future bars from being used by the strategy and structurally reducing look-ahead bias.

### Portfolio Intelligence

The `analytics` module provides higher-level portfolio analysis, including:

- Portfolio overview
- Position analytics
- Long/short exposure
- Sector allocation
- Concentration analysis
- Risk metrics
- Performance statistics
- Winning/losing trade statistics
- Drawdown analysis
- Portfolio snapshots and equity history
- Risk warnings

---

## User Interfaces

### Console Application

The original command-line interface is still available.

Main menu includes:

```text
1.  View Market
2.  View Portfolio
3.  Place Order (Market/Limit/Stop/Stop-Loss/Take-Profit/Stop-Limit)
4.  View Open / Pending Orders
5.  Cancel Order
6.  View Order Book
7.  Transaction History
8.  Portfolio Analysis
9.  Run What-If Simulation
10. Advance Market Tick
11. Market Index
12. Market Statistics
13. Price History (OHLC)
14. Risk & Margin
15. Exit
```

### Qt 6 Desktop GUI

The repository also contains an optional Qt 6 desktop interface.

Current navigation includes:

- Dashboard
- Market
- Stock Detail
- Trading
- Portfolio
- Risk
- Charts
- Historical Replay
- Backtesting

The GUI uses the same backend core library as the console application.

```text
                 +-------------------+
                 |    marketsim      |
                 |     console       |
                 +---------+---------+
                           |
                           v
                 +-------------------+
                 |  marketsim_core    |
                 +---------+---------+
                           ^
                           |
                 +---------+---------+
                 |   marketsim_gui    |
                 |       Qt 6         |
                 +-------------------+
```

The GUI is optional: CMake skips the GUI target when Qt 6 Widgets is not found, while the console application can still be built.

---

## Project Structure

```text
MarketSim/
│
├── main.cpp
├── cmakelists.txt
├── README.md
├── CURRENT_STATE.md
├── PROJECT_CONTEXT.md
├── PHASE_HISTORY.md
├── .gitignore
│
├── app/
│   ├── marketseed.cpp
│   └── marketseed.h
│
├── market/
│   ├── market.cpp
│   ├── market.h
│   ├── Marketindex.cpp
│   ├── Marketindex.h
│   ├── marketstatistics.cpp
│   ├── Marketstatistics.h
│   ├── priceengine.cpp
│   └── priceengine.h
│
├── trading/
│   ├── trading.cpp
│   └── trading.h
│
├── execution/
│   ├── order.cpp
│   ├── order.h
│   ├── orderbook.cpp
│   ├── orderbook.h
│   ├── matchingengine.cpp
│   ├── matchingengine.h
│   ├── executionservice.cpp
│   └── executionservice.h
│
├── broker/
│   ├── broker.cpp
│   └── broker.h
│
├── portfolio/
│   ├── portfolio.cpp
│   └── portfolio.h
│
├── risk/
│   ├── riskmanager.cpp
│   └── riskmanager.h
│
├── analytics/
│   ├── PortfolioIntelligence.cpp
│   └── PortfolioIntelligence.h
│
├── simulation/
│   ├── simulation.cpp
│   └── simulation.h
│
├── historical/
│   ├── historicaldata.cpp
│   ├── historicaldata.h
│   ├── marketreplay.cpp
│   └── marketreplay.h
│
├── backtest/
│   ├── backtestengine.cpp
│   ├── backtestengine.h
│   ├── strategy.cpp
│   └── strategy.h
│
├── UI/
│   ├── menu.cpp
│   └── menu.h
│
├── GUI/
│   ├── MainWindow.*
│   ├── DashboardPage.*
│   ├── MarketPage.*
│   ├── StockDetailPage.*
│   ├── TradingPage.*
│   ├── PortfolioPage.*
│   ├── RiskPage.*
│   ├── ChartsPage.*
│   ├── ReplayPage.*
│   ├── BacktestPage.*
│   ├── ChartWidgets.*
│   ├── AppContext.*
│   └── supporting UI helpers/themes
│
└── TESTS/
    ├── Phase3tests.cpp
    ├── phase3integrationtests.cpp
    ├── phase3stoptests.cpp
    ├── phase4tests.cpp
    └── phase5portfoliotests.cpp
```

> The repository may also contain local build output such as `build/` and compiled executables on development machines. These are build artifacts and should not be committed to GitHub.

---

## Module Responsibilities

| Module | Responsibility |
|---|---|
| `market/` | Instruments, quotes, simulated clock, price engine, OHLC history, market index, market statistics |
| `trading/` | Legacy/simple trading abstractions retained by the project |
| `execution/` | Orders, order book, matching engine, execution and settlement flow |
| `broker/` | Client-facing trading facade and read-only market/risk views |
| `portfolio/` | Cash, positions, transactions, P&L and portfolio accounting |
| `risk/` | Pre-trade risk checks, buying power, margin and liquidation |
| `analytics/` | Portfolio intelligence, concentration, exposure, performance and risk analytics |
| `simulation/` | Read-only What-If scenarios |
| `historical/` | OHLCV datasets, CSV persistence and market replay |
| `backtest/` | Historical strategy execution and backtest metrics |
| `app/` | Shared application setup and fictional market seeding |
| `UI/` | Console input/output and menu handling |
| `GUI/` | Optional Qt 6 desktop interface |
| `TESTS/` | Phase and integration tests |

---

## OOP Concepts Demonstrated

MarketSim is designed as an OOP-focused academic project and demonstrates:

- Classes and objects
- Encapsulation
- Abstraction
- Information hiding
- Constructors and member initializer lists
- Composition
- Inheritance
- Abstract base classes
- Virtual functions
- Method overriding
- Runtime polymorphism
- Dynamic binding
- Object interaction
- STL containers
- References and `const` correctness
- RAII
- Exception handling
- Modular architecture
- Separation of responsibilities

Examples of polymorphic hierarchies include:

```text
Order
├── BuyOrder
└── SellOrder
```

and

```text
Scenario
├── BullScenario
├── BearScenario
└── CustomScenario
```

Strategies also use an abstract `Strategy` interface with concrete implementations such as Buy & Hold and Moving Average Crossover.

---

## Technologies

- **C++:** C++17
- **Build system:** CMake
- **Core dependencies:** C++ Standard Library
- **GUI:** Qt 6 Widgets (optional)
- **Data:** Fictional/simulated market data plus historical CSV/synthetic datasets

### External APIs

**No external market-data API is required.**

The project does not depend on an API key, live stock feed, exchange connection, database, or networking service. The market is generated inside the application by the project's own simulation code.

---

## Building

### Recommended: CMake

From the project root:

```bash
cmake -S . -B build
cmake --build build
```

On a Windows Visual Studio generator, a Release build can be created with:

```powershell
cmake --build build --config Release
```

The console target is:

```text
marketsim
```

and, when Qt 6 Widgets is available, the GUI target is:

```text
marketsim_gui
```

### Qt 6

To build the GUI, CMake must be able to find Qt 6 Widgets. When Qt 6 is unavailable, CMake prints a warning and skips the GUI target; the console application remains buildable.

> **Note:** the repository currently uses the filename `cmakelists.txt`. On case-sensitive systems, CMake expects the conventional `CMakeLists.txt` spelling, so rename the file before configuring there if necessary.

---

## Running

### Windows

After a successful build, run the console program from the directory containing the executable, for example:

```powershell
.\marketsim.exe
```

For the Qt interface:

```powershell
.\marketsim_gui.exe
```

### Linux/macOS

Use the corresponding executable produced by your CMake generator:

```bash
./marketsim
```

and, when Qt 6 is enabled:

```bash
./marketsim_gui
```

---

## Testing

The repository contains focused C++ test programs covering the major development phases, including:

- Order validation and lifecycle behavior
- Price-time priority and matching
- Limit and market order behavior
- Cancellation
- Self-trade prevention
- Stop and trigger orders
- Execution-service integration
- Historical data validation
- CSV persistence
- Synthetic historical-series generation
- Historical market replay
- Portfolio intelligence
- Risk, margin, long/short positions, P&L and analytics

Test source files are located in:

```text
TESTS/
```

The project does not currently define dedicated CTest targets, so these test programs are built separately when running the test suite.

---

## Market Simulation Model

The project intentionally uses a **simulated** price engine rather than a live-data API.

A simplified tick looks like:

```text
Market Tick
    |
    +--> Advance simulated clock
    |
    +--> Generate price movement
    |
    +--> Update quotes
    |
    +--> Update volume/turnover
    |
    +--> Record OHLC bar
    |
    +--> Re-publish execution liquidity / process triggers
    |
    +--> Check risk and margin
```

This keeps the project deterministic in architecture while still allowing market behavior, trading, replay, and strategy testing to be demonstrated without internet access.

---

## Educational Scope

MarketSim is intended to demonstrate software engineering and OOP concepts through a realistic trading-system domain.

It is **not** a production trading platform and does not attempt to reproduce the full behavior of NSE, BSE, NYSE, NASDAQ, or any real broker/exchange.

---

## Project Status

The repository currently contains working implementations for:

- Core market simulation
- Dynamic pricing and market statistics
- Order books and matching
- Broker and execution services
- Risk, margin and short selling
- What-If simulation
- Historical data and replay
- Backtesting
- Portfolio intelligence
- Qt 6 GUI

Development is incremental, and the architecture is intentionally modular so additional order types, risk rules, analytics, strategies, and UI features can be added without replacing the core execution path.

---

## License

No license file is currently included in the repository. Add an appropriate license before distributing the project publicly if required.
