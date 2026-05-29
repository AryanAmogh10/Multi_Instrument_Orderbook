// Microbenchmarks for the matching engine hot path.
//
// Run with: ./ordbk_bench --benchmark_format=console
// For latency percentiles add: --benchmark_repetitions=10
//
// Benchmark categories:
//   BM_LimitRests      - submit a non-crossing limit order (rests in book)
//   BM_LimitCross      - two matching orders produce one trade
//   BM_Cancel          - add then cancel a resting order
//   BM_DeepBook_Sweep  - taker sweeps a 1 000-order deep book (stress path)
//   BM_MarketIOC       - market IOC against 100 levels

#include <benchmark/benchmark.h>

#include "ordbk/instruments/instrument_registry.hpp"
#include "ordbk/matching/book_matcher.hpp"
#include "ordbk/utils/order_pool.hpp"

#include <cassert>

using namespace ordbk;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr InstrumentId kInst{1};
static constexpr std::size_t kPoolCap = 131072;

static Order* make_order(OrderPool& pool,
                         std::uint64_t id,
                         Side side,
                         std::int64_t price,
                         std::uint64_t qty,
                         OrderType type = OrderType::Limit,
                         TimeInForce tif = TimeInForce::GTC)
{
    Order* o = pool.acquire();
    assert(o && "bench pool exhausted");
    *o = Order{
        OrderId{id},
        Price{price},
        Quantity{qty},
        kZeroQty,
        kInst,
        ClientId{1},
        OrderStatus::New,
        side,
        type,
        tif,
        Timestamp{0},
    };
    return o;
}

// ---------------------------------------------------------------------------
// BM_LimitRests: non-crossing limit bid rests in an empty book
// ---------------------------------------------------------------------------
static void BM_LimitRests(benchmark::State& state)
{
    OrderPool pool{kPoolCap};
    OrderBook book{kInst};
    BookMatcher matcher{book, pool};

    std::uint64_t id = 1;
    for (auto _ : state)
    {
        auto* o = make_order(pool, id++, Side::Buy, 100, 1);
        benchmark::DoNotOptimize(matcher.submit(o));
        // After the loop the book grows; that's intentional (measures insert).
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
// Capped iterations: this bench leaks one order per iteration into the book
// on purpose (measures the resting-insert path), so we must not exceed the
// pool capacity.
BENCHMARK(BM_LimitRests)->Iterations(100000);

// ---------------------------------------------------------------------------
// BM_LimitCross: one resting ask + one crossing bid → one trade per iteration
// ---------------------------------------------------------------------------
static void BM_LimitCross(benchmark::State& state)
{
    OrderPool pool{kPoolCap};
    std::uint64_t id = 1;

    for (auto _ : state)
    {
        // Each iteration sets up a fresh book so there's exactly one fill.
        OrderBook book{kInst};
        BookMatcher matcher{book, pool};

        matcher.submit(make_order(pool, id++, Side::Sell, 100, 1));           // rests
        auto res = matcher.submit(make_order(pool, id++, Side::Buy, 100, 1)); // fills
        benchmark::DoNotOptimize(res);
        pool.release(res.order); // taker fully filled - release
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_LimitCross);

// ---------------------------------------------------------------------------
// BM_Cancel: submit a resting bid then cancel it
// ---------------------------------------------------------------------------
static void BM_Cancel(benchmark::State& state)
{
    OrderPool pool{kPoolCap};
    OrderBook book{kInst};
    BookMatcher matcher{book, pool};

    std::uint64_t id = 1;
    for (auto _ : state)
    {
        matcher.submit(make_order(pool, id, Side::Buy, 100, 1));
        benchmark::DoNotOptimize(matcher.cancel(OrderId{id}));
        ++id;
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_Cancel);

// ---------------------------------------------------------------------------
// BM_DeepBook_Sweep: pre-populate 1 000 ask levels, then submit an aggressive
// market buy that sweeps all of them in one shot.
// ---------------------------------------------------------------------------
static void BM_DeepBook_Sweep(benchmark::State& state)
{
    const std::int64_t kDepth = state.range(0);
    OrderPool pool{kPoolCap};
    std::uint64_t id = 1;

    for (auto _ : state)
    {
        state.PauseTiming();
        OrderBook book{kInst};
        BookMatcher matcher{book, pool};
        // Seed the ask side with `kDepth` levels, 1 unit each.
        for (std::int64_t i = 0; i < kDepth; ++i)
        {
            matcher.submit(make_order(pool, id++, Side::Sell, 100 + i, 1));
        }
        auto* taker = make_order(pool,
                                 id++,
                                 Side::Buy,
                                 0,
                                 static_cast<std::uint64_t>(kDepth),
                                 OrderType::Market,
                                 TimeInForce::IOC);
        state.ResumeTiming();

        auto res = matcher.submit(taker);
        benchmark::DoNotOptimize(res);
        pool.release(res.order);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * kDepth);
}
BENCHMARK(BM_DeepBook_Sweep)->Arg(100)->Arg(500)->Arg(1000);

// ---------------------------------------------------------------------------
// BM_MarketIOC: market IOC taker versus a 100-level ask book (repeated with
// the same book - each taker drains levels, so this degrades gracefully)
// ---------------------------------------------------------------------------
static void BM_MarketIOC(benchmark::State& state)
{
    OrderPool pool{kPoolCap};
    OrderBook book{kInst};
    BookMatcher matcher{book, pool};
    std::uint64_t id = 1;

    // Seed once with many levels.
    for (std::int64_t i = 0; i < 100; ++i)
    {
        matcher.submit(make_order(pool, id++, Side::Sell, 100 + i, 1000));
    }

    for (auto _ : state)
    {
        auto* taker = make_order(pool, id++, Side::Buy, 0, 1, OrderType::Market, TimeInForce::IOC);
        auto res = matcher.submit(taker);
        benchmark::DoNotOptimize(res);
        pool.release(res.order);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_MarketIOC);

BENCHMARK_MAIN();
