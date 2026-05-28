// Standalone latency profiler for the matching hot path.
//
// Google Benchmark (orderbook_bench.cpp) reports mean/median throughput.
// This complements it by capturing every individual submit() latency and
// reporting the tail: p50 / p90 / p99 / p99.9 / p99.99 / max.
//
// Method: rest a resting sell, time a single crossing buy (one full trade),
// record the nanoseconds, repeat. The book returns to empty after each
// fully-crossed pair, so one book/matcher is reused across all samples.
//
// Build:  cmake -S . -B build -DVELOX_BUILD_BENCHMARKS=ON
// Run:    ./build/benchmarks/velox_latency

#include "velox/instruments/instrument_registry.hpp"
#include "velox/matching/matcher.hpp"
#include "velox/utils/latency.hpp" // now_ns()
#include "velox/utils/pool.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace velox;

namespace
{

constexpr InstrumentId kInst{1};
constexpr std::size_t kPoolCap = 1024; // only ~2 orders live at once
constexpr std::size_t kWarmup = 100'000;
constexpr std::size_t kSamples = 2'000'000;

Order* make_order(Pool& pool, std::uint64_t id, Side side, std::int64_t price, std::uint64_t qty)
{
    Order* o = pool.acquire_or_abort();
    *o = Order{
        OrderId{id},
        Price{price},
        Quantity{qty},
        kZeroQty,
        kInst,
        ClientId{1},
        OrderStatus::New,
        side,
        OrderType::Limit,
        TimeInForce::GTC,
        Timestamp{0},
    };
    return o;
}

// One sample: rest a sell, time the crossing buy, release the taker.
// Returns elapsed nanoseconds for the timed submit().
std::uint64_t one_cross(Matcher& matcher, Pool& pool, std::uint64_t& id)
{
    matcher.submit(make_order(pool, id++, Side::Sell, 100, 1)); // rests (untimed)
    Order* taker = make_order(pool, id++, Side::Buy, 100, 1);
    const std::uint64_t t0 = now_ns();
    auto res = matcher.submit(taker); // timed: full cross
    const std::uint64_t t1 = now_ns();
    pool.release(res.order);
    return t1 - t0;
}

std::uint64_t pct(const std::vector<std::uint64_t>& sorted, double p)
{
    if (sorted.empty())
        return 0;
    auto idx = static_cast<std::size_t>(p * static_cast<double>(sorted.size() - 1));
    return sorted[idx];
}

} // namespace

int main()
{
    Pool pool{kPoolCap};
    OrderBook book{kInst};
    Matcher matcher{book, pool};
    std::uint64_t id = 1;

    // Warm up caches / branch predictors; discard these.
    for (std::size_t i = 0; i < kWarmup; ++i)
        (void)one_cross(matcher, pool, id);

    std::vector<std::uint64_t> lat;
    lat.reserve(kSamples);
    for (std::size_t i = 0; i < kSamples; ++i)
        lat.push_back(one_cross(matcher, pool, id));

    std::sort(lat.begin(), lat.end());

    std::uint64_t sum = 0;
    for (auto v : lat)
        sum += v;
    const double mean = static_cast<double>(sum) / static_cast<double>(lat.size());

    std::printf("matching cross+fill latency (one full trade per submit)\n");
    std::printf("samples : %zu\n", lat.size());
    std::printf("mean    : %.1f ns\n", mean);
    std::printf("min     : %llu ns\n", static_cast<unsigned long long>(lat.front()));
    std::printf("p50     : %llu ns\n", static_cast<unsigned long long>(pct(lat, 0.50)));
    std::printf("p90     : %llu ns\n", static_cast<unsigned long long>(pct(lat, 0.90)));
    std::printf("p99     : %llu ns\n", static_cast<unsigned long long>(pct(lat, 0.99)));
    std::printf("p99.9   : %llu ns\n", static_cast<unsigned long long>(pct(lat, 0.999)));
    std::printf("p99.99  : %llu ns\n", static_cast<unsigned long long>(pct(lat, 0.9999)));
    std::printf("max     : %llu ns\n", static_cast<unsigned long long>(lat.back()));
    return 0;
}
