#pragma once

#include "velox/orderbook/order.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <vector>

namespace velox {

// Fixed-capacity slab allocator for Order objects.
// Pre-allocates everything at startup so we never call malloc on the critical path.
// acquire() / release() are mutex-protected — this is fine because they only run
// at order entry/exit, not inside the inner matching loop.
//
// Ownership: the pool owns all memory. Order* pointers returned by acquire() stay
// valid until the pool is destroyed — callers must never delete them directly.
class OrderPool {
public:
    explicit OrderPool(std::size_t capacity)
        : slab_(capacity), free_(capacity), top_(static_cast<std::uint32_t>(capacity)) {
        assert(capacity > 0 && capacity <= 0xFFFF'FFFE);
        std::iota(free_.begin(), free_.end(), std::uint32_t{0});
    }

    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;

    // Grab a zeroed Order slot. Returns nullptr if the pool is full.
    [[nodiscard]] Order* acquire() noexcept {
        std::lock_guard lk{mu_};
        if (top_ == 0) return nullptr;
        Order* o = &slab_[free_[--top_]];
        *o = Order{};
        return o;
    }

    // Return a slot obtained from acquire() back to the pool.
    void release(Order* o) noexcept {
        const auto idx = static_cast<std::uint32_t>(o - slab_.data());
        std::lock_guard lk{mu_};
        free_[top_++] = idx;
    }

    // Same as acquire() but calls abort() if the pool is exhausted.
    // Use in tests and benchmarks where running out is a bug, not a runtime condition.
    [[nodiscard]] Order* acquire_or_abort() noexcept {
        Order* o = acquire();
        if (!o) std::abort();
        return o;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return slab_.size(); }

    // Not synchronized — only useful for diagnostics/logging.
    [[nodiscard]] std::size_t available_approx() const noexcept { return top_; }

private:
    std::vector<Order>         slab_;
    std::vector<std::uint32_t> free_;
    std::uint32_t              top_;
    mutable std::mutex         mu_;
};

}  // namespace velox
