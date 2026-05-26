#include "velox/matching/sharded_engine.hpp"

#include <stdexcept>

namespace velox {

ShardedEngine::ShardedEngine(const InstrumentRegistry& registry,
                             std::size_t num_shards,
                             std::size_t pool_capacity)
    : pool_(pool_capacity), registry_(registry) {
    if (num_shards == 0) throw std::invalid_argument{"num_shards must be > 0"};
    if (!registry.frozen()) throw std::logic_error{"registry must be frozen"};

    shards_.reserve(num_shards);
    for (std::size_t i = 0; i < num_shards; ++i) {
        shards_.push_back(std::make_unique<Shard>());
    }
    for (std::size_t i = 0; i < num_shards; ++i) {
        const std::size_t my_shard = i;
        auto filter = [my_shard, num_shards](InstrumentId id) {
            return (to_underlying(id) % num_shards) == my_shard;
        };
        shards_[i]->engine = std::make_unique<MatchingEngine>(registry_, pool_, filter);
    }
    for (auto& s : shards_) {
        Shard* ptr = s.get();
        s->worker = std::thread{[this, ptr] { run_shard(*ptr); }};
    }
}

ShardedEngine::~ShardedEngine() {
    for (auto& s : shards_) s->stop.store(true, std::memory_order_release);
    for (auto& s : shards_) {
        if (s->worker.joinable()) s->worker.join();
    }
}

void ShardedEngine::run_shard(Shard& shard) {
    Command cmd;
    while (!shard.stop.load(std::memory_order_acquire)) {
        if (shard.queue.pop(cmd)) {
            if (cmd.kind == Command::Kind::Submit) {
                auto result = shard.engine->submit(cmd.order);
                // Release terminal takers back to the shared pool.
                if (result.order && result.order->is_terminal()) {
                    pool_.release(result.order);
                }
            } else {
                (void)shard.engine->cancel(cmd.instrument, cmd.order_id);
            }
            shard.processed.fetch_add(1, std::memory_order_release);
        } else {
            std::this_thread::yield();
        }
    }
    // Drain remaining commands on shutdown.
    while (shard.queue.pop(cmd)) {
        if (cmd.kind == Command::Kind::Submit) {
            auto result = shard.engine->submit(cmd.order);
            if (result.order && result.order->is_terminal()) {
                pool_.release(result.order);
            }
        } else {
            (void)shard.engine->cancel(cmd.instrument, cmd.order_id);
        }
        shard.processed.fetch_add(1, std::memory_order_release);
    }
}

bool ShardedEngine::submit(Order* order) {
    const std::size_t idx = shard_for(order->instrument);
    Shard& s = *shards_[idx];
    Command cmd{Command::Kind::Submit, order->instrument, order->id, order};
    if (!s.queue.push(cmd)) return false;
    s.submitted.fetch_add(1, std::memory_order_release);
    return true;
}

bool ShardedEngine::cancel(InstrumentId inst, OrderId id) {
    const std::size_t idx = shard_for(inst);
    Shard& s = *shards_[idx];
    Command cmd{Command::Kind::Cancel, inst, id, nullptr};
    if (!s.queue.push(cmd)) return false;
    s.submitted.fetch_add(1, std::memory_order_release);
    return true;
}

void ShardedEngine::wait_idle() {
    for (;;) {
        bool all_idle = true;
        for (auto& s : shards_) {
            const auto sub  = s->submitted.load(std::memory_order_acquire);
            const auto proc = s->processed.load(std::memory_order_acquire);
            if (sub != proc) { all_idle = false; break; }
        }
        if (all_idle) return;
        std::this_thread::yield();
    }
}

const OrderBook* ShardedEngine::book(InstrumentId id) const noexcept {
    const std::size_t idx = shard_for(id);
    return shards_[idx]->engine->book(id);
}

}  // namespace velox
