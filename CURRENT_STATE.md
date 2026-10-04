# MarketSim — Current State

## Project Overview

MarketSim is a **C++17 educational simulated equity-market, broker, and paper-trading platform**.

Long-term goals:
- Dynamic simulated equity market
- Order book and matching engine
- Broker layer
- Advanced order types
- Risk management
- Margin and short selling
- Historical replay
- Backtesting
- Portfolio intelligence
- Qt GUI

The project is developed incrementally. Existing architecture must be preserved unless there is a strong reason to change it.

---

# 5-Phase Roadmap

| Phase | Scope | Status |
|---|---|---|
| Phase 1 | Core Market Architecture | ✅ Complete |
| Phase 2 | Dynamic Market + Price Engine | ✅ Complete |
| Phase 3 | Trading + Broker + Risk/Margin/Short Selling | ✅ Implemented |
| Phase 4 | Historical Market + Replay + Backtesting | ⏳ Next |
| Phase 5 | Portfolio Intelligence + Qt GUI | ⏳ Future |

---

# Phase 1 — Core Market Architecture

**Status: COMPLETE**

Implemented:
- `MarketClock`
- Instruments
- Quotes
- Static instrument/reference data
- Dynamic market data separation
- Market APIs for instruments and quotes

---

# Phase 2 — Dynamic Market + Price Engine

**Status: COMPLETE**

Implemented:
- `PriceEngine`
- Dynamic price movement
- Market ticks
- `MarketClock` advancement
- OHLC history
- Volume
- Turnover
- `MarketIndex`
- `MarketStatistics`
- Gainers/losers and market statistics
- 40+ fictional equities
- Sector classification
- MarketSim 50 basket
- Console inspection of market state

The current price engine is a simulated market generator, not a real-market data feed.

---

# Phase 3 — Trading + Broker + Risk/Margin/Short Selling

**Status: IMPLEMENTED**

Phase 3 established the core trading infrastructure.

## Order System

Implemented:
- Market orders
- Limit orders
- Stop orders
- Stop-Loss orders
- Take-Profit orders
- Stop-Limit orders
- Order IDs
- Order status
- Order lifecycle
- Pending trigger orders
- Order cancellation
- Partial fills

Order lifecycle supports:

```text
NEW
WAITING_FOR_TRIGGER
TRIGGERED / ACTIVE
PARTIALLY_FILLED
FILLED
CANCELLED
```

Orders, fills/trades, and positions are separate concepts.

## Order Book

Implemented:
- Per-symbol order books
- Bid/ask sides
- Price-time priority
- FIFO within price levels
- Best bid/ask
- Market depth
- Order cancellation
- Multiple price levels
- Resting limit orders

## Matching Engine

Implemented:
- Price-time matching
- Resting-order-price execution
- Market-order execution
- Limit-order crossing
- Partial fills
- Multi-level execution
- Self-trade prevention
- Market-maker liquidity
- Liquidity refresh/requoting
- Fill generation
- Trade/fill history

The MatchingEngine does not own portfolio accounting.

## Broker

Implemented:
- Broker facade
- Order placement/cancellation
- Open/pending order access
- Market depth access
- Account-risk access
- Buying-power access
- Risk configuration
- Market-update processing

The Broker does not contain matching logic.

## Execution Service

Architecture:

```text
UI
 ↓
Broker
 ↓
RiskManager
 ↓
ExecutionService
 ↓
MatchingEngine
 ↓
OrderBook
 ↓
Fills
 ↓
Portfolio / Positions
```

Implemented:
- Order validation
- Reservations
- Fill settlement
- Market statistics updates
- Stop-order trigger processing
- Liquidity refresh
- Margin enforcement
- Forced liquidation

---

# Risk Management

Implemented `RiskManager`.

Configurable controls:
- Buying power
- Maximum order quantity
- Maximum position quantity
- Maximum gross exposure
- Short-selling enabled/disabled
- Initial margin rate
- Maintenance margin rate
- Liquidation buffer
- Automatic liquidation

