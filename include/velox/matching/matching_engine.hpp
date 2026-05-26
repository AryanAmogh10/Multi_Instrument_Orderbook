#pragma once

#include "velox/instruments/instrument_registry.hpp"
#include "velox/matching/book_matcher.hpp"
#include "velox/utils/order_pool.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace velox {

// Owns one OrderBook + BookMatcher per registered instrument and routes
// incoming orders to the right book by InstrumentId.
//
// Single-threaded — concurrency is handled by ShardedEngine which runs one
// instance of this per worker thread, each owning a disjoint set of instruments.
//
// The registry must be frozen before this class is constructed.
class MatchingEngine {
public:
    using InstrumentFilter = std::function<bool(InstrumentId)>;

    explicit MatchingEngine(const InstrumentRegistry& registry,
                            OrderPool& pool,
                            InstrumentFilter filter = {});

    // Borrow an Order slot from the shared pool. Returns nullptr if full.
    [[nodiscard]] Order* acquire_order() noexcept { return pool_.acquire(); }

    // Return a terminal taker Order to the pool after reading its result fields.
    void release_order(Order* o) noexcept { pool_.release(o); }

    SubmitResult submit(Order* order);
    bool cancel(InstrumentId instrument, OrderId id);

    // Cancel all resting orders and remove the instrument from active trading.
    // Returns false if the instrument isn't managed by this engine.
    bool expire_instrument(InstrumentId id);

    [[nodiscard]] const OrderBook* book(InstrumentId id) const noexcept;
    [[nodiscard]] OrderBook*       book(InstrumentId id) noexcept;
    [[nodiscard]] std::size_t      book_count() const noexcept { return books_.size(); }
    [[nodiscard]] const InstrumentRegistry& registry() const noexcept { return registry_; }

    LatencyStats::Snapshot latency_snapshot() const noexcept;

private:
    struct Slot {
        std::unique_ptr<OrderBook>   book;
        std::unique_ptr<BookMatcher> matcher;
    };

    const InstrumentRegistry&                     registry_;
    OrderPool&                                    pool_;
    std::unordered_map<std::uint32_t, Slot>       slots_;
    std::unordered_map<std::uint32_t, OrderBook*> books_;
};

}  // namespace velox
