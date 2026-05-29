#include <gtest/gtest.h>

#include "ordbk/orderbook/orderbook.hpp"
#include "ordbk/utils/order_pool.hpp"

#include <cassert>

using namespace ordbk;

namespace
{

constexpr InstrumentId kInst{1};

// Pool shared across all tests in this file. 256 slots is more than enough.
OrderPool g_pool{256};

Order* mk(std::uint64_t id, Side side, std::int64_t price, std::uint64_t qty)
{
    Order* o = g_pool.acquire_or_abort();
    assert(o && "orderbook test pool exhausted — increase g_pool size");
    *o = Order{
        OrderId{id},
        Price{price},
        Quantity{qty},
        kZeroQty,
        kInst,
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

TEST(OrderBook, StartsEmpty)
{
    OrderBook ob{kInst};
    EXPECT_TRUE(ob.empty());
    EXPECT_EQ(ob.order_count(), 0u);
    EXPECT_FALSE(ob.best_bid().has_value());
    EXPECT_FALSE(ob.best_ask().has_value());
}

TEST(OrderBook, AddSingleBid)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 5));
    ASSERT_TRUE(ob.best_bid().has_value());
    EXPECT_EQ(*ob.best_bid(), Price{100});
    EXPECT_EQ(ob.order_count(), 1u);
}

TEST(OrderBook, AddSingleAsk)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Sell, 100, 5));
    ASSERT_TRUE(ob.best_ask().has_value());
    EXPECT_EQ(*ob.best_ask(), Price{100});
}

TEST(OrderBook, BestBidIsHighest)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 5));
    ob.add_resting(mk(2, Side::Buy, 105, 5));
    ob.add_resting(mk(3, Side::Buy, 95, 5));
    EXPECT_EQ(*ob.best_bid(), Price{105});
}

TEST(OrderBook, BestAskIsLowest)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Sell, 110, 5));
    ob.add_resting(mk(2, Side::Sell, 108, 5));
    ob.add_resting(mk(3, Side::Sell, 115, 5));
    EXPECT_EQ(*ob.best_ask(), Price{108});
}

TEST(OrderBook, AggregatedQtyAtLevel)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 5));
    ob.add_resting(mk(2, Side::Buy, 100, 7));
    ob.add_resting(mk(3, Side::Buy, 100, 3));
    EXPECT_EQ(to_underlying(ob.bid_qty_at(Price{100})), 15u);
}

TEST(OrderBook, QtyAtMissingLevelIsZero)
{
    OrderBook ob{kInst};
    EXPECT_EQ(to_underlying(ob.bid_qty_at(Price{100})), 0u);
    EXPECT_EQ(to_underlying(ob.ask_qty_at(Price{100})), 0u);
}

TEST(OrderBook, FindReturnsSameObject)
{
    OrderBook ob{kInst};
    Order* o = mk(42, Side::Buy, 100, 5);
    ob.add_resting(o);
    EXPECT_EQ(ob.find(OrderId{42}), o); // both are Order*
}

TEST(OrderBook, FindMissingReturnsNull)
{
    OrderBook ob{kInst};
    EXPECT_EQ(ob.find(OrderId{999}), nullptr);
}

TEST(OrderBook, CancelExisting)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 5));
    EXPECT_TRUE(ob.cancel(OrderId{1}));
    EXPECT_TRUE(ob.empty());
    EXPECT_FALSE(ob.best_bid().has_value());
}

TEST(OrderBook, CancelMissingReturnsFalse)
{
    OrderBook ob{kInst};
    EXPECT_FALSE(ob.cancel(OrderId{999}));
}

TEST(OrderBook, CancelCollapsesLevelOnlyWhenEmpty)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 5));
    ob.add_resting(mk(2, Side::Buy, 100, 7));
    ob.cancel(OrderId{1});
    EXPECT_EQ(*ob.best_bid(), Price{100});
    EXPECT_EQ(to_underlying(ob.bid_qty_at(Price{100})), 7u);
    ob.cancel(OrderId{2});
    EXPECT_FALSE(ob.best_bid().has_value());
}

TEST(OrderBook, PeekTopReturnsFrontOfBestLevel)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 5));
    ob.add_resting(mk(2, Side::Buy, 100, 7));
    ob.add_resting(mk(3, Side::Buy, 105, 3));
    EXPECT_EQ(ob.peek_top(Side::Buy)->id, OrderId{3});
}

TEST(OrderBook, PopTopRemovesAndPreservesPriority)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 5));
    ob.add_resting(mk(2, Side::Buy, 100, 7));
    ob.pop_top(Side::Buy);
    EXPECT_EQ(ob.peek_top(Side::Buy)->id, OrderId{2});
    EXPECT_EQ(ob.order_count(), 1u);
}

TEST(OrderBook, MultipleBidLevelsOrderedDescending)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Buy, 100, 1));
    ob.add_resting(mk(2, Side::Buy, 102, 1));
    ob.add_resting(mk(3, Side::Buy, 101, 1));
    EXPECT_EQ(ob.bids().begin()->first, Price{102});
    EXPECT_EQ(std::next(ob.bids().begin())->first, Price{101});
}

TEST(OrderBook, MultipleAskLevelsOrderedAscending)
{
    OrderBook ob{kInst};
    ob.add_resting(mk(1, Side::Sell, 100, 1));
    ob.add_resting(mk(2, Side::Sell, 102, 1));
    ob.add_resting(mk(3, Side::Sell, 101, 1));
    EXPECT_EQ(ob.asks().begin()->first, Price{100});
    EXPECT_EQ(std::next(ob.asks().begin())->first, Price{101});
}

TEST(OrderBook, PeekTopEmptyReturnsNull)
{
    OrderBook ob{kInst};
    EXPECT_EQ(ob.peek_top(Side::Buy), nullptr);
    EXPECT_EQ(ob.peek_top(Side::Sell), nullptr);
}
