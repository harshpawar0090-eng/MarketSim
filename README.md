# MarketSim — Personal Portfolio Intelligence & What-If Simulation Engine

MarketSim is a **C++17 console-based stock-market portfolio simulation system** built as an Object-Oriented Programming project.

The system allows users to manage a virtual investment portfolio, execute simulated buy/sell transactions, analyze portfolio performance, and experiment with hypothetical market scenarios without modifying their actual portfolio.

---

## Features

- Simulated stock market with fictional companies
- Multiple stock sectors
- Virtual cash balance
- Buy and sell stocks
- Holdings and quantity tracking
- Weighted average purchase cost
- Current market value
- Realized P&L
- Unrealized P&L
- Total portfolio value
- Transaction history
- Portfolio analysis
- Sector allocation
- Bull-market What-If scenarios
- Bear-market What-If scenarios
- Custom stock-price scenarios
- Baseline vs hypothetical comparison
- Input validation and error handling
- Simulation isolated from the real portfolio

---

## Project Architecture

```text
MarketSim/
│
├── main.cpp
│
├── Market/
│   ├── Market.h
│   └── Market.cpp
│
├── Trading/
│   ├── Trading.h
│   └── Trading.cpp
│
├── Portfolio/
│   ├── Portfolio.h
│   └── Portfolio.cpp
│
├── Simulation/
│   ├── Simulation.h
│   └── Simulation.cpp
│
├── UI/
│   ├── Menu.h
│   └── Menu.cpp
│
└── README.md
```

---

## Modules

### Market

Contains:

- `Sector`
- `Stock`
- `Market`

Responsible for maintaining the simulated stock market and current stock prices.

### Trading

Contains:

- `Order`
- `BuyOrder`
- `SellOrder`

Responsible for validating and executing simulated trades.

Inheritance hierarchy:

```text
Order
├── BuyOrder
└── SellOrder
```

### Portfolio

Contains:

- `Transaction`
- `Holding`
- `Portfolio`
- `PortfolioAnalyzer`

Responsible for:

- Virtual cash
- Holdings
- Average purchase cost
- Transaction history
- Realized P&L
- Unrealized P&L
- Portfolio value
- Portfolio analysis

### Simulation

Contains:

- `Scenario`
- `BullScenario`
- `BearScenario`
- `CustomScenario`
- `SimulationResult`
- `SimulationEngine`

Inheritance hierarchy:

```text
Scenario
├── BullScenario
├── BearScenario
└── CustomScenario
```

The simulation system operates on read-only market and portfolio data, ensuring that hypothetical scenarios do not modify the real portfolio.

### UI

Contains:

- `Menu`

Responsible for console interaction and coordinating operations between the different modules.

---

## OOP Concepts Demonstrated

MarketSim provides practical examples of:

- Classes and objects
- Encapsulation
- Abstraction
- Information hiding
- Constructors
- Member initializer lists
- Composition
- Inheritance
- Method overriding
- Virtual functions
- Abstract classes
- Runtime polymorphism
- Dynamic binding
- Object interaction
- STL containers
- References
- `const` correctness
- RAII
- Exception handling
- Modular design

---

## Technologies

- **Language:** C++17
- **Libraries:** C++ Standard Library
- **Application:** Console-based

No external frameworks, APIs, databases, networking libraries, or GUI libraries are required.

---

## Initial Market

The MVP contains fictional stocks across multiple sectors.

| Symbol | Company | Sector | Initial Price |
|---|---|---|---:|
| NOVA | Nova Technologies | Technology | $150.00 |
| QBIT | Qubit Systems | Technology | $320.50 |
| SOLR | Solaris Energy | Energy | $75.25 |
| PETR | Petro Dynamics | Energy | $62.10 |
| BNKX | Bankex Financial | Finance | $98.40 |
| TRUST | TrustCore Capital | Finance | $54.75 |
| MEDI | MediCare Plus | Healthcare | $210.00 |
| CURA | Cura Biosciences | Healthcare | $88.60 |

### Starting Capital

```text
$100,000.00
```

---

## Application Menu

```text
===================== MarketSim =====================
1. View Market
2. View Portfolio
3. Buy Stock
4. Sell Stock
5. Transaction History
6. Portfolio Analysis
7. Run What-If Simulation
8. Exit
======================================================
```

---

## Building the Project

Open a terminal inside the `MarketSim` directory.

