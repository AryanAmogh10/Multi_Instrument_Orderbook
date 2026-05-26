#include <gtest/gtest.h>

#include "velox/instruments/instrument_registry.hpp"
#include "velox/instruments/option_contract.hpp"
#include "velox/matching/expiry_sweeper.hpp"
#include "velox/matching/matching_engine.hpp"
#include "velox/utils/order_pool.hpp"

#include <cassert>

using namespace velox;

namespace {

OrderPool g_pool{512};

constexpr ExpiryDate kJan{2026, 1, 17};
constexpr ExpiryDate kFeb{2026, 2, 20};

// Build a registry with 2 equities + 4 options (2 calls + 2 puts on AAPL).
InstrumentRegistry make_options_registry() {
    InstrumentRegistry r;
    r.add(InstrumentSpec{InstrumentId{1}, "AAPL", InstrumentType::Equity, 1, 1, "USD"});
    r.add(InstrumentSpec{InstrumentId{2}, "MSFT", InstrumentType::Equity, 1, 1, "USD"});

    // AAPL Jan $150 call (id=10)
    {
        OptionContract c;
        c.underlying  = "AAPL";
        c.strike      = Price{15000};
        c.expiry      = kJan;
        c.option_type = OptionType::Call;
        r.add(InstrumentSpec{InstrumentId{10}, c.occ_symbol(),
                             InstrumentType::Option, 1, 1, "USD", c});
    }
    // AAPL Jan $150 put (id=11)
    {
        OptionContract c;
        c.underlying  = "AAPL";
        c.strike      = Price{15000};
        c.expiry      = kJan;
        c.option_type = OptionType::Put;
        r.add(InstrumentSpec{InstrumentId{11}, c.occ_symbol(),
                             InstrumentType::Option, 1, 1, "USD", c});
    }
    // AAPL Feb $155 call (id=12)
    {
        OptionContract c;
        c.underlying  = "AAPL";
        c.strike      = Price{15500};
        c.expiry      = kFeb;
        c.option_type = OptionType::Call;
        r.add(InstrumentSpec{InstrumentId{12}, c.occ_symbol(),
                             InstrumentType::Option, 1, 1, "USD", c});
    }
    // AAPL Feb $155 put (id=13)
    {
        OptionContract c;
        c.underlying  = "AAPL";
        c.strike      = Price{15500};
        c.expiry      = kFeb;
        c.option_type = OptionType::Put;
        r.add(InstrumentSpec{InstrumentId{13}, c.occ_symbol(),
                             InstrumentType::Option, 1, 1, "USD", c});
    }
    r.freeze();
    return r;
}

Order* mk(InstrumentId inst, Side side, std::int64_t price, std::uint64_t qty) {
    Order* o = g_pool.acquire_or_abort();
    *o = Order{
        OrderId{0}, Price{price}, Quantity{qty}, kZeroQty,
        inst, ClientId{1}, OrderStatus::New, side,
        OrderType::Limit, TimeInForce::GTC, Timestamp{0},
    };
    return o;
}

}  // namespace

// ---- InstrumentRegistry option chain queries ------------------------------

TEST(OptionsE2E, RegistryBuildsOptionChain) {
    auto r = make_options_registry();

    const OptionChain* chain = r.option_chain("AAPL");
    ASSERT_NE(chain, nullptr);
    EXPECT_EQ(chain->size(), 4u);

    EXPECT_EQ(r.option_chain("MSFT"), nullptr);  // equity, no options
    EXPECT_EQ(r.option_chain("TSLA"), nullptr);  // unknown
}

TEST(OptionsE2E, OptionChainFindBySpec) {
    auto r = make_options_registry();
    const OptionChain* chain = r.option_chain("AAPL");

    auto id = chain->find(kJan, Price{15000}, OptionType::Call);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, InstrumentId{10});

    auto put = chain->find(kJan, Price{15000}, OptionType::Put);
    ASSERT_TRUE(put.has_value());
    EXPECT_EQ(*put, InstrumentId{11});
}

