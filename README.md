# velox-match

> High-performance C++20 matching engine supporting equities, futures, and options — with lock-free hot paths, multi-instrument architecture, and sub-microsecond order processing as the target.

**Status:** Phase 0 complete — scaffold builds, smoke tests pass. Engine logic begins in Phase 1.

See [roadmap.md](roadmap.md) for the full phase plan.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/velox_engine
```

### Sanitizer builds

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DVELOX_SANITIZER=ASAN_UBSAN
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DVELOX_SANITIZER=TSAN
```

### Benchmarks

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DVELOX_BUILD_BENCHMARKS=ON
cmake --build build-bench -j
./build-bench/benchmarks/velox_bench
```

## Requirements

- CMake ≥ 3.20
- C++20 compiler (GCC 11+, Clang 13+, MSVC 19.30+)
- Internet access on first configure (FetchContent pulls GoogleTest / Google Benchmark)

## Architecture

See [docs/architecture.md](docs/architecture.md). The engine layers are: Gateway → Matching Engine Core (InstrumentRegistry + per-instrument OrderBook) → Trade Reporter / Market Data Publisher → Persistence.

## Layout

```
include/velox/  Public headers
src/            Implementation
tests/          Unit / integration / property tests (GoogleTest)
benchmarks/     Google Benchmark microbenchmarks
tools/          Replay + synthetic data generator
docs/           Architecture, protocol, benchmark write-ups
cmake/          Reusable CMake modules
```

## License

MIT — see [LICENSE](LICENSE).