Compile using:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -o marketsim main.cpp Market/Market.cpp Trading/Trading.cpp Portfolio/Portfolio.cpp Simulation/Simulation.cpp UI/Menu.cpp
```

---

## Running the Project

### Windows PowerShell

```powershell
.\marketsim.exe
```

### Linux/macOS

```bash
./marketsim
```

---

## Basic Workflow

```text
Start MarketSim
      │
      ▼
 View Market
      │
      ▼
 Buy / Sell Stocks
      │
      ▼
 View Portfolio
      │
      ▼
 Transaction History
      │
      ▼
 Portfolio Analysis
      │
      ▼
 What-If Simulation
      │
      ▼
 Compare Results
      │
      ▼
     Exit
```

---

## What-If Simulation

MarketSim allows users to test hypothetical market scenarios without changing their real portfolio.

For example:

```text
Current NOVA price: $150

Bull Scenario: +10%

Hypothetical NOVA price: $165
```

If the portfolio contains 15 shares:

```text
Current value:

15 × $150 = $2,250


Hypothetical value:

15 × $165 = $2,475


Change:

+$225
```

The actual market and portfolio remain unchanged after the simulation.

---

## Error Handling

The system validates operations such as:

- Invalid stock symbols
- Invalid quantities
- Insufficient cash
- Insufficient shares
- Invalid orders
- Invalid user input

Invalid operations are rejected without corrupting portfolio state.

---

## Team Module Division

The project can be divided between four team members:

| Team Member | Responsibility |
|---|---|
| Member 1 | Market Module |
| Member 2 | Trading Module |
| Member 3 | Portfolio Module |
| Member 4 | Simulation Module |

UI and final integration can be coordinated across the team.

---

## Dependency Structure

The project follows a simple dependency structure:

```text
              Market
             ▲      ▲
             │      │
             │      │
        Portfolio   │
             ▲      │
             │      │
          Trading   │
                    │
             ┌──────┘
             │
        Simulation
             ▲
             │
             UI
             ▲
             │
            main
```

The key design principle is that **Simulation only reads the real Market and Portfolio state**.

It must not modify either one.

---

## Design Principles

MarketSim follows several important C++ and software-engineering principles:

- Separation of responsibilities
- Encapsulation
- Composition over unnecessary inheritance
- Runtime polymorphism where it is actually useful
- `const` correctness
- RAII
- STL-based data structures
- Exception-based error handling
- Minimal global state
- Modular compilation
- Clear dependency boundaries

---

## Project Scope

MarketSim is an **educational stock-market simulation**, not a real trading platform.

It does not currently provide:

- Live market data
- Real-money transactions
- Brokerage integration
- Real-time trading
- Options
- Futures
- Advanced derivatives
- High-frequency trading
- External databases
- Networking
- GUI

The architecture is intentionally designed so that additional functionality can be introduced later without rewriting the entire system.

---

## Testing

The current MVP has been tested for:

- Market display
- Initial portfolio state
- Buying stocks
- Multiple purchases
- Weighted average cost
- Selling stocks
- Realized P&L
- Transaction history
- Bull scenarios
- Invalid stock symbols
- Insufficient cash
- Insufficient shares
- Invalid input handling
- Simulation isolation

Example verified workflow:

```text
Initial Cash       = $100,000
Buy 10 NOVA       = -$1,500
Buy 10 NOVA       = -$1,500
Sell 5 NOVA        = +$750

Remaining Shares   = 15
Remaining Cash     = $97,750

NOVA Price         = $150
Holdings Value     = $2,250

Portfolio Value    = $100,000
```

A `+10%` Bull Scenario produces:

```text
Hypothetical NOVA Price = $165
Hypothetical Value      = $2,475
Change                  = +$225
```

The real portfolio remains unchanged after the simulation.

---

## Educational Objective

The primary objective of MarketSim is to provide a practical environment for learning:

```text
C++
 │
 ├── OOP
 │    ├── Encapsulation
 │    ├── Abstraction
 │    ├── Inheritance
 │    └── Polymorphism
 │
 ├── STL
 │    ├── map
 │    ├── vector
 │    └── unordered_map
 │
 ├── Memory Management
 │    ├── RAII
 │    └── Object Lifetime
 │
 ├── Software Engineering
 │    ├── Modular Design
 │    ├── Separation of Concerns
 │    └── Dependency Management
 │
 └── DSA Foundation
      ├── Containers
      ├── Searching
      ├── Aggregation
      └── Data Processing
```

---

## Status

**Current Version: MVP**

The current version provides the complete core simulation workflow and serves as the foundation for future extensions.
