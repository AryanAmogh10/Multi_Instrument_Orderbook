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

// Fixed-capacity pool of Order objects. Eliminates per-order heap allocation
// and shared_ptr atomic-refcount overhead on the critical matching path.
//
// Phase 4 §4.2: pool is pre-allocated at startup; acquire()/release() are
// protected by a mutex, which is acceptable because pool operations are off
// the hot matching loop (they happen at order entry/exit, not per tick).
//
// Lifetime: the pool OWNS all slab memory. Raw Order* returned by acquire()
// are valid until the pool is destroyed — callers must never delete them.
// release() marks a slot as reusable; the pool destructor frees the slab.
class OrderPool {
public:
    explicit OrderPool(std::size_t capacity)
        : slab_(capacity), free_(capacity), top_(static_cast<std::uint32_t>(capacity)) {
        assert(capacity > 0 && capacity <= 0xFFFF'FFFE);
        std::iota(free_.begin(), free_.end(), std::uint32_t{0});
    }

    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;

    // Acquire a zeroed Order from the pool. Returns nullptr if exhausted.
    [[nodiscard]] Order* acquire() noexcept {
        std::lock_guard lk{mu_};
        if (top_ == 0) return nullptr;
        Order* o = &slab_[free_[--top_]];
        *o = Order{};    // reset to clean state — no stale fields from prior use
        return o;
    }

    // Return an Order* (previously obtained via acquire()) to the pool.
    // Behaviour is undefined if `o` was not obtained from this pool.
    void release(Order* o) noexcept {
        const auto idx = static_cast<std::uint32_t>(o - slab_.data());
        std::lock_guard lk{mu_};
        free_[top_++] = idx;
    }

    // Like acquire(), but aborts on exhaustion.  Intended for tests and
    // benchmarks where the pool capacity should never be reached.
    [[nodiscard]] Order* acquire_or_abort() noexcept {
        Order* o = acquire();
        if (!o) std::abort();
        return o;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return slab_.size(); }

    // Approximate free count — not synchronized, for diagnostics only.
    [[nodiscard]] std::size_t available_approx() const noexcept { return top_; }

private:
    std::vector<Order>        slab_;
    std::vector<std::uint32_t> free_;
    std::uint32_t              top_;
    mutable std::mutex         mu_;
};

}  // namespace velox
