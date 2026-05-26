# velox-match

A high-performance C++20 matching engine supporting equities, futures, and options.
Built to explore the internals of exchange matching systems — price-time priority, multi-instrument routing, binary wire protocols, and zero-allocation hot paths.

---

## What it does

- **Price-time priority matching** with full GTC / IOC / FOK / Day support
- **Multi-instrument routing** — one `OrderBook` per instrument, sharded across N worker threads
- **Options support** — OCC-format contract specifications, per-underlying chains, expiry sweeping
- **Binary wire protocol** over TCP — logon, new order, cancel, fill, reject messages
- **Zero heap allocation on the hot path** — pre-allocated `Pool` slab + intrusive linked lists
- **Per-order latency tracking** — atomic histogram recording match time from arrival to result

---

## Architecture

```
TCP Clients
    │
    ▼
┌─────────────────────────────────────┐
│  Gateway (Server + Dispatcher)       │
│  • TCP session management            │
│  • Protocol encode / decode          │
│  • Session state machine             │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│  ShardedMatcher                      │
│  • N worker threads                  │
│  • SPSC ring buffer per shard        │
│  • Consistent hashing by inst. ID    │
│                                      │
│  ┌────────────────────────────────┐  │
│  │  Engine (per shard)            │  │
│  │  ├── OrderBook (AAPL)          │  │
│  │  ├── OrderBook (MSFT)          │  │
│  │  └── OrderBook (AAPL Jan $150C)│  │
│  └────────────────────────────────┘  │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│  Pool  •  LatencyTracker             │
└─────────────────────────────────────┘
```

---

## Building

Requirements: CMake ≥ 3.20, a C++20 compiler (GCC 11+, Clang 13+, MSVC 19.30+), internet access on first configure (FetchContent pulls GoogleTest).

```bash
# Standard build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

```bash
# With address + undefined behavior sanitizers
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DVELOX_SANITIZER=ASAN_UBSAN
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure

# With thread sanitizer
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DVELOX_SANITIZER=TSAN
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```

```bash
# Benchmarks (Google Benchmark)
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DVELOX_BUILD_BENCHMARKS=ON
cmake --build build-bench -j
./build-bench/benchmarks/velox_bench
```

---

## Tests

153 tests across unit, integration, and property-based categories.

```
tests/unit/
  order_test.cpp              – Order struct helpers (remaining, is_terminal, etc.)
  orderbook_test.cpp          – Price-time priority, best bid/ask, cancel
  matching_test.cpp           – Limit/market orders, IOC/FOK, partial fills, sweeps
  engine_test.cpp             – Multi-instrument routing, unknown instrument rejection
  dispatcher_test.cpp         – Gateway session handling, reject reasons
  option_contract_test.cpp    – OCC symbol encoding/decoding, ExpiryDate ordering
  option_chain_test.cpp       – Chain queries (at_expiry, calls_at, expiring_on_or_before)
  expiry_sweeper_test.cpp     – Sweep logic, callback, unregister

tests/integration/
  sharded_engine_test.cpp     – Multi-threaded shard routing and wait_idle
  gateway_e2e_test.cpp        – Full TCP round-trip: logon → order → fill → cancel
  options_e2e_test.cpp        – Option registration, trading, expiry via sweeper

tests/property/
  invariants_test.cpp         – Randomised order sequences; book invariants must always hold
```

---

## Project layout

```
include/velox/
  core/           – Strong types (Price, Quantity, OrderId, …)
  orderbook/      – Order struct, OrderBook, trade record
  matching/       – Matcher, Engine, ShardedMatcher, Sweeper
  instruments/    – InstrumentSpec, InstrumentRegistry, Contract, Chain
  protocol/       – Binary codec, framer, session state machine
  gateway/        – Dispatcher, TCP server/client
  utils/          – Pool, LatencyTracker, SPSC ring buffer

src/              – Implementations
tests/            – GoogleTest suites
benchmarks/       – Google Benchmark microbenchmarks
cmake/            – CompilerWarnings.cmake, Sanitizers.cmake
docs/             – Architecture, protocol spec, benchmark notes
```

---

## Key design decisions

**Integer ticks, never floats** — prices are stored as `int64_t` ticks. A $150.00 strike with a 1-cent tick size is `Price{15000}`. Eliminates rounding issues entirely.

**Strong enum types** — `Price`, `Quantity`, `OrderId`, `InstrumentId` are `enum class` wrappers. The compiler catches mixing them up.

**Intrusive linked list for price levels** — `level_prev` / `level_next` pointers are embedded directly in `Order`. No separate node allocation per order, better cache locality when walking a level.

**Pre-allocated order pool** — `Pool` is a fixed slab allocated at startup. `acquire()` and `release()` are the only allocations on the matching path, and they're mutex-protected but happen outside the inner match loop.

**SPSC queues between threads** — each shard has a wait-free ring buffer for inbound commands. The matching loop itself is single-threaded per instrument, so no locks inside matching.

**Options as first-class instruments** — options are just `InstrumentType::Option` entries in the `InstrumentRegistry`. Each gets a normal `OrderBook`. The `Chain` is an index layer on top for chain queries; `Sweeper` handles end-of-day cleanup.

---

## License

MIT — see [LICENSE](LICENSE).
