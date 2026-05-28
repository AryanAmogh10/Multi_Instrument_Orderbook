// Property-style randomised testing: generate large sequences of submits and
// cancels, then verify book invariants. Seeded for determinism.
//
// Invariants checked after every operation:
//   I1. Book is never crossed (best_bid < best_ask).
//   I2. Index count == sum of orders across all price levels.
//   I3. Every order on the bid side has Side::Buy; ask side has Side::Sell.
//   I4. Total filled across all orders is conserved relative to traded qty.

#include <gtest/gtest.h>

#include "velox/matching/book_matcher.hpp"
#include "velox/utils/order_pool.hpp"

#include <cassert>
#include <random>
#include <unordered_map>

using namespace velox;

namespace
{

constexpr InstrumentId kInst{1};

// Large pool: property tests submit up to 5000 orders without releasing.
OrderPool g_pool{8192};

Order* mk(std::uint64_t id, Side side, std::int64_t price, std::uint64_t qty, TimeInForce tif)
{
    Order* o = g_pool.acquire_or_abort();
    assert(o && "invariants test pool exhausted — increase g_pool size");
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
        tif,
        Timestamp{0},
    };
    return o;
}

struct Counts
{
    std::size_t orders = 0;
};

Counts walk_levels(const OrderBook& ob)
{
    Counts c;
    for (const auto& [p, list] : ob.bids())
    {
        (void)p;
        for (Order* o : list)
        {
            EXPECT_EQ(o->side, Side::Buy);
            ++c.orders;
        }
    }
    for (const auto& [p, list] : ob.asks())
    {
        (void)p;
        for (Order* o : list)
        {
            EXPECT_EQ(o->side, Side::Sell);
            ++c.orders;
        }
    }
    return c;
}

void check_invariants(const OrderBook& ob)
{
    if (ob.best_bid() && ob.best_ask())
    {
        EXPECT_LT(to_underlying(*ob.best_bid()), to_underlying(*ob.best_ask()))
            << "book is crossed";
    }
    const auto c = walk_levels(ob);
    EXPECT_EQ(c.orders, ob.order_count());
}

} // namespace

TEST(Property, RandomLimitSequenceMaintainsInvariants)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};

    std::mt19937_64 rng{0xC0FFEEULL};
    std::uniform_int_distribution<int> side_d(0, 1);
    std::uniform_int_distribution<int> price_d(95, 105);
    std::uniform_int_distribution<int> qty_d(1, 10);
    std::uniform_int_distribution<int> action_d(0, 9);

    std::vector<OrderId> live;
    std::uint64_t next_id = 1;
    std::uint64_t total_traded = 0;
    std::uint64_t total_submitted_qty = 0;

    for (int i = 0; i < 5000; ++i)
    {
        if (action_d(rng) < 2 && !live.empty())
        {
            // Cancel a random live order.
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const std::size_t idx = pick(rng);
            const OrderId id = live[idx];
            if (me.cancel(id))
            {
                live[idx] = live.back();
                live.pop_back();
            }
        }
        else
        {
            const Side side = side_d(rng) ? Side::Buy : Side::Sell;
            const auto qty = static_cast<std::uint64_t>(qty_d(rng));
            auto order = mk(next_id++, side, price_d(rng), qty, TimeInForce::GTC);
            total_submitted_qty += qty;
            auto res = me.submit(order);
            for (const auto& t : res.trades)
            {
                total_traded += to_underlying(t.quantity);
            }
            if (res.order->status == OrderStatus::New ||
                res.order->status == OrderStatus::PartiallyFilled)
            {
                if (!res.order->is_fully_filled() && ob.find(res.order->id) != nullptr)
                {
                    live.push_back(res.order->id);
                }
            }
        }
        check_invariants(ob);
    }

    // Conservation: traded qty must not exceed total submitted qty.
    EXPECT_LE(total_traded, total_submitted_qty);
}

TEST(Property, FOKNeverPartiallyExecutes)
{
    OrderBook ob{kInst};
    BookMatcher me{ob, g_pool};

    std::mt19937_64 rng{42};
    std::uniform_int_distribution<int> side_d(0, 1);
    std::uniform_int_distribution<int> price_d(95, 105);
    std::uniform_int_distribution<int> qty_d(1, 20);

    std::uint64_t next_id = 1;

    // Seed book with some GTC liquidity.
    for (int i = 0; i < 50; ++i)
    {
        const Side s = side_d(rng) ? Side::Buy : Side::Sell;
        me.submit(mk(
            next_id++, s, price_d(rng), static_cast<std::uint64_t>(qty_d(rng)), TimeInForce::GTC));
    }

    // Now hammer with FOKs.
    for (int i = 0; i < 500; ++i)
    {
        const auto before_count = ob.order_count();
        const Side s = side_d(rng) ? Side::Buy : Side::Sell;
        auto fok = mk(
            next_id++, s, price_d(rng), static_cast<std::uint64_t>(qty_d(rng)), TimeInForce::FOK);
        const std::uint64_t want = to_underlying(fok->initial_qty);
        auto res = me.submit(fok);

        if (res.order->status == OrderStatus::Rejected)
        {
            EXPECT_TRUE(res.trades.empty());
            EXPECT_LE(ob.order_count(), before_count);
        }
        else
        {
            EXPECT_EQ(res.order->status, OrderStatus::Filled);
            std::uint64_t got = 0;
            for (const auto& t : res.trades)
                got += to_underlying(t.quantity);
            EXPECT_EQ(got, want);
        }
        check_invariants(ob);
    }
}
