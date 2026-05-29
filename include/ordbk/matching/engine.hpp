#pragma once

#include "ordbk/instruments/instrument_registry.hpp"
#include "ordbk/matching/matcher.hpp"
#include "ordbk/utils/pool.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace ordbk
{

// Routes orders to the right OrderBook+Matcher by InstrumentId.
// Single-threaded - ShardedMatcher handles concurrency by running one Engine
// per worker thread, each owning a disjoint slice of instruments.
//
// Registry must be frozen before constructing this.
class Engine
{
public:
    // Filter lets ShardedMatcher assign instruments to shards
    using ShardFilter = std::function<bool(InstrumentId)>;

    explicit Engine(const InstrumentRegistry& registry, Pool& pool, ShardFilter filter = {});

    [[nodiscard]] Order* acquire_order() noexcept { return pool_.acquire(); }
    void release_order(Order* o) noexcept { pool_.release(o); }

    MatchResult submit(Order* order);
    bool cancel(InstrumentId instrument, OrderId id);

    // Cancels all resting orders for an instrument and removes it from trading.
    bool expire_instrument(InstrumentId id);

    [[nodiscard]] const OrderBook* book(InstrumentId id) const noexcept;
    [[nodiscard]] OrderBook* book(InstrumentId id) noexcept;
    [[nodiscard]] std::size_t book_count() const noexcept { return book_ptrs_.size(); }
    [[nodiscard]] const InstrumentRegistry& registry() const noexcept { return registry_; }

    LatencyTracker::Snapshot latency_snapshot() const noexcept;

private:
    struct Slot
    {
        std::unique_ptr<OrderBook> book;
        std::unique_ptr<Matcher> matcher;
    };

    const InstrumentRegistry& registry_;
    Pool& pool_;
    std::unordered_map<std::uint32_t, Slot> matchers_;        // id -> book+matcher
    std::unordered_map<std::uint32_t, OrderBook*> book_ptrs_; // id -> raw book ptr
};

// keep old names working
using MatchingEngine = Engine;
using InstrumentFilter = Engine::ShardFilter;

} // namespace ordbk
