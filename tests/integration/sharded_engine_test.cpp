#include <gtest/gtest.h>

#include "ordbk/matching/sharded_engine.hpp"

#include <cassert>

using namespace ordbk;

namespace
{

InstrumentRegistry make_registry(std::uint32_t n)
{
    InstrumentRegistry r;
    for (std::uint32_t i = 1; i <= n; ++i)
    {
        r.add(InstrumentSpec{
            InstrumentId{i}, "S" + std::to_string(i), InstrumentType::Equity, 1, 1, "USD"});
    }
    r.freeze();
    return r;
}

// Orders are acquired from the ShardedEngine's own pool.
Order* mk(ShardedEngine& eng,
          std::uint64_t id,
          InstrumentId inst,
          Side side,
          std::int64_t price,
          std::uint64_t qty)
{
    Order* o = eng.acquire_order();
    assert(o && "sharded engine pool exhausted");
    *o = Order{
        OrderId{id},
        Price{price},
        Quantity{qty},
        kZeroQty,
        inst,
        ClientId{1},
        OrderStatus::New,
        side,
        OrderType::Limit,
        TimeInForce::GTC,
        Timestamp{0},
    };
    return o;
}

} // namespace

TEST(ShardedEngine, ConstructsAndShutsDown)
{
    auto r = make_registry(4);
    ShardedEngine eng{r, 2};
    EXPECT_EQ(eng.num_shards(), 2u);
    eng.wait_idle();
}

TEST(ShardedEngine, RejectsZeroShards)
{
    auto r = make_registry(1);
    EXPECT_THROW((ShardedEngine{r, 0}), std::invalid_argument);
}

TEST(ShardedEngine, ConsistentHashingAssignsInstruments)
{
    auto r = make_registry(8);
    ShardedEngine eng{r, 4};
    for (std::uint32_t i = 1; i <= 8; ++i)
    {
        const auto inst = InstrumentId{i};
        EXPECT_EQ(eng.shard_for(inst), to_underlying(inst) % 4u);
        EXPECT_NE(eng.book(inst), nullptr);
    }
}

TEST(ShardedEngine, SingleSubmitMatches)
{
    auto r = make_registry(2);
    ShardedEngine eng{r, 2};
    EXPECT_TRUE(eng.submit(mk(eng, 1, InstrumentId{1}, Side::Buy, 100, 5)));
    eng.wait_idle();
    EXPECT_EQ(*eng.book(InstrumentId{1})->best_bid(), Price{100});
}

TEST(ShardedEngine, CrossMatchesAcrossShards)
{
    auto r = make_registry(4);
    ShardedEngine eng{r, 4};
    eng.submit(mk(eng, 1, InstrumentId{1}, Side::Sell, 100, 5));
    eng.submit(mk(eng, 2, InstrumentId{2}, Side::Sell, 200, 5));
    eng.submit(mk(eng, 3, InstrumentId{1}, Side::Buy, 100, 5));
    eng.submit(mk(eng, 4, InstrumentId{2}, Side::Buy, 200, 5));
    eng.wait_idle();
    EXPECT_TRUE(eng.book(InstrumentId{1})->empty());
    EXPECT_TRUE(eng.book(InstrumentId{2})->empty());
}

TEST(ShardedEngine, ManyOrdersPreserveCount)
{
    auto r = make_registry(4);
    ShardedEngine eng{r, 2};
    constexpr std::uint64_t kPerInst = 200;
    std::uint64_t next_id = 1;
    for (std::uint32_t inst = 1; inst <= 4; ++inst)
    {
        for (std::uint64_t i = 0; i < kPerInst; ++i)
        {
            const std::int64_t price = 100 + static_cast<std::int64_t>(i);
            eng.submit(mk(eng, next_id++, InstrumentId{inst}, Side::Buy, price, 1));
        }
    }
    eng.wait_idle();
    for (std::uint32_t inst = 1; inst <= 4; ++inst)
    {
        EXPECT_EQ(eng.book(InstrumentId{inst})->order_count(), kPerInst);
    }
}

TEST(ShardedEngine, CancelRoutesCorrectly)
{
    auto r = make_registry(2);
    ShardedEngine eng{r, 2};
    eng.submit(mk(eng, 1, InstrumentId{1}, Side::Buy, 100, 5));
    eng.submit(mk(eng, 2, InstrumentId{2}, Side::Buy, 200, 5));
    eng.wait_idle();
    eng.cancel(InstrumentId{1}, OrderId{1});
    eng.wait_idle();
    EXPECT_TRUE(eng.book(InstrumentId{1})->empty());
    EXPECT_FALSE(eng.book(InstrumentId{2})->empty());
}

TEST(ShardedEngine, SingleShardWorks)
{
    auto r = make_registry(3);
    ShardedEngine eng{r, 1};
    eng.submit(mk(eng, 1, InstrumentId{1}, Side::Sell, 100, 5));
    eng.submit(mk(eng, 2, InstrumentId{1}, Side::Buy, 100, 5));
    eng.wait_idle();
    EXPECT_TRUE(eng.book(InstrumentId{1})->empty());
}
