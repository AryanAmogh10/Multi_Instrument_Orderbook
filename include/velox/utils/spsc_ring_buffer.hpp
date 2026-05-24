#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

namespace velox {

// Single-producer, single-consumer wait-free ring buffer.
//
// Capacity must be a power of two; one slot is reserved to distinguish empty
// from full, so usable capacity is Capacity - 1.
//
// Producer thread calls push() exclusively; consumer thread calls pop()
// exclusively. Calling from more than one producer or consumer is undefined.
template <class T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity >= 2, "Capacity must be >= 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    SpscRingBuffer() = default;
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    bool push(T value) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) & kMask;
        if (next == head_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[tail] = std::move(value);
        tail_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        out = std::move(buffer_[head]);
        head_.store((head + 1) & kMask, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t approx_size() const noexcept {
        const std::size_t t = tail_.load(std::memory_order_acquire);
        const std::size_t h = head_.load(std::memory_order_acquire);
        return (t - h) & kMask;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    // Hardcoded to 64 — matches x86_64, ARM64, and most modern CPUs. We avoid
    // std::hardware_destructive_interference_size because GCC warns it changes
    // between -mtune values and isn't ABI-stable.
    static constexpr std::size_t kCacheLine = 64;

    alignas(kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
    alignas(kCacheLine) std::array<T, Capacity>  buffer_{};
};

}  // namespace velox
