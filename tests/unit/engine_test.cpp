#include <gtest/gtest.h>

#include "velox/matching/matching_engine.hpp"
#include "velox/utils/order_pool.hpp"

#include <cassert>

using namespace velox;

namespace
{

// Pool shared across all tests in this file.
OrderPool g_pool{256};

InstrumentRegistry make_registry()
{
    InstrumentRegistry r;
    r.add(InstrumentSpec{InstrumentId{1}, "AAPL", InstrumentType::Equity, 1, 1, "USD"});
    r.add(InstrumentSpec{InstrumentId{2}, "MSFT", InstrumentType::Equity, 1, 1, "USD"});
    r.add(InstrumentSpec{InstrumentId{3}, "TSLA", InstrumentType::Equity, 1, 1, "USD"});
    r.freeze();
    return r;
}

Order* mk(std::uint64_t id, InstrumentId inst, Side side, std::int64_t price, std::uint64_t qty)
{
    Order* o = g_pool.acquire_or_abort();
    assert(o && "engine test pool exhausted — increase g_pool size");
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

TEST(Engine, RequiresFrozenRegistry)
{
    InstrumentRegistry r;
    r.add(InstrumentSpec{InstrumentId{1}, "AAPL", InstrumentType::Equity, 1, 1, "USD"});
    EXPECT_THROW((MatchingEngine{r, g_pool}), std::logic_error);
}

TEST(Engine, CreatesOneBookPerInstrument)
{
    auto r = make_registry();
    MatchingEngine eng{r, g_pool};
    EXPECT_EQ(eng.book_count(), 3u);
    EXPECT_NE(eng.book(InstrumentId{1}), nullptr);
    EXPECT_NE(eng.book(InstrumentId{2}), nullptr);
    EXPECT_NE(eng.book(InstrumentId{3}), nullptr);
}

TEST(Engine, FilterRestrictsBooks)
{
    auto r = make_registry();
    MatchingEngine eng{r, g_pool, [](InstrumentId id) { return to_underlying(id) % 2 == 1; }};
    EXPECT_EQ(eng.book_count(), 2u);
    EXPECT_NE(eng.book(InstrumentId{1}), nullptr);
    EXPECT_EQ(eng.book(InstrumentId{2}), nullptr);
    EXPECT_NE(eng.book(InstrumentId{3}), nullptr);
}

TEST(Engine, RoutesByInstrument)
{
    auto r = make_registry();
    MatchingEngine eng{r, g_pool};
    eng.submit(mk(1, InstrumentId{1}, Side::Buy, 100, 5));
    eng.submit(mk(2, InstrumentId{2}, Side::Buy, 200, 5));
    EXPECT_EQ(*eng.book(InstrumentId{1})->best_bid(), Price{100});
    EXPECT_EQ(*eng.book(InstrumentId{2})->best_bid(), Price{200});
    EXPECT_FALSE(eng.book(InstrumentId{3})->best_bid().has_value());
}

TEST(Engine, RejectsUnknownInstrument)
{
    auto r = make_registry();
    MatchingEngine eng{r, g_pool};
    auto o = mk(1, InstrumentId{999}, Side::Buy, 100, 5);
    auto res = eng.submit(o);
    EXPECT_EQ(res.order->status, OrderStatus::Rejected);
    eng.release_order(res.order); // release rejected taker
}

TEST(Engine, InstrumentsAreIsolated)
{
    auto r = make_registry();
    MatchingEngine eng{r, g_pool};
    // Cross on AAPL only.
    eng.submit(mk(1, InstrumentId{1}, Side::Sell, 100, 5));
    auto res = eng.submit(mk(2, InstrumentId{1}, Side::Buy, 100, 5));
    EXPECT_EQ(res.order->status, OrderStatus::Filled);
    eng.release_order(res.order);
    // MSFT untouched.
    EXPECT_TRUE(eng.book(InstrumentId{2})->empty());
}

TEST(Engine, CancelRoutesByInstrument)
{
    auto r = make_registry();
    MatchingEngine eng{r, g_pool};
    eng.submit(mk(1, InstrumentId{1}, Side::Buy, 100, 5));
    EXPECT_TRUE(eng.cancel(InstrumentId{1}, OrderId{1}));
    EXPECT_TRUE(eng.book(InstrumentId{1})->empty());
}

TEST(Engine, CancelUnknownInstrumentFails)
{
    auto r = make_registry();
    MatchingEngine eng{r, g_pool};
    EXPECT_FALSE(eng.cancel(InstrumentId{42}, OrderId{1}));
}
