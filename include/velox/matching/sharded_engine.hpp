#pragma once

#include "velox/instruments/instrument_registry.hpp"
#include "velox/matching/matching_engine.hpp"
#include "velox/orderbook/orderbook.hpp"
#include "velox/utils/order_pool.hpp"
#include "velox/utils/spsc_ring_buffer.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace velox {

// Multi-threaded front-end. N worker threads each own a disjoint subset of
// instruments selected by (instrument_id % N). Incoming orders are routed to
// the right shard via a per-shard SPSC queue.
//
// All threads share a single OrderPool. Callers must use acquire_order() to
// obtain an Order* before calling submit(); terminal takers are released
// automatically by the worker.
class ShardedEngine {
public:
    static constexpr std::size_t kQueueCapacity  = 4096;
    static constexpr std::size_t kDefaultPoolSize = 65'536;

    struct Command {
        enum class Kind : std::uint8_t { Submit, Cancel } kind;
        InstrumentId  instrument;
        OrderId       order_id;   // used for Cancel
        Order*        order;      // used for Submit
    };

    ShardedEngine(const InstrumentRegistry& registry,
                  std::size_t num_shards,
                  std::size_t pool_capacity = kDefaultPoolSize);
    ~ShardedEngine();

    ShardedEngine(const ShardedEngine&) = delete;
    ShardedEngine& operator=(const ShardedEngine&) = delete;

    [[nodiscard]] Order* acquire_order() noexcept { return pool_.acquire(); }

    bool submit(Order* order);
    bool cancel(InstrumentId inst, OrderId id);

    // Block until all previously enqueued commands have been processed.
    // Must not be called while a producer is still enqueuing.
    void wait_idle();

    [[nodiscard]] std::size_t num_shards() const noexcept { return shards_.size(); }
    [[nodiscard]] std::size_t shard_for(InstrumentId id) const noexcept {
        return to_underlying(id) % shards_.size();
    }

    // Only safe to call after wait_idle() with no concurrent producer.
    [[nodiscard]] const OrderBook* book(InstrumentId id) const noexcept;

private:
    struct Shard {
        SpscRingBuffer<Command, kQueueCapacity> queue;
        std::unique_ptr<MatchingEngine>         engine;
        std::atomic<std::uint64_t>              submitted{0};
        std::atomic<std::uint64_t>              processed{0};
        std::atomic<bool>                       stop{false};
        std::thread                             worker;
    };

    void run_shard(Shard& shard);

    OrderPool                            pool_;
    std::vector<std::unique_ptr<Shard>>  shards_;
    const InstrumentRegistry&            registry_;
};

}  // namespace velox
