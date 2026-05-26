#include "velox/matching/sharded_engine.hpp"

#include <stdexcept>

namespace velox {

ShardedMatcher::ShardedMatcher(const InstrumentRegistry& registry,
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
        shards_[i]->engine = std::make_unique<Engine>(registry_, pool_, filter);
    }
    for (auto& s : shards_) {
        Shard* ptr = s.get();
        s->worker = std::thread{[this, ptr] { run_shard(*ptr); }};
    }
}

ShardedMatcher::~ShardedMatcher() {
    for (auto& s : shards_) s->stop.store(true, std::memory_order_release);
    for (auto& s : shards_) {
        if (s->worker.joinable()) s->worker.join();
    }
}

void ShardedMatcher::run_shard(Shard& shard) {
    Command cmd;
    while (!shard.stop.load(std::memory_order_acquire)) {
        if (shard.queue.pop(cmd)) {
            if (cmd.kind == Command::Kind::Submit) {
                auto result = shard.engine->submit(cmd.order);
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
    // drain leftovers before exit
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

bool ShardedMatcher::submit(Order* order) {
    const std::size_t idx = shard_for(order->instrument);
    Shard& s = *shards_[idx];
    Command cmd{Command::Kind::Submit, order->instrument, order->id, order};
    if (!s.queue.push(cmd)) return false;
    s.submitted.fetch_add(1, std::memory_order_release);
    return true;
}

bool ShardedMatcher::cancel(InstrumentId inst, OrderId id) {
    const std::size_t idx = shard_for(inst);
    Shard& s = *shards_[idx];
    Command cmd{Command::Kind::Cancel, inst, id, nullptr};
    if (!s.queue.push(cmd)) return false;
    s.submitted.fetch_add(1, std::memory_order_release);
    return true;
}

void ShardedMatcher::wait_idle() {
    for (;;) {
        bool all_done = true;
        for (auto& s : shards_) {
            if (s->submitted.load(std::memory_order_acquire) !=
                s->processed.load(std::memory_order_acquire)) {
                all_done = false;
                break;
            }
        }
        if (all_done) return;
        std::this_thread::yield();
    }
}

const OrderBook* ShardedMatcher::book(InstrumentId id) const noexcept {
    const std::size_t idx = shard_for(id);
    return shards_[idx]->engine->book(id);
}

}  // namespace velox
