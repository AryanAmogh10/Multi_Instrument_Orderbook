#include <gtest/gtest.h>

#include "velox/matching/matching_engine.hpp"

using namespace velox;

namespace {

InstrumentRegistry make_registry() {
    InstrumentRegistry r;
    r.add(InstrumentSpec{InstrumentId{1}, "AAPL", InstrumentType::Equity, 1, 1, "USD"});
    r.add(InstrumentSpec{InstrumentId{2}, "MSFT", InstrumentType::Equity, 1, 1, "USD"});
    r.add(InstrumentSpec{InstrumentId{3}, "TSLA", InstrumentType::Equity, 1, 1, "USD"});
    r.freeze();
    return r;
}

OrderBook::OrderPtr mk(std::uint64_t id, InstrumentId inst, Side side,
                      std::int64_t price, std::uint64_t qty) {
    return std::make_shared<Order>(Order{
        OrderId{id}, inst, ClientId{1}, side, OrderType::Limit, TimeInForce::GTC,
        Price{price}, Quantity{qty}, kZeroQty, Timestamp{0}, OrderStatus::New,
    });
}

}  // namespace

TEST(Engine, RequiresFrozenRegistry) {
    InstrumentRegistry r;
    r.add(InstrumentSpec{InstrumentId{1}, "AAPL", InstrumentType::Equity, 1, 1, "USD"});
    EXPECT_THROW(MatchingEngine{r}, std::logic_error);
}

TEST(Engine, CreatesOneBookPerInstrument) {
    auto r = make_registry();
    MatchingEngine eng{r};
    EXPECT_EQ(eng.book_count(), 3u);
    EXPECT_NE(eng.book(InstrumentId{1}), nullptr);
    EXPECT_NE(eng.book(InstrumentId{2}), nullptr);
    EXPECT_NE(eng.book(InstrumentId{3}), nullptr);
}

TEST(Engine, FilterRestrictsBooks) {
    auto r = make_registry();
    MatchingEngine eng{r, [](InstrumentId id) { return to_underlying(id) % 2 == 1; }};
    EXPECT_EQ(eng.book_count(), 2u);
    EXPECT_NE(eng.book(InstrumentId{1}), nullptr);
    EXPECT_EQ(eng.book(InstrumentId{2}), nullptr);
    EXPECT_NE(eng.book(InstrumentId{3}), nullptr);
}

TEST(Engine, RoutesByInstrument) {
    auto r = make_registry();
    MatchingEngine eng{r};
    eng.submit(mk(1, InstrumentId{1}, Side::Buy, 100, 5));
    eng.submit(mk(2, InstrumentId{2}, Side::Buy, 200, 5));
    EXPECT_EQ(*eng.book(InstrumentId{1})->best_bid(), Price{100});
    EXPECT_EQ(*eng.book(InstrumentId{2})->best_bid(), Price{200});
    EXPECT_FALSE(eng.book(InstrumentId{3})->best_bid().has_value());
}

TEST(Engine, RejectsUnknownInstrument) {
    auto r = make_registry();
    MatchingEngine eng{r};
    auto o = mk(1, InstrumentId{999}, Side::Buy, 100, 5);
    auto res = eng.submit(o);
    EXPECT_EQ(res.order->status, OrderStatus::Rejected);
}

TEST(Engine, InstrumentsAreIsolated) {
    auto r = make_registry();
    MatchingEngine eng{r};
    // Cross on AAPL only.
    eng.submit(mk(1, InstrumentId{1}, Side::Sell, 100, 5));
    auto res = eng.submit(mk(2, InstrumentId{1}, Side::Buy, 100, 5));
    EXPECT_EQ(res.order->status, OrderStatus::Filled);
    // MSFT untouched.
    EXPECT_TRUE(eng.book(InstrumentId{2})->empty());
}

TEST(Engine, CancelRoutesByInstrument) {
    auto r = make_registry();
    MatchingEngine eng{r};
    eng.submit(mk(1, InstrumentId{1}, Side::Buy, 100, 5));
    EXPECT_TRUE(eng.cancel(InstrumentId{1}, OrderId{1}));
    EXPECT_TRUE(eng.book(InstrumentId{1})->empty());
}

TEST(Engine, CancelUnknownInstrumentFails) {
    auto r = make_registry();
    MatchingEngine eng{r};
    EXPECT_FALSE(eng.cancel(InstrumentId{42}, OrderId{1}));
}
