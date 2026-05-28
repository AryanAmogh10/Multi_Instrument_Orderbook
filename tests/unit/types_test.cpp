#include <gtest/gtest.h>

#include "velox/core/types.hpp"

using namespace velox;

TEST(Types, UnderlyingExtraction)
{
    EXPECT_EQ(to_underlying(Price{1234}), 1234);
    EXPECT_EQ(to_underlying(Quantity{99}), 99u);
    EXPECT_EQ(to_underlying(OrderId{7}), 7u);
}

TEST(Types, QuantityAddition)
{
    Quantity a{10};
    Quantity b{15};
    EXPECT_EQ(to_underlying(a + b), 25u);
}

TEST(Types, QuantitySubtraction)
{
    EXPECT_EQ(to_underlying(Quantity{30} - Quantity{12}), 18u);
}

TEST(Types, QuantityCompoundAssign)
{
    Quantity a{10};
    a += Quantity{5};
    EXPECT_EQ(to_underlying(a), 15u);
    a -= Quantity{4};
    EXPECT_EQ(to_underlying(a), 11u);
}

TEST(Types, ZeroQtyConstant)
{
    EXPECT_EQ(to_underlying(kZeroQty), 0u);
}

TEST(Types, QtyMinPicksSmaller)
{
    EXPECT_EQ(to_underlying(qty_min(Quantity{4}, Quantity{9})), 4u);
    EXPECT_EQ(to_underlying(qty_min(Quantity{9}, Quantity{4})), 4u);
    EXPECT_EQ(to_underlying(qty_min(Quantity{7}, Quantity{7})), 7u);
}

TEST(Types, OppositeSide)
{
    EXPECT_EQ(opposite(Side::Buy), Side::Sell);
    EXPECT_EQ(opposite(Side::Sell), Side::Buy);
}

TEST(Types, PriceOrdering)
{
    EXPECT_LT(Price{1}, Price{2});
    EXPECT_GT(Price{5}, Price{4});
}

TEST(Types, StrongTypesAreDistinct)
{
    static_assert(!std::is_same_v<Price, Quantity>);
    static_assert(!std::is_same_v<OrderId, ClientId>);
    static_assert(!std::is_same_v<InstrumentId, ClientId>);
    SUCCEED();
}