Risk validation occurs before an order reaches the MatchingEngine.

Rejected orders should not mutate:
- Cash
- Positions
- Reservations
- Portfolio
- Order book

---

# Portfolio / Position System

Portfolio supports:
- Long positions
- Short positions
- Signed position views
- Average entry price
- Cash
- Holdings
- Short quantities
- Realized P&L
- Unrealized P&L
- Transaction history

Position convention:

```text
Positive quantity → Long
Negative quantity → Short
```

Long and short accounting are handled separately internally.

---

# Short Selling

Implemented:
- Opening short positions
- Increasing short positions
- Partial short covering
- Full short covering
- Short P&L
- Prevention of over-covering
- Short-selling enable/disable

Short positions are not treated as owned long shares for sell validation.

---

# Margin

Implemented a simplified educational margin model.

Configurable:
- Initial margin
- Maintenance margin
- Liquidation buffer
- Leverage

Risk calculations include:
- Equity
- Long market value
- Short market value
- Gross exposure
- Leverage
- Initial margin required
- Maintenance margin required
- Available margin
- Buying power
- Margin status

**Important:** this is a simulator model and does not attempt to exactly reproduce NSE margin rules.

---

# Margin Calls

Implemented:
- Continuous margin monitoring
- Maintenance-margin breach detection
- Margin-call state
- Automatic liquidation when enabled

---

# Forced Liquidation

Implemented:
- Long-position liquidation
- Short-position covering
- Position liquidation through the existing execution path
- Open-order cancellation before liquidation
- Bounded liquidation attempts
- Protection against infinite liquidation loops

Forced liquidation does not bypass the normal trading architecture.

---

# Advanced Order Behavior

Stop-based orders remain outside the order book until triggered.

After triggering:

```text
Stop
    → executable market order

Stop-Loss
    → executable market order

Take-Profit
    → executable market order

Stop-Limit
    → executable limit order
```

Triggered orders still pass through:

```text
RiskManager
→ ExecutionService
→ MatchingEngine
→ OrderBook
```

---

# Phase 3 Testing

Previously verified advanced-order test result:

```text
186 checks, 0 failed
ALL TESTS PASSED
```

The final Risk/Margin/Short-Selling implementation was added after that test run.

Therefore:

**A fresh full compile and regression test of the current final Phase 3 state is still required before Phase 3 is considered fully validated.**

Do not claim Phase 3 is fully tested unless the current project passes its complete regression suite.

---

# Current Project Structure

```text
MarketSim/
│
├── Market/
│   ├── Market.h
│   ├── Market.cpp
│   ├── PriceEngine.h
│   ├── PriceEngine.cpp
│   ├── MarketIndex.h
│   ├── MarketIndex.cpp
│   ├── MarketStatistics.h
│   └── MarketStatistics.cpp
│
├── Execution/
│   ├── Order.h
│   ├── Order.cpp
│   ├── OrderBook.h
│   ├── OrderBook.cpp
│   ├── MatchingEngine.h
│   ├── MatchingEngine.cpp
│   ├── ExecutionService.h
│   └── ExecutionService.cpp
│
├── Broker/
│   ├── Broker.h
│   └── Broker.cpp
│
├── Risk/
│   ├── RiskManager.h
│   └── RiskManager.cpp
│
├── Portfolio/
│   ├── Portfolio.h
│   └── Portfolio.cpp
│
├── Trading/
│   ├── Trading.h
│   └── Trading.cpp
│
├── Simulation/
│   ├── Simulation.h
│   └── Simulation.cpp
│
├── UI/
│   ├── Menu.h
│   └── Menu.cpp
│
├── tests/
│   ├── Phase3Tests.cpp
│   └── Phase3StopTests.cpp
│
├── main.cpp
└── CURRENT_STATE.md
```

---

# Architecture Rules

