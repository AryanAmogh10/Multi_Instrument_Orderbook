#include <gtest/gtest.h>

#include "ordbk/matching/book_matcher.hpp"
#include "ordbk/utils/order_pool.hpp"

#include <cassert>

using namespace ordbk;

namespace
{

constexpr InstrumentId kInst{1};

// Pool shared across all tests in this file.
OrderPool g_pool{512};

Order* mk(std::uint64_t id,
          Side side,
          std::int64_t price,
          std::uint64_t qty,
          OrderType type = OrderType::Limit,
          TimeInForce tif = TimeInForce::GTC)
{
    Order* o = g_pool.acquire_or_abort();
    assert(o && "matching test pool exhausted — increase g_pool size");
    *o = Order{
        OrderId{id},
        Price{price},
        Quantity{qty},
        kZeroQty,
        kInst,
        ClientId{1},
        OrderStatus::New,
        side,
        type,
        tif,
        Timestamp{0},
    };
    return o;
}

} // namespace

// ---- validation -----------------------------------------------------------

TEST(Matching, RejectsZeroQuantity)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    auto r = me.submit(mk(1, Side::Buy, 100, 0));
    EXPECT_EQ(r.order->status, OrderStatus::Rejected);
    EXPECT_TRUE(r.trades.empty());
}

TEST(Matching, RejectsNonPositiveLimitPrice)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    EXPECT_EQ(me.submit(mk(1, Side::Buy, 0, 5)).order->status, OrderStatus::Rejected);
    EXPECT_EQ(me.submit(mk(2, Side::Buy, -1, 5)).order->status, OrderStatus::Rejected);
}

TEST(Matching, RejectsInstrumentMismatch)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    auto o = mk(1, Side::Buy, 100, 5);
    o->instrument = InstrumentId{99};
    EXPECT_EQ(me.submit(o).order->status, OrderStatus::Rejected);
}

TEST(Matching, RejectsMarketWithGTC)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    auto o = mk(1, Side::Buy, 0, 5, OrderType::Market, TimeInForce::GTC);
    EXPECT_EQ(me.submit(o).order->status, OrderStatus::Rejected);
}

// ---- limit, no cross ------------------------------------------------------

TEST(Matching, NonCrossingBidRests)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    auto r = me.submit(mk(1, Side::Buy, 100, 5));
    EXPECT_EQ(r.order->status, OrderStatus::New);
    EXPECT_EQ(*ob.best_bid(), Price{100});
    EXPECT_TRUE(r.trades.empty());
}

TEST(Matching, NonCrossingAskRests)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Buy, 100, 5));
    auto r = me.submit(mk(2, Side::Sell, 105, 5));
    EXPECT_EQ(r.order->status, OrderStatus::New);
    EXPECT_EQ(*ob.best_ask(), Price{105});
    EXPECT_TRUE(r.trades.empty());
}

// ---- limit, crossing ------------------------------------------------------

TEST(Matching, FullFillAtMakerPrice)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 5));
    auto r = me.submit(mk(2, Side::Buy, 100, 5));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].price, Price{100});
    EXPECT_EQ(to_underlying(r.trades[0].quantity), 5u);
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
    EXPECT_TRUE(ob.empty());
}

TEST(Matching, AggressiveBuyerCrossesUpwards)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 5));
    auto r = me.submit(mk(2, Side::Buy, 110, 5));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].price, Price{100}); // maker price wins
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
}

TEST(Matching, AggressiveSellerCrossesDownwards)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Buy, 100, 5));
    auto r = me.submit(mk(2, Side::Sell, 90, 5));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].price, Price{100});
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
}

TEST(Matching, PartialMakerLeavesRemainder)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 10));
    auto r = me.submit(mk(2, Side::Buy, 100, 3));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(to_underlying(r.trades[0].quantity), 3u);
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
    EXPECT_EQ(to_underlying(ob.ask_qty_at(Price{100})), 7u);
    auto maker = ob.find(OrderId{1});
    ASSERT_NE(maker, nullptr);
    EXPECT_EQ(maker->status, OrderStatus::PartiallyFilled);
}

TEST(Matching, PartialTakerRestsRemainderForGTC)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 3));
    auto r = me.submit(mk(2, Side::Buy, 100, 10));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.order->status, OrderStatus::PartiallyFilled);
    EXPECT_EQ(to_underlying(ob.bid_qty_at(Price{100})), 7u);
    EXPECT_FALSE(ob.best_ask().has_value());
}

