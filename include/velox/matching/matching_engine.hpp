#pragma once

#include "velox/instruments/instrument_registry.hpp"
#include "velox/matching/book_matcher.hpp"
#include "velox/utils/order_pool.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace velox {

// Orchestrator: owns one OrderBook + one BookMatcher per registered instrument,
// routes incoming orders by InstrumentId. Single-threaded (not thread-safe);
// concurrency comes from ShardedEngine which composes one of these per shard.
//
// Phase 4: takes an external OrderPool reference.  The pool is shared with the
// caller (e.g. Dispatcher) so orders can be acquired before submit() and
// released after terminal results are processed.
//
// Registry must be frozen before MatchingEngine is constructed.
class MatchingEngine {
public:
    using InstrumentFilter = std::function<bool(InstrumentId)>;

    explicit MatchingEngine(const InstrumentRegistry& registry,
                            OrderPool& pool,
                            InstrumentFilter filter = {});

    // Phase 4: acquire a zeroed Order slot from the shared pool.
    // Returns nullptr if the pool is exhausted.
    [[nodiscard]] Order* acquire_order() noexcept { return pool_.acquire(); }

    // Release a terminal taker Order back to the pool after the caller has
    // finished reading its fields (status, fills, etc.).
    void release_order(Order* o) noexcept { pool_.release(o); }

    SubmitResult submit(Order* order);
    bool cancel(InstrumentId instrument, OrderId id);

    [[nodiscard]] const OrderBook* book(InstrumentId id) const noexcept;
    [[nodiscard]] OrderBook*       book(InstrumentId id) noexcept;
    [[nodiscard]] std::size_t      book_count() const noexcept { return books_.size(); }
    [[nodiscard]] const InstrumentRegistry& registry() const noexcept { return registry_; }

    // Aggregate latency statistics across all instruments.
    LatencyStats::Snapshot latency_snapshot() const noexcept;

private:
    struct Slot {
        std::unique_ptr<OrderBook>   book;
        std::unique_ptr<BookMatcher> matcher;
    };

    const InstrumentRegistry&                       registry_;
    OrderPool&                                      pool_;
    std::unordered_map<std::uint32_t, Slot>         slots_;
    std::unordered_map<std::uint32_t, OrderBook*>   books_;
};

}  // namespace velox
