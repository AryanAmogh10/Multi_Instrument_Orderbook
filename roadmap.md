# Project 1: Options-Aware Multi-Instrument Matching Engine (C++)

> **Status:** Planning complete — Phase 0 not yet started
> **Language:** C++20
> **Target:** Portfolio-grade exchange simulator with options support

---

## Table of Contents

- [Project Identity](#project-identity)
- [Architecture Overview](#architecture-overview)
- [Phase 0: Project Setup & Foundations](#phase-0-project-setup--foundations)
- [Phase 1: Core Type System & Single-Instrument Orderbook](#phase-1-core-type-system--single-instrument-orderbook)
- [Phase 2: Multi-Instrument Architecture](#phase-2-multi-instrument-architecture)
- [Phase 3: Wire Protocol & Gateway](#phase-3-wire-protocol--gateway)
- [Phase 4: Performance Engineering](#phase-4-performance-engineering)
- [Phase 5: Options Support](#phase-5-options-support)
- [Phase 6: Market Data Publishing](#phase-6-market-data-publishing)
- [Phase 7: Persistence, Replay & Synthetic Data](#phase-7-persistence-replay--synthetic-data)
- [Phase 8: Polish & Portfolio Presentation](#phase-8-polish--portfolio-presentation)
- [Working With Claude Code](#working-with-claude-code)
- [Realistic Total Timeline](#realistic-total-timeline)
- [What To Do Right Now](#what-to-do-right-now)

---

## Project Identity

**Name suggestion:** `velox-match` or `nyx-exchange` _(pick your own — having a name matters for portfolio presentation)_

**One-line pitch:**
> *"A high-performance C++ matching engine supporting equities, futures, and options with lock-free hot paths, multi-instrument architecture, and sub-microsecond order processing."*

### What makes it different from Tzadiko's orderbook

1. **Multi-instrument** (his is single-orderbook)
2. **Options support** with proper contract specifications
3. **Lock-free critical paths** (his uses coarse mutexes)
4. **Comprehensive order types** including stop, iceberg, peg
5. **Market data publishing** (he just stores state)
6. **FIX-like wire protocol** (his has no protocol)
7. **Persistence and replay** (his has none)
8. **Comprehensive benchmarking** (his has none)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    EXTERNAL CLIENTS                          │
│         (Trading Engines, Test Harnesses, Replayers)         │
└──────────────────────────┬──────────────────────────────────┘
                           │ Wire Protocol (binary)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                    GATEWAY LAYER                             │
│  • Connection management   • Auth   • Rate limiting          │
│  • Protocol decode/encode  • Session state                   │
└──────────────────────────┬──────────────────────────────────┘
                           │ Internal message bus
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                  MATCHING ENGINE CORE                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  InstrumentRegistry                                  │   │
│  │  ├── Equity: AAPL, MSFT, TSLA                        │   │
│  │  ├── Future: ES-DEC25, CL-JAN26                      │   │
│  │  └── Option: AAPL-250117C00150000                    │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│  ┌────────────────────▼─────────────────────────────────┐   │
│  │  OrderBook (one per instrument)                      │   │
│  │  • Price levels  • Order types  • Matching logic     │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│  ┌────────────────────▼─────────────────────────────────┐   │
│  │  Trade Reporter / Market Data Publisher              │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│       PERSISTENCE   │   MARKET DATA FEED   │   METRICS       │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 0: Project Setup & Foundations

> **Goal:** Build a professional C++ project scaffold before writing engine code.
> **Estimated time:** ~1–2 days
> **Deliverable:** A repo that builds, runs a "hello world" test, has CI passing, and looks professional.

### Step 0.1: Repository structure

Create this exact directory structure:

```
velox-match/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
├── .clang-format
├── .clang-tidy
├── .github/
│   └── workflows/
│       └── ci.yml
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   └── FindGTest.cmake
├── include/
│   └── velox/
│       ├── core/
│       ├── instruments/
│       ├── orderbook/
│       ├── matching/
│       ├── protocol/
│       └── utils/
├── src/
│   ├── core/
│   ├── instruments/
│   ├── orderbook/
│   ├── matching/
│   ├── protocol/
│   └── main.cpp
├── tests/
│   ├── unit/
│   ├── integration/
│   └── property/
├── benchmarks/
│   └── orderbook_bench.cpp
├── tools/
│   ├── replay/
│   └── synthetic_data_gen/
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   └── benchmarks.md
└── docker/
    └── Dockerfile
```

### Step 0.2: Build system setup

- **CMake 3.20+** with modular `CMakeLists.txt` per subdirectory
- **C++20 minimum** (you want concepts, ranges, `std::span`)
- **Compiler flags:** `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wnon-virtual-dtor -Wcast-align`
- **Sanitizer presets:** ASan, UBSan, TSan as separate CMake build types
- **Dependencies** (via FetchContent or vcpkg):
  - GoogleTest — unit testing
  - Google Benchmark — microbenchmarks
  - spdlog — logging
  - fmt — formatting (or `std::format` from C++20)
  - nlohmann/json — config files

### Step 0.3: Tooling

- `.clang-format` matching LLVM or Google style
- `.clang-tidy` with `cppcoreguidelines-*` and `performance-*` enabled
- Pre-commit hooks _(optional but professional)_
- GitHub Actions CI: build + test + sanitizer runs on every PR

### Step 0.4: Documentation skeleton

- `README.md` with project pitch, build instructions, architecture summary, current status
- `docs/architecture.md` (start empty, fill as you build)
- `docs/protocol.md` (for the wire protocol spec)
- `docs/benchmarks.md` (for results table)

---

## Phase 1: Core Type System & Single-Instrument Orderbook

> **Goal:** Build a clean, well-tested single-instrument orderbook — better than Tzadiko's baseline. This phase is where you internalize his code and improve it.
> **Estimated time:** ~1 week
> **Deliverable:** Single-instrument orderbook passing 50+ unit tests including property tests.

### Step 1.1: Strong type system

Replace primitives with strongly typed aliases to prevent bugs:

```cpp
// include/velox/core/types.hpp
namespace velox {
    enum class Price       : std::int64_t {};  // in ticks, not floats
    enum class Quantity    : std::uint64_t {};
    enum class OrderId     : std::uint64_t {};
    enum class InstrumentId: std::uint32_t {};
    enum class ClientId    : std::uint32_t {};
    enum class Timestamp   : std::int64_t {};  // nanoseconds since epoch
}
```

> **Why this matters:** Tzadiko uses `using Price = std::int32_t` which lets you accidentally pass quantities where prices are expected. Strong enums prevent this at compile time.

> **⚠️ Pitfall:** Don't use floats for price. **Ever.** Use integer ticks (e.g. `price * 10000` for 4 decimal places).

### Step 1.2: Order representation

Design the `Order` class with these explicit considerations:

- Immutable initial quantity, mutable filled quantity
- Use `enum class` for `Side`, `OrderType`, `TimeInForce`, `OrderStatus`
- Order types to support **(in order of implementation difficulty):**
  1. Limit (GTC, IOC, FOK, Day)
  2. Market
  3. Stop / Stop-Limit
  4. Iceberg (hidden quantity)
  5. Peg (later)

### Step 1.3: Single-instrument orderbook

Reimplement Tzadiko's orderbook with these improvements:

- Same data structure choice (`std::map` for levels + intrusive list for orders at a level)
- **But:** extract matching logic into a separate `MatchingEngine` class — orderbook holds state, matcher operates on it
- Add proper move semantics
- Add `noexcept` where appropriate
- Replace `std::shared_ptr<Order>` with a custom intrusive pointer or object pool _(we'll add this in Phase 4)_

### Step 1.4: Comprehensive unit tests

Write tests that Tzadiko doesn't have:

- Property-based testing with **rapidcheck** (random order sequences, invariants must hold)
- Edge cases: zero quantity (should reject), negative price (should reject), self-cross prevention
- Time-in-force semantics for each combination

---

## Phase 2: Multi-Instrument Architecture

> **Goal:** Support many orderbooks concurrently — the foundation for options later.
> **Estimated time:** ~1–2 weeks
> **Deliverable:** Engine handles 100+ instruments concurrently with measurable throughput.

### Step 2.1: InstrumentRegistry

Design a registry that:

- Stores instrument metadata: symbol, type (equity/future/option), tick size, lot size, currency
- Allows lookup by `InstrumentId` (fast path, integer) or symbol string (slow path, admin)
- Is **immutable after startup** (instruments registered at boot, no dynamic add for now)

### Step 2.2: MatchingEngine as orchestrator

```
MatchingEngine
├── InstrumentRegistry (read-only)
├── std::unordered_map<InstrumentId, std::unique_ptr<OrderBook>>
└── Routes incoming orders to the correct orderbook
```

### Step 2.3: Per-instrument threading model decision

This is a **big architectural choice**. Three options:

| Option | Description | Verdict |
|--------|-------------|---------|
| 1 | Single thread, all instruments | Simple, no contention, doesn't scale |
| 2 | One thread per instrument | Maximum parallelism, complex |
| 3 | **Sharded threads** — N threads, each owns M instruments via consistent hashing | ✅ **Recommended** |

Go with **Option 3** for the right complexity/performance tradeoff.

### Step 2.4: Lock-free order ingestion queue

Replace mutex-protected order submission with a **single-producer-single-consumer (SPSC) lock-free queue** per matching thread. Implement using `std::atomic` and a ring buffer.

> **⚠️ Pitfall:** Don't try to make the *entire* matching engine lock-free. Order ingestion and trade publication should be lock-free; matching itself can remain single-threaded per instrument (much simpler, still fast).

---

## Phase 3: Wire Protocol & Gateway

> **Goal:** Make the engine accessible over a network/IPC boundary — this is what makes it a "real" exchange simulator and lets your C# trading engine connect.
> **Estimated time:** ~1–2 weeks
> **Deliverable:** Two processes (engine + test client) communicating over TCP, exchanging real orders.

### Step 3.1: Protocol design

Design a **binary protocol** (don't use JSON for the hot path — too slow). Inspired by SBE (Simple Binary Encoding) or ITCH/OUCH.

**Messages to support:**

| Direction | Messages |
|-----------|----------|
| Inbound | `NewOrder`, `CancelOrder`, `ModifyOrder` |
| Outbound | `OrderAck`, `OrderReject`, `Fill`, `Cancelled` |
| Outbound (market data) | `MarketDataSnapshot`, `MarketDataIncremental` |
| Session | `Heartbeat`, `Logon`, `Logout` |

**Design considerations:**

- Fixed-size header with message type + length
- Little-endian, packed structs
- Sequence numbers for gap detection
- Document the wire format in `docs/protocol.md`

### Step 3.2: Gateway implementation

- TCP server using either raw sockets, Boost.Asio, or libuv _(recommend Boost.Asio for portability)_
- One thread per connection initially _(you can move to epoll/io_uring later)_
- Message framing: read header → read body → dispatch
- Connection state machine: `NotConnected → LoggedOn → Active → Closing`

### Step 3.3: Client library

Create a thin C++ client library so test harnesses (and eventually your C# engine via interop) can connect. This is what your C# trading engine will eventually call.

> **⚠️ Pitfall:** Don't design the protocol once and never change it. **Version it from day one** (`uint8_t protocol_version` in header).

---

## Phase 4: Performance Engineering

> **Goal:** Move from "correct" to "fast." This is where you generate the impressive numbers for your portfolio.
> **Estimated time:** ~2–3 weeks
> **Deliverable:** Documented benchmarks showing sub-microsecond p99 order processing for limit orders on a single instrument.

### Step 4.1: Benchmarking infrastructure first

**Before optimizing anything:**

- Set up Google Benchmark for microbenchmarks
- Build a load generator tool in `tools/` that fires N orders/sec
- Define your latency metrics: **p50, p99, p99.9, p99.99** (these matter more than averages)
- **Measure baseline first** — you can't optimize what you don't measure

### Step 4.2: Object pools

`std::shared_ptr<Order>` is slow because of atomic refcount and heap allocation. Replace with:

- A fixed-size object pool of `Order` objects, pre-allocated at startup
- Custom intrusive pointer or raw pointer + handle table
- Free list for recycling

### Step 4.3: Cache-friendly data structures

- Replace `std::list` (linked list of orders at a price level) with a **custom intrusive linked list** embedded in the `Order` struct (no separate allocation, better cache locality)
- Consider flat hash maps (`absl::flat_hash_map` or `robin_hood::unordered_map`) instead of `std::unordered_map`
- **Align hot structs to cache line size** (64 bytes) using `alignas(64)`

### Step 4.4: Lock-free order submission

- SPSC ring buffer for orders coming into each matching thread
- MPSC queue for trade publication going out
- Use `std::memory_order_acquire/release` correctly (**not `seq_cst` everywhere**)

### Step 4.5: Latency measurement in the engine itself

Add timestamp-on-receive and timestamp-on-publish, log the delta. Generates the *"we process orders in 800 nanoseconds p99"* claim that wins interviews.

> **⚠️ Pitfall:** Don't optimize prematurely. Phases 1–3 should be correct first. Don't write lock-free code until you've read the relevant chapter of *C++ Concurrency in Action* (Williams) carefully.

---

## Phase 5: Options Support

> **Goal:** This is where your project becomes **genuinely rare.**
> **Estimated time:** ~3–4 weeks
> **Deliverable:** Engine handles equity options with proper chain organization, basic single-leg trading. Multi-leg as stretch.

### Step 5.1: Options contract specification

An option isn't just a symbol — it's a **tuple**. Design `OptionContract`:

- Underlying symbol (`AAPL`)
- Strike price (`150.00`)
- Expiry date (`2026-01-17`)
- Option type (`Call` / `Put`)
- Style (`American` / `European`)
- Multiplier (typically `100` for equity options)
- Settlement (`Physical` / `Cash`)

**OCC-style symbol encoding:** `AAPL  260117C00150000` (root + expiry + C/P + strike)

### Step 5.2: Option chain organization

A single underlying has many options. Design the storage:

- `OptionChain` per underlying, indexed by `(expiry, strike)`
- Efficient queries: *"give me all calls at expiry X"*, *"give me ATM options"*
- Each option still has its own `OrderBook` — the chain is just the organizational layer

### Step 5.3: Option-specific order types

Options trading has unique constructs:

- **Multi-leg orders:** Spreads (call spread, iron condor, etc.) — atomic execution across multiple options
- **Combos:** User-defined multi-leg orders

> This is hard. Recommend defer to Step 5.5 or skip if scope is too big.

### Step 5.4: Cross-product risk checks _(optional, advanced)_

Reject orders that would create undefined positions (e.g., shorting a call without sufficient collateral). This is technically a risk-layer concern but exchanges do basic checks.

### Step 5.5: Multi-leg matching _(stretch goal)_

A spread order matches atomically: **all legs fill or none do.** Implementation:

- Detect spread orders via order type flag
- Lock all relevant orderbooks
- Check fillability across all legs
- Execute atomically or reject

> **⚠️ Pitfall:** Options **expire**. You need an "expiry sweeper" that removes expired contracts from active trading at end of day. Build this into Phase 5.

---

## Phase 6: Market Data Publishing

> **Goal:** Other systems (your C# engine, dashboards) need to know what's happening in the book.
> **Estimated time:** ~1–2 weeks
> **Deliverable:** Clients can subscribe to market data and see real-time book updates.

### Step 6.1: Market data message types

| Level | Description |
|-------|-------------|
| **L1 (top of book)** | Best bid, best ask, last trade |
| **L2 (full depth)** | All price levels with aggregate quantities |
| **L3 (order-by-order)** | Every individual order (rarely public, but you can support it) |
| **Trade tape** | Stream of executed trades |

### Step 6.2: Snapshot + incremental architecture

**Industry standard:**

- Periodic full snapshots (e.g. every 5 seconds)
- Incremental updates between snapshots (add/modify/delete at a price level)
- Consumers join by getting a snapshot then applying incrementals

### Step 6.3: Multicast vs unicast

For your project: just publish over TCP to subscribed clients.
_(Real exchanges use UDP multicast — out of scope but mention in docs as future work.)_

---

## Phase 7: Persistence, Replay & Synthetic Data

> **Goal:** Make the engine debuggable, testable, and demonstrable.
> **Estimated time:** ~1–2 weeks
> **Deliverable:** Engine can be killed mid-session and recover to identical state.

### Step 7.1: Order journal

Every inbound message written to an **append-only binary log file.** Used for:

- Crash recovery (replay journal on startup)
- Debugging (what orders led to this state?)
- Backtesting (replay historical sessions)

### Step 7.2: Trade ledger

Every executed trade written separately. Used for:

- P&L reconciliation
- Compliance demonstration
- Analytics

### Step 7.3: Synthetic data generator

A separate tool in `tools/synthetic_data_gen/` that:

- Generates realistic order flow for equities and options
- Configurable: arrival rate (Poisson), price distribution (around fair value with noise), order size distribution
- Generates options chains with realistic implied volatility skew
- Output: a journal file you can replay into the engine

> **Why this matters:** This is how you demonstrate the engine without paying for data. You can show **10 million orders processed** in a benchmark.

---

## Phase 8: Polish & Portfolio Presentation

> **Goal:** Make this look like a project that took itself seriously.
> **Estimated time:** ~1 week
> **Deliverable:** A project page that would impress a quant dev hiring manager at first glance.

### Step 8.1: Documentation

- `README.md`: pitch, screenshots/diagrams, build instructions, benchmark results table
- `docs/architecture.md`: full architecture diagrams (use Mermaid or PlantUML)
- `docs/protocol.md`: wire format spec
- `docs/benchmarks.md`: methodology + results
- `docs/design_decisions.md`: explain why you made key choices — **this is gold for interviews**

### Step 8.2: Demo material

- A short demo script that spins up the engine, fires 100k orders, shows the book state
- A simple TUI dashboard (using `ftxui` or similar) that visualizes order flow in real time
- _Optional:_ a tiny web dashboard via WebSocket bridge

### Step 8.3: Blog post / write-up

Write 1–2 articles explaining the most interesting parts. Hosts: dev.to, Medium, or your own site. This drives traffic to the GitHub.

---

## Working With Claude Code

When handing pieces to Claude Code:

1. **Always give it the phase context.**
   > *"I'm in Phase 2 of the velox-match project. Here's the architecture doc. I need to implement the InstrumentRegistry."*

2. **Give it explicit type definitions first.** Hand it `types.hpp` so it doesn't invent its own.

3. **Test-driven prompts work best:**
   > *"Here are the unit tests this class must pass. Implement it."*

4. **Review every diff carefully.** Especially for lock-free code — Claude Code can write subtly wrong concurrency. **Always verify with TSan.**

5. **Don't let it skip the benchmarks.** It will want to. Make it generate the benchmark file with each new component.

---

## Realistic Total Timeline

If you're working consistently (~10–15 hours/week alongside other commitments):

| Phases | Duration |
|--------|----------|
| Phases 0–2 | ~1 month |
| Phases 3–4 | ~1.5 months |
| Phase 5 | ~1 month |
| Phases 6–8 | ~1 month |
| **Total** | **~4.5 months** |

> You'd have a deliverable portfolio piece after Phase 4 (~2.5 months) even if you stop there.

---

## What To Do Right Now

1. **Pick a project name**
2. **Create the GitHub repo** with Phase 0 structure
3. **Get CMake building "hello world"** with all warnings/sanitizers configured
4. **Read Tzadiko's full orderbook code** (the C++ one) end-to-end — *understand every line before you improve it*