TEST(OptionsE2E, OptionChainAtExpiry) {
    auto r = make_options_registry();
    const OptionChain* chain = r.option_chain("AAPL");

    auto jan_ids = chain->at_expiry(kJan);
    EXPECT_EQ(jan_ids.size(), 2u);  // call + put

    auto feb_ids = chain->at_expiry(kFeb);
    EXPECT_EQ(feb_ids.size(), 2u);
}

TEST(OptionsE2E, OptionUnderlyings) {
    auto r = make_options_registry();
    auto underlyings = r.option_underlyings();
    ASSERT_EQ(underlyings.size(), 1u);
    EXPECT_EQ(underlyings[0], "AAPL");
}

// ---- Matching on option instruments ---------------------------------------

TEST(OptionsE2E, TradeOptionContract) {
    auto r = make_options_registry();
    MatchingEngine eng{r, g_pool};

    // Sell then buy the Jan $150 call.
    static std::uint64_t oid = 100;
    auto seller = mk(InstrumentId{10}, Side::Sell, 500, 1);  // ask $5.00
    seller->id = OrderId{++oid};
    eng.submit(seller);

    auto buyer = mk(InstrumentId{10}, Side::Buy, 500, 1);   // bid $5.00 → crosses
    buyer->id = OrderId{++oid};
    auto result = eng.submit(buyer);

    EXPECT_EQ(result.order->status, OrderStatus::Filled);
    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, Price{500});
    eng.release_order(result.order);
}

TEST(OptionsE2E, OptionBooksAreIsolated) {
    auto r = make_options_registry();
    MatchingEngine eng{r, g_pool};

    static std::uint64_t oid = 200;

    // Rest a sell on the Jan call.
    auto ask = mk(InstrumentId{10}, Side::Sell, 500, 5);
    ask->id = OrderId{++oid};
    eng.submit(ask);

    // Put is a different instrument — should see an empty book.
    EXPECT_TRUE(eng.book(InstrumentId{11})->empty());

    // Equity book also untouched.
    EXPECT_TRUE(eng.book(InstrumentId{1})->empty());
}

// ---- Expiry flow ----------------------------------------------------------

TEST(OptionsE2E, ExpireInstrumentCancelsOrders) {
    auto r = make_options_registry();
    MatchingEngine eng{r, g_pool};

    static std::uint64_t oid = 300;

    // Rest a buy on the Jan $150 put.
    auto bid = mk(InstrumentId{11}, Side::Buy, 200, 10);
    bid->id = OrderId{++oid};
    eng.submit(bid);
    EXPECT_EQ(to_underlying(eng.book(InstrumentId{11})->bid_qty_at(Price{200})), 10u);

    // Expire the instrument.
    EXPECT_TRUE(eng.expire_instrument(InstrumentId{11}));
    EXPECT_EQ(eng.book(InstrumentId{11}), nullptr);

    // Second call returns false.
    EXPECT_FALSE(eng.expire_instrument(InstrumentId{11}));
}

TEST(OptionsE2E, ExpireSweepIntegration) {
    auto r = make_options_registry();
    MatchingEngine eng{r, g_pool};

    ExpirySweeper sweeper;
    // Register Jan options in the sweeper.
    sweeper.register_instrument(InstrumentId{10}, kJan);
    sweeper.register_instrument(InstrumentId{11}, kJan);
    sweeper.register_instrument(InstrumentId{12}, kFeb);
    sweeper.register_instrument(InstrumentId{13}, kFeb);

    sweeper.set_callback([&](InstrumentId id) {
        eng.expire_instrument(id);
    });

    // Sweep on Jan expiry — should retire ids 10 and 11.
    auto expired = sweeper.sweep(kJan);
    EXPECT_EQ(expired.size(), 2u);
    EXPECT_EQ(eng.book(InstrumentId{10}), nullptr);
    EXPECT_EQ(eng.book(InstrumentId{11}), nullptr);

    // Feb options still alive.
    EXPECT_NE(eng.book(InstrumentId{12}), nullptr);
    EXPECT_NE(eng.book(InstrumentId{13}), nullptr);
    EXPECT_EQ(sweeper.size(), 2u);
}
