#include <gtest/gtest.h>

#include "velox/utils/spsc_ring_buffer.hpp"

#include <atomic>
#include <thread>
#include <vector>

using namespace velox;

TEST(Spsc, EmptyOnConstruction) {
    SpscRingBuffer<int, 8> q;
    EXPECT_TRUE(q.empty());
    int v{};
    EXPECT_FALSE(q.pop(v));
}

TEST(Spsc, PushPopSingle) {
    SpscRingBuffer<int, 8> q;
    EXPECT_TRUE(q.push(42));
    int v{};
    EXPECT_TRUE(q.pop(v));
    EXPECT_EQ(v, 42);
    EXPECT_TRUE(q.empty());
}

TEST(Spsc, FIFOOrder) {
    SpscRingBuffer<int, 8> q;
    for (int i = 0; i < 5; ++i) q.push(i);
    for (int i = 0; i < 5; ++i) {
        int v{};
        ASSERT_TRUE(q.pop(v));
        EXPECT_EQ(v, i);
    }
}

TEST(Spsc, FillsToCapacityMinusOne) {
    SpscRingBuffer<int, 4> q;  // usable capacity = 3
    EXPECT_EQ(q.capacity(), 3u);
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));
    EXPECT_FALSE(q.push(4));  // full
}

TEST(Spsc, WrapsAround) {
    SpscRingBuffer<int, 4> q;
    int v{};
    for (int round = 0; round < 10; ++round) {
        EXPECT_TRUE(q.push(round * 10 + 1));
        EXPECT_TRUE(q.push(round * 10 + 2));
        EXPECT_TRUE(q.pop(v));
        EXPECT_EQ(v, round * 10 + 1);
        EXPECT_TRUE(q.pop(v));
        EXPECT_EQ(v, round * 10 + 2);
    }
    EXPECT_TRUE(q.empty());
}

TEST(Spsc, ApproxSizeTracksOccupancy) {
    SpscRingBuffer<int, 8> q;
    EXPECT_EQ(q.approx_size(), 0u);
    q.push(1); q.push(2); q.push(3);
    EXPECT_EQ(q.approx_size(), 3u);
    int v{};
    q.pop(v);
    EXPECT_EQ(q.approx_size(), 2u);
}

TEST(Spsc, MoveOnlyType) {
    SpscRingBuffer<std::unique_ptr<int>, 8> q;
    EXPECT_TRUE(q.push(std::make_unique<int>(7)));
    std::unique_ptr<int> out;
    EXPECT_TRUE(q.pop(out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, 7);
}

TEST(Spsc, ProducerConsumerNoLoss) {
    constexpr int kCount = 100000;
    SpscRingBuffer<int, 1024> q;

    std::atomic<int> consumed_sum{0};
    std::thread consumer{[&] {
        int seen = 0;
        int v{};
        long long sum = 0;
        while (seen < kCount) {
            if (q.pop(v)) {
                sum += v;
                ++seen;
            } else {
                std::this_thread::yield();
            }
        }
        consumed_sum.store(static_cast<int>(sum));
    }};

    std::thread producer{[&] {
        for (int i = 1; i <= kCount; ++i) {
            while (!q.push(i)) std::this_thread::yield();
        }
    }};

    producer.join();
    consumer.join();
    // Sum 1..N = N(N+1)/2
    constexpr long long expected = static_cast<long long>(kCount) * (kCount + 1) / 2;
    EXPECT_EQ(consumed_sum.load(), static_cast<int>(expected));
}
