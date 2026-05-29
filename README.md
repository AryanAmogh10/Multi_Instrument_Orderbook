# ordbk-match

A high-performance C++20 matching engine supporting equities, futures, and options.
Built to explore the internals of exchange matching systems - price-time priority, multi-instrument routing, binary wire protocols, and zero-allocation hot paths.

---

## What it does

- **Price-time priority matching** with full GTC / IOC / FOK / Day support
- **Multi-instrument routing** - one `OrderBook` per instrument, sharded across N worker threads
- **Options support** - OCC-format contract specifications, per-underlying chains, expiry sweeping
- **Binary wire protocol** over TCP - logon, new order, cancel, fill, reject messages
- **Zero heap allocation on the hot path** - pre-allocated `Pool` slab + intrusive linked lists
- **Per-order latency tracking** - atomic histogram recording match time from arrival to result

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
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DORDBK_SANITIZER=ASAN_UBSAN
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure

# With thread sanitizer
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DORDBK_SANITIZER=TSAN
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```

```bash
# Benchmarks (Google Benchmark)
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DORDBK_BUILD_BENCHMARKS=ON
cmake --build build-bench -j
./build-bench/benchmarks/ordbk_bench
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
include/ordbk/
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

**Integer ticks, never floats** - prices are stored as `int64_t` ticks. A $150.00 strike with a 1-cent tick size is `Price{15000}`. Eliminates rounding issues entirely.

**Strong enum types** - `Price`, `Quantity`, `OrderId`, `InstrumentId` are `enum class` wrappers. The compiler catches mixing them up.

**Intrusive linked list for price levels** - `level_prev` / `level_next` pointers are embedded directly in `Order`. No separate node allocation per order, better cache locality when walking a level.

**Pre-allocated order pool** - `Pool` is a fixed slab allocated at startup. `acquire()` and `release()` are the only allocations on the matching path, and they're mutex-protected but happen outside the inner match loop.

**SPSC queues between threads** - each shard has a wait-free ring buffer for inbound commands. The matching loop itself is single-threaded per instrument, so no locks inside matching.

**Options as first-class instruments** - options are just `InstrumentType::Option` entries in the `InstrumentRegistry`. Each gets a normal `OrderBook`. The `Chain` is an index layer on top for chain queries; `Sweeper` handles end-of-day cleanup.

---

## Performance

Single-threaded microbenchmarks on the matching hot path (Release build, GCC 15.2, 12-core x86_64 @ 3.3 GHz). Measured with Google Benchmark - these are wall-clock times per operation, not synthetic estimates.

| Operation | Time / op | Throughput |
|---|---|---|
| Rest a limit order | ~310 ns | 3.2 M ops/s |
| Cross + fill a pair (1 trade) | ~960 ns | 1.04 M ops/s |
| Cancel a resting order | ~500 ns | 2.0 M ops/s |
| Market IOC vs deep book (1 fill) | ~260 ns | 3.8 M ops/s |
| Sweep 100 levels | ~12 µs | 8.2 M fills/s |
| Sweep 500 levels | ~77 µs | 6.5 M fills/s |
| Sweep 1000 levels | ~157 µs | 6.4 M fills/s |

Sweep throughput scales close to linearly with depth, as expected - most of the cost is walking the price ladder and recording trades. Numbers will vary by hardware; rebuild with `-DORDBK_BUILD_BENCHMARKS=ON` and run `./build/benchmarks/ordbk_bench` to measure on your own machine.

### Latency distribution

`ordbk_latency` captures 2 M individual `submit()` timings for the cross-and-fill path and reports the tail:

```
mean    : 194 ns
p50     : 200 ns
p99     : 300 ns
p99.9   : 300 ns
p99.99  : 3.3 µs    ← scheduler jitter
max     : 76 µs     ← worst-case OS stall
```

A tight ~200 ns body with a thin tail. The jump at p99.99 is OS scheduling noise, not matching cost. Note: these were measured with `std::steady_clock` at ~100 ns resolution on this platform, so the sub-microsecond percentiles are quantized to 100 ns steps - the body is "≤ a couple hundred ns", and the tail is what's genuinely informative.

---

## License

MIT - see [LICENSE](LICENSE).
