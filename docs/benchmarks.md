# Benchmarks

Two harnesses live under `benchmarks/`:

- `ordbk_bench` — Google Benchmark microbenchmarks reporting mean wall-clock time per operation.
- `ordbk_latency` — a standalone profiler that captures individual `submit()` latencies and reports the tail (p50 / p90 / p99 / p99.9 / p99.99 / max).

Build and run:

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DORDBK_BUILD_BENCHMARKS=ON
cmake --build build-bench -j
./build-bench/benchmarks/ordbk_bench
./build-bench/benchmarks/ordbk_latency
```

## Throughput (ordbk_bench)

Measured Release build, GCC 15.2, 12-core x86_64 @ 3.3 GHz.

| Operation | Time / op | Throughput |
|---|---|---|
| Rest a limit order | ~310 ns | 3.2 M ops/s |
| Cross + fill a pair (1 trade) | ~960 ns | 1.04 M ops/s |
| Cancel a resting order | ~500 ns | 2.0 M ops/s |
| Market IOC vs deep book (1 fill) | ~260 ns | 3.8 M ops/s |
| Sweep 100 levels | ~12 µs | 8.2 M fills/s |
| Sweep 500 levels | ~77 µs | 6.5 M fills/s |
| Sweep 1000 levels | ~157 µs | 6.4 M fills/s |

## Latency distribution (ordbk_latency, 2 M samples)

```
mean    : 194 ns
p50     : 200 ns
p99     : 300 ns
p99.9   : 300 ns
p99.99  : 3.3 µs
max     : 76 µs
```

A tight ~200 ns body with a thin tail; the jump at p99.99 is OS scheduling
jitter rather than matching cost. The sub-microsecond percentiles are quantized
to ~100 ns because that is the `steady_clock` resolution on this platform, so
the body should be read as "a couple hundred nanoseconds" and the tail is the
genuinely informative part.

## Method notes

- `BM_LimitRests` leaks one resting order per iteration on purpose (it measures
  the insert path), so its iteration count is capped below the pool capacity.
- The sweep benchmarks rebuild a fresh book inside paused timing so only the
  sweep itself is measured.
- Numbers vary by hardware; rerun locally for figures on your own machine.
