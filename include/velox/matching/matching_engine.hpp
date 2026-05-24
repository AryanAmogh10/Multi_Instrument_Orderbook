#pragma once

#include "velox/instruments/instrument_registry.hpp"
#include "velox/matching/book_matcher.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace velox {

// Orchestrator: owns one OrderBook + one BookMatcher per registered instrument,
// routes incoming orders by InstrumentId. Single-threaded (not thread-safe);
// concurrency comes from ShardedEngine which composes one of these per shard.
//
// The registry must be frozen before MatchingEngine is constructed.
class MatchingEngine {
public:
    // Builds books for every instrument in `registry` whose id satisfies
    // `predicate(id)`. The default predicate accepts all instruments.
    using InstrumentFilter = std::function<bool(InstrumentId)>;

    explicit MatchingEngine(const InstrumentRegistry& registry,
                            InstrumentFilter filter = {});

    SubmitResult submit(OrderBook::OrderPtr order);
    bool cancel(InstrumentId instrument, OrderId id);

    [[nodiscard]] const OrderBook* book(InstrumentId id) const noexcept;
    [[nodiscard]] OrderBook* book(InstrumentId id) noexcept;
    [[nodiscard]] std::size_t book_count() const noexcept { return books_.size(); }
    [[nodiscard]] const InstrumentRegistry& registry() const noexcept { return registry_; }

private:
    struct Slot {
        std::unique_ptr<OrderBook>   book;
        std::unique_ptr<BookMatcher> matcher;
    };

    const InstrumentRegistry&                       registry_;
    std::unordered_map<std::uint32_t, Slot>         slots_;
    // Stable view of books for iteration (parallel to slots_).
    std::unordered_map<std::uint32_t, OrderBook*>   books_;
};

}  // namespace velox
