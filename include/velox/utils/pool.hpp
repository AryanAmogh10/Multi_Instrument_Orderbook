#pragma once

#include "velox/orderbook/order.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <numeric>
#include <vector>

namespace velox
{

// Fixed-size slab allocator for Order objects.
// Pre-allocates everything upfront so malloc never gets called on the hot path.
// acquire/release are mutex-protected — only happens at order entry/exit anyway.
//
// Caller must never delete Order* pointers directly; the pool owns the memory.
class Pool
{
public:
    explicit Pool(std::size_t capacity)
        : storage_(capacity), free_slots_(capacity),
          stack_top_(static_cast<std::uint32_t>(capacity))
    {
        assert(capacity > 0 && capacity <= 0xFFFF'FFFE);
        std::iota(free_slots_.begin(), free_slots_.end(), std::uint32_t{0});
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    // Returns a zeroed slot, or nullptr if we're full.
    [[nodiscard]] Order* acquire() noexcept
    {
        std::lock_guard lk{mu_};
        if (stack_top_ == 0)
            return nullptr;
        Order* o = &storage_[free_slots_[--stack_top_]];
        *o = Order{};
        return o;
    }

    // Give a slot back.
    void release(Order* o) noexcept
    {
        const auto idx = static_cast<std::uint32_t>(o - storage_.data());
        std::lock_guard lk{mu_};
        free_slots_[stack_top_++] = idx;
    }

    // Like acquire() but aborts if the pool is full — useful in tests.
    [[nodiscard]] Order* acquire_or_abort() noexcept
    {
        Order* o = acquire();
        if (!o)
            std::abort();
        return o;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return storage_.size(); }

    // Rough estimate only — not synchronized.
    [[nodiscard]] std::size_t available_approx() const noexcept { return stack_top_; }

private:
    std::vector<Order> storage_;
    std::vector<std::uint32_t> free_slots_;
    std::uint32_t stack_top_;
    mutable std::mutex mu_;
};

// TODO: maybe add a try_acquire_bulk() for batch order entry later
using OrderPool = Pool; // keep old name working for now

} // namespace velox
