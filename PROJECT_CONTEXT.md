# MarketSim Context

MarketSim is a C++17 fictional stock-market and paper-trading simulator.

Workflow:
ChatGPT = architecture/review
Claude = code builder
User = compile/test

Claude uses normal chat, not Claude Code.
Always provide complete copy-paste-ready files, never diffs.

## Architecture

Market
- MarketClock
- Instruments
- Quotes
- PriceEngine
- PriceHistory
- MarketIndex
- MarketStatistics

Trading target:

Order
↓
OrderBook
↓
MatchingEngine
↓
Trade/Fill
↓
Portfolio

## Rules

- UI contains no business logic.
- Order != Trade/Fill != Position.
- MatchingEngine produces fills.
- Portfolio consumes fills.
- Preserve working modules.
- Avoid unnecessary redesign.
- C++17 only.
- Use fictional stocks only.

## Completed

Phase 1: Market architecture
Phase 2: Dynamic market data

## Current

Phase 3: Order Book + Matching Engine