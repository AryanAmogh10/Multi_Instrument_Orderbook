#include <gtest/gtest.h>

#include "velox/orderbook/order.hpp"

using namespace velox;

namespace
{

Order make_order(std::uint64_t id, Side side, std::int64_t price, std::uint64_t qty)
{
    // Phase 4 field order: id, limit_price, initial_qty, filled_qty,
    //                      instrument, client, status, side, type, tif, ts
    return Order{
        OrderId{id},
        Price{price},
        Quantity{qty},
        kZeroQty,
        InstrumentId{1},
        ClientId{1},
        OrderStatus::New,
        side,
        OrderType::Limit,
        TimeInForce::GTC,
        Timestamp{0},
    };
}

} // namespace

TEST(OrderModel, RemainingWhenUnfilled)
{
    Order o = make_order(1, Side::Buy, 100, 10);
    EXPECT_EQ(to_underlying(o.remaining()), 10u);
}

TEST(OrderModel, RemainingAfterPartial)
{
    Order o = make_order(1, Side::Buy, 100, 10);
    o.filled_qty = Quantity{3};
    EXPECT_EQ(to_underlying(o.remaining()), 7u);
}

TEST(OrderModel, FullyFilledFlag)
{
    Order o = make_order(1, Side::Buy, 100, 10);
    EXPECT_FALSE(o.is_fully_filled());
    o.filled_qty = Quantity{10};
    EXPECT_TRUE(o.is_fully_filled());
}

TEST(OrderModel, TerminalDetection)
{
    Order o = make_order(1, Side::Buy, 100, 10);
    EXPECT_FALSE(o.is_terminal());
    o.status = OrderStatus::PartiallyFilled;
    EXPECT_FALSE(o.is_terminal());
    o.status = OrderStatus::Filled;
    EXPECT_TRUE(o.is_terminal());
    o.status = OrderStatus::Cancelled;
    EXPECT_TRUE(o.is_terminal());
    o.status = OrderStatus::Rejected;
    EXPECT_TRUE(o.is_terminal());
}

TEST(OrderModel, DefaultStatusIsNew)
{
    Order o = make_order(1, Side::Buy, 100, 10);
    EXPECT_EQ(o.status, OrderStatus::New);
}

TEST(OrderModel, DefaultFilledIsZero)
{
    Order o = make_order(1, Side::Buy, 100, 10);
    EXPECT_EQ(to_underlying(o.filled_qty), 0u);
}
