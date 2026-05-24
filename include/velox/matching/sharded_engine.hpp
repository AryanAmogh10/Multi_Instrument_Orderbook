#pragma once

#include "velox/instruments/instrument_registry.hpp"
#include "velox/matching/matching_engine.hpp"
#include "velox/orderbook/orderbook.hpp"
#include "velox/utils/spsc_ring_buffer.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace velox {

// Sharded matching engine — N worker threads, each owns a disjoint subset of
// instruments selected by consistent hashing (instrument_id % N).
//
// Per-shard ingestion uses an SPSC lock-free queue. Phase 2 assumes a single
// producer thread for all submit()/cancel() calls; multi-producer dispatch
// arrives with the gateway in Phase 3.
class ShardedEngine {
public:
    static constexpr std::size_t kQueueCapacity = 4096;

    struct Command {
        enum class Kind : std::uint8_t { Submit, Cancel } kind;
        InstrumentId         instrument;
        OrderId              order_id;       // Cancel
        OrderBook::OrderPtr  order;          // Submit
    };

    ShardedEngine(const InstrumentRegistry& registry, std::size_t num_shards);
    ~ShardedEngine();

    ShardedEngine(const ShardedEngine&) = delete;
    ShardedEngine& operator=(const ShardedEngine&) = delete;

    bool submit(OrderBook::OrderPtr order);
    bool cancel(InstrumentId inst, OrderId id);

    // Block until every previously enqueued command has been processed and
    // all shard queues are empty. Safe only when no concurrent producer is
    // still enqueuing.
    void wait_idle();

    [[nodiscard]] std::size_t num_shards() const noexcept { return shards_.size(); }
    [[nodiscard]] std::size_t shard_for(InstrumentId id) const noexcept {
        return to_underlying(id) % shards_.size();
    }

    // Safe to call only after wait_idle() and with no concurrent producer.
    [[nodiscard]] const OrderBook* book(InstrumentId id) const noexcept;

private:
    struct Shard {
        SpscRingBuffer<Command, kQueueCapacity>  queue;
        std::unique_ptr<MatchingEngine>          engine;
        std::atomic<std::uint64_t>               submitted{0};
        std::atomic<std::uint64_t>               processed{0};
        std::atomic<bool>                        stop{false};
        std::thread                              worker;
    };

    static void run_shard(Shard& shard);

    std::vector<std::unique_ptr<Shard>>  shards_;
    const InstrumentRegistry&            registry_;
};

}  // namespace velox
