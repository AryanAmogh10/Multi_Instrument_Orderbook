#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace ordbk
{

// Grab current time in nanoseconds (steady_clock).
[[nodiscard]] inline std::uint64_t now_ns() noexcept
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

// Lock-free latency recorder. Tracks count/sum/max and a 5-bucket histogram.
//
// Buckets (ns): <1µs, 1-10µs, 10-100µs, 100µs-1ms, >=1ms
class LatencyTracker
{
public:
    static constexpr std::size_t kBuckets = 5;

    void record(std::uint64_t ns) noexcept
    {
        count_.fetch_add(1, std::memory_order_relaxed);
        sum_ns_.fetch_add(ns, std::memory_order_relaxed);

        // CAS loop to track running max without locking
        auto cur = max_ns_.load(std::memory_order_relaxed);
        while (ns > cur)
        {
            if (max_ns_.compare_exchange_weak(cur, ns, std::memory_order_relaxed))
                break;
        }

        hist_[bucket(ns)].fetch_add(1, std::memory_order_relaxed);
    }

    struct Snapshot
    {
        std::uint64_t count{0};
        std::uint64_t mean_ns{0};
        std::uint64_t max_ns{0};
        std::array<std::uint64_t, kBuckets> hist{};
    };

    [[nodiscard]] Snapshot snapshot() const noexcept
    {
        Snapshot s;
        s.count = count_.load(std::memory_order_relaxed);
        auto sum = sum_ns_.load(std::memory_order_relaxed);
        s.mean_ns = s.count ? sum / s.count : 0;
        s.max_ns = max_ns_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < kBuckets; ++i)
            s.hist[i] = hist_[i].load(std::memory_order_relaxed);
        return s;
    }

    void reset() noexcept
    {
        count_.store(0, std::memory_order_relaxed);
        sum_ns_.store(0, std::memory_order_relaxed);
        max_ns_.store(0, std::memory_order_relaxed);
        for (auto& h : hist_)
            h.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr std::uint64_t kThresholds[kBuckets - 1] = {1'000, 10'000, 100'000, 1'000'000};

    [[nodiscard]] static std::size_t bucket(std::uint64_t ns) noexcept
    {
        for (std::size_t i = 0; i < kBuckets - 1; ++i)
            if (ns < kThresholds[i])
                return i;
        return kBuckets - 1;
    }

    std::atomic<std::uint64_t> count_{0};
    std::atomic<std::uint64_t> sum_ns_{0};
    std::atomic<std::uint64_t> max_ns_{0};
    std::array<std::atomic<std::uint64_t>, kBuckets> hist_{};
};

// keep old name around so existing code still compiles
using LatencyStats = LatencyTracker;

} // namespace ordbk
