#include <gtest/gtest.h>

#include "ordbk/instruments/instrument_registry.hpp"

using namespace ordbk;

namespace
{
InstrumentSpec equity(std::uint32_t id, std::string sym)
{
    return InstrumentSpec{InstrumentId{id}, std::move(sym), InstrumentType::Equity, 1, 1, "USD"};
}
} // namespace

TEST(InstrumentRegistry, StartsEmptyUnfrozen)
{
    InstrumentRegistry r;
    EXPECT_EQ(r.size(), 0u);
    EXPECT_FALSE(r.frozen());
}

TEST(InstrumentRegistry, AddIncreasesSize)
{
    InstrumentRegistry r;
    r.add(equity(1, "AAPL"));
    r.add(equity(2, "MSFT"));
    EXPECT_EQ(r.size(), 2u);
}

TEST(InstrumentRegistry, LookupById)
{
    InstrumentRegistry r;
    r.add(equity(7, "TSLA"));
    const auto* spec = r.find(InstrumentId{7});
    ASSERT_NE(spec, nullptr);
    EXPECT_EQ(spec->symbol, "TSLA");
}

TEST(InstrumentRegistry, LookupBySymbol)
{
    InstrumentRegistry r;
    r.add(equity(7, "TSLA"));
    const auto* spec = r.find("TSLA");
    ASSERT_NE(spec, nullptr);
    EXPECT_EQ(to_underlying(spec->id), 7u);
}

TEST(InstrumentRegistry, MissingIdReturnsNull)
{
    InstrumentRegistry r;
    EXPECT_EQ(r.find(InstrumentId{42}), nullptr);
    EXPECT_EQ(r.find("NOPE"), nullptr);
}

TEST(InstrumentRegistry, DuplicateIdRejected)
{
    InstrumentRegistry r;
    r.add(equity(1, "AAPL"));
    EXPECT_THROW(r.add(equity(1, "OTHER")), InstrumentRegistry::DuplicateError);
}

TEST(InstrumentRegistry, DuplicateSymbolRejected)
{
    InstrumentRegistry r;
    r.add(equity(1, "AAPL"));
    EXPECT_THROW(r.add(equity(2, "AAPL")), InstrumentRegistry::DuplicateError);
}

TEST(InstrumentRegistry, FreezeBlocksFurtherAdds)
{
    InstrumentRegistry r;
    r.add(equity(1, "AAPL"));
    r.freeze();
    EXPECT_TRUE(r.frozen());
    EXPECT_THROW(r.add(equity(2, "MSFT")), InstrumentRegistry::FrozenError);
}

TEST(InstrumentRegistry, LookupsWorkAfterFreeze)
{
    InstrumentRegistry r;
    r.add(equity(1, "AAPL"));
    r.freeze();
    EXPECT_NE(r.find(InstrumentId{1}), nullptr);
    EXPECT_NE(r.find("AAPL"), nullptr);
}

TEST(InstrumentRegistry, AllReturnsAllSpecs)
{
    InstrumentRegistry r;
    r.add(equity(1, "AAPL"));
    r.add(equity(2, "MSFT"));
    EXPECT_EQ(r.all().size(), 2u);
}
