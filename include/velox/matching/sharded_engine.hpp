#pragma once

#include "velox/instruments/instrument_registry.hpp"
#include "velox/matching/engine.hpp"
#include "velox/orderbook/orderbook.hpp"
#include "velox/utils/pool.hpp"
#include "velox/utils/spsc_ring_buffer.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace velox
{

// Multi-threaded front-end. N worker threads each own a disjoint subset of
// instruments (instrument_id % N). Orders are routed via per-shard SPSC queues.
//
// All threads share one Pool. Use acquire_order() before submit().
// Terminal takers are released automatically by the worker.
class ShardedMatcher
{
public:
    static constexpr std::size_t kQueueCapacity = 4096;
    static constexpr std::size_t kDefaultPoolSize = 65'536;

    struct Command
    {
        enum class Kind : std::uint8_t
        {
            Submit,
            Cancel
        } kind;
        InstrumentId instrument;
        OrderId order_id; // for Cancel
        Order* order;     // for Submit
    };

    ShardedMatcher(const InstrumentRegistry& registry,
                   std::size_t num_shards,
                   std::size_t pool_capacity = kDefaultPoolSize);
    ~ShardedMatcher();

    ShardedMatcher(const ShardedMatcher&) = delete;
    ShardedMatcher& operator=(const ShardedMatcher&) = delete;

    [[nodiscard]] Order* acquire_order() noexcept { return pool_.acquire(); }

    bool submit(Order* order);
    bool cancel(InstrumentId inst, OrderId id);

    // Block until all previously enqueued commands are done.
    // Don't call while a producer is still sending.
    void wait_idle();

    [[nodiscard]] std::size_t num_shards() const noexcept { return shards_.size(); }
    [[nodiscard]] std::size_t shard_for(InstrumentId id) const noexcept
    {
        return to_underlying(id) % shards_.size();
    }

    // Only safe after wait_idle() with no concurrent producer.
    [[nodiscard]] const OrderBook* book(InstrumentId id) const noexcept;

private:
    struct Shard
    {
        SpscRingBuffer<Command, kQueueCapacity> queue;
        std::unique_ptr<Engine> engine;
        std::atomic<std::uint64_t> submitted{0};
        std::atomic<std::uint64_t> processed{0};
        std::atomic<bool> stop{false};
        std::thread worker;
    };

    void run_shard(Shard& shard);

    Pool pool_;
    std::vector<std::unique_ptr<Shard>> shards_;
    const InstrumentRegistry& registry_;
};

// keep old name working
using ShardedEngine = ShardedMatcher;

} // namespace velox