TEST(Matching, SweepsMultipleMakersAcrossLevels)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 2));
    me.submit(mk(2, Side::Sell, 101, 3));
    me.submit(mk(3, Side::Sell, 102, 5));
    auto r = me.submit(mk(4, Side::Buy, 102, 10));
    ASSERT_EQ(r.trades.size(), 3u);
    EXPECT_EQ(r.trades[0].price, Price{100});
    EXPECT_EQ(r.trades[1].price, Price{101});
    EXPECT_EQ(r.trades[2].price, Price{102});
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
    EXPECT_TRUE(ob.empty());
}

TEST(Matching, StopsAtFirstNonCrossingLevel)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 2));
    me.submit(mk(2, Side::Sell, 105, 5));
    auto r = me.submit(mk(3, Side::Buy, 102, 10));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].price, Price{100});
    EXPECT_EQ(r.order->status, OrderStatus::PartiallyFilled);
    EXPECT_EQ(*ob.best_bid(), Price{102});
    EXPECT_EQ(to_underlying(ob.bid_qty_at(Price{102})), 8u);
    EXPECT_EQ(*ob.best_ask(), Price{105});
}

// ---- time priority within a level ----------------------------------------

TEST(Matching, FIFOWithinLevel)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 5)); // arrives first
    me.submit(mk(2, Side::Sell, 100, 5)); // second
    auto r = me.submit(mk(3, Side::Buy, 100, 5));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].maker_id, OrderId{1}); // first in, first matched
}

// ---- IOC ------------------------------------------------------------------

TEST(Matching, IOCCancelsRemainder)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 3));
    auto r = me.submit(mk(2, Side::Buy, 100, 10, OrderType::Limit, TimeInForce::IOC));
    EXPECT_EQ(r.order->status, OrderStatus::PartiallyFilled);
    EXPECT_FALSE(ob.best_bid().has_value()); // not rested
}

TEST(Matching, IOCNoFillIsCancelled)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 105, 3));
    auto r = me.submit(mk(2, Side::Buy, 100, 10, OrderType::Limit, TimeInForce::IOC));
    EXPECT_EQ(r.order->status, OrderStatus::Cancelled);
    EXPECT_TRUE(r.trades.empty());
}

// ---- FOK ------------------------------------------------------------------

TEST(Matching, FOKFullyFillsOrRejects_Success)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 5));
    me.submit(mk(2, Side::Sell, 101, 5));
    auto r = me.submit(mk(3, Side::Buy, 101, 10, OrderType::Limit, TimeInForce::FOK));
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
    EXPECT_EQ(r.trades.size(), 2u);
}

TEST(Matching, FOKRejectsWhenInsufficientLiquidity)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 5));
    auto r = me.submit(mk(2, Side::Buy, 100, 10, OrderType::Limit, TimeInForce::FOK));
    EXPECT_EQ(r.order->status, OrderStatus::Rejected);
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(to_underlying(ob.ask_qty_at(Price{100})), 5u);
}

TEST(Matching, FOKRejectsWhenPriceCutoffPreventsFill)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 5));
    me.submit(mk(2, Side::Sell, 105, 5));
    auto r = me.submit(mk(3, Side::Buy, 100, 10, OrderType::Limit, TimeInForce::FOK));
    EXPECT_EQ(r.order->status, OrderStatus::Rejected);
}

// ---- Market ---------------------------------------------------------------

TEST(Matching, MarketBuyEatsAsks)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 3));
    me.submit(mk(2, Side::Sell, 101, 5));
    auto r = me.submit(mk(3, Side::Buy, 0, 6, OrderType::Market, TimeInForce::IOC));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].price, Price{100});
    EXPECT_EQ(r.trades[1].price, Price{101});
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
}

TEST(Matching, MarketCancelsRemainderWhenBookEmpty)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 3));
    auto r = me.submit(mk(2, Side::Buy, 0, 10, OrderType::Market, TimeInForce::IOC));
    EXPECT_EQ(r.order->status, OrderStatus::PartiallyFilled);
    EXPECT_TRUE(ob.empty());
}

// ---- cancel ---------------------------------------------------------------

TEST(Matching, CancelResting)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Buy, 100, 5));
    EXPECT_TRUE(me.cancel(OrderId{1}));
    EXPECT_FALSE(ob.best_bid().has_value());
}

TEST(Matching, CancelUnknownFails)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    EXPECT_FALSE(me.cancel(OrderId{42}));
}

// ---- self-cross is permitted at this phase --------------------------------

TEST(Matching, OrdersFromSameClientCanCrossInPhase1)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};
    me.submit(mk(1, Side::Sell, 100, 5));
    auto r = me.submit(mk(2, Side::Buy, 100, 5));
    EXPECT_EQ(r.order->status, OrderStatus::Filled);
}