1. UI contains presentation/input logic only.
2. Broker is the application-facing trading facade.
3. RiskManager handles risk validation and risk state.
4. ExecutionService coordinates execution and settlement.
5. MatchingEngine performs order matching.
6. OrderBook stores resting orders.
7. Portfolio owns account and position accounting.
8. Market owns market state.
9. Do not bypass the execution architecture.
10. Do not create a second fake trading system for backtesting.
11. Keep C++17.
12. Avoid unnecessary abstractions.
13. Preserve existing functionality.
14. Build incrementally rather than rewriting the project.

Trading should continue through:

```text
Broker
→ RiskManager
→ ExecutionService
→ MatchingEngine
→ OrderBook
→ Fills
→ Portfolio
```

---

# Phase 4 — Historical Market + Replay + Backtesting

**Status: NEXT**

Phase 4 turns MarketSim into a platform that can replay historical market data and evaluate trading strategies.

## Historical Market Data

Implement:
- Historical OHLCV data
- Timestamps
- Historical time-series representation
- Historical data loading/saving
- Separation between historical data and generated/live-style market data
- Structure that can later accept real datasets

Initially, data can remain simulated/local. No live API is required.

## Market Replay

Implement:
- Replay clock
- Historical market replay
- Step-by-step replay
- Controlled progression through historical data
- Market state updates from replayed data
- Existing trading system observing replayed prices normally

Replay should integrate with the existing Market architecture.

## Backtesting Engine

Implement:
- Strategy interface
- Backtesting engine
- Strategy execution over historical data
- Strategy-generated orders
- Broker-integrated order execution
- Trade recording
- Equity curve
- Portfolio history

Intended flow:

```text
Historical Data
      ↓
Replay Clock
      ↓
Market
      ↓
Strategy
      ↓
Broker
      ↓
RiskManager
      ↓
ExecutionService
      ↓
MatchingEngine
      ↓
OrderBook
      ↓
Fills
      ↓
Portfolio
      ↓
Backtest Results
```

## Backtest Analytics

At minimum:
- Total return
- Benchmark return
- Number of trades
- Win rate
- Equity curve
- Maximum drawdown
- Transaction costs
- Slippage where applicable

## Look-Ahead Bias Protection

A strategy must only access information available at the current replay point. It must not inspect future candles/data when making the current trading decision.

## Phase 4 Testing

Add focused tests for:
- Historical data representation
- Historical data loading/saving
- Replay progression
- Replay clock
- Strategy execution
- Order generation
- Trade recording
- Portfolio/P&L behavior
- Equity curve
- Drawdown
- Look-ahead-bias prevention
- Existing Phase 3 regression behavior

## Phase 4 Constraints

Do NOT introduce:
- Qt
- Live market APIs
- Network market feeds
- Options
- Futures
- Derivatives
- Unrelated financial concepts
- A second execution engine
- A second portfolio/accounting system

Keep Phase 4 focused on:

**Historical Data → Replay → Strategy → Existing Trading Architecture → Backtest Results**

---

# Phase 5 — Portfolio Intelligence + Qt GUI

**Status: FUTURE**

Planned:
- Advanced portfolio analytics
- Portfolio dashboard
- Risk dashboard
- Watchlist
- Stock explorer
- Stock detail screen
- Historical charts
- Order placement interface
- Order book UI
- Trade history
- Portfolio history
- Qt 6 GUI
- Final platform integration

Qt should not be introduced during Phase 4.

---

# Current Development Status

```text
Phase 1 — Core Market Architecture
    ✅ Complete

Phase 2 — Dynamic Market + Price Engine
    ✅ Complete

Phase 3 — Trading + Broker + Risk/Margin/Short
    ✅ Implemented
    ⚠️ Final regression validation still required

Phase 4 — Historical Market + Replay + Backtesting
    ⏳ NEXT

Phase 5 — Portfolio Intelligence + Qt GUI
    ⏳ Future
```

The **actual project files on disk are always the source of truth**.

Before modifying existing architecture, inspect the current files and preserve their existing APIs and behavior wherever practical.
    