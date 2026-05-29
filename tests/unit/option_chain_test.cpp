#include <gtest/gtest.h>

#include "ordbk/instruments/option_chain.hpp"

#include <algorithm>

using namespace ordbk;

namespace
{

constexpr ExpiryDate kJan{2026, 1, 17};
constexpr ExpiryDate kFeb{2026, 2, 20};
constexpr ExpiryDate kMar{2026, 3, 20};

OptionContract
make_contract(const std::string& und, ExpiryDate exp, std::int64_t strike_cents, OptionType type)
{
    OptionContract c;
    c.underlying = und;
    c.expiry = exp;
    c.strike = Price{strike_cents};
    c.option_type = type;
    return c;
}

} // namespace

TEST(OptionChain, EmptyChain)
{
    OptionChain chain{"AAPL"};
    EXPECT_EQ(chain.underlying(), "AAPL");
    EXPECT_EQ(chain.size(), 0u);
    EXPECT_FALSE(chain.find(kJan, Price{15000}, OptionType::Call).has_value());
    EXPECT_TRUE(chain.expiries().empty());
}

TEST(OptionChain, AddAndFind)
{
    OptionChain chain{"AAPL"};
    auto c = make_contract("AAPL", kJan, 15000, OptionType::Call);
    EXPECT_TRUE(chain.add(InstrumentId{1}, c));
    EXPECT_EQ(chain.size(), 1u);

    auto found = chain.find(kJan, Price{15000}, OptionType::Call);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, InstrumentId{1});
}

TEST(OptionChain, AddDuplicateReturnsFalse)
{
    OptionChain chain{"AAPL"};
    auto c = make_contract("AAPL", kJan, 15000, OptionType::Call);
    EXPECT_TRUE(chain.add(InstrumentId{1}, c));
    EXPECT_FALSE(chain.add(InstrumentId{2}, c)); // same slot
    EXPECT_FALSE(chain.add(InstrumentId{1}, c)); // same id
}

TEST(OptionChain, CallAndPutAtSameStrike)
{
    OptionChain chain{"AAPL"};
    EXPECT_TRUE(chain.add(InstrumentId{1}, make_contract("AAPL", kJan, 15000, OptionType::Call)));
    EXPECT_TRUE(chain.add(InstrumentId{2}, make_contract("AAPL", kJan, 15000, OptionType::Put)));
    EXPECT_EQ(chain.size(), 2u);
    EXPECT_EQ(*chain.find(kJan, Price{15000}, OptionType::Call), InstrumentId{1});
    EXPECT_EQ(*chain.find(kJan, Price{15000}, OptionType::Put), InstrumentId{2});
}

TEST(OptionChain, AtExpiry)
{
    OptionChain chain{"AAPL"};
    chain.add(InstrumentId{1}, make_contract("AAPL", kJan, 14500, OptionType::Call));
    chain.add(InstrumentId{2}, make_contract("AAPL", kJan, 14500, OptionType::Put));
    chain.add(InstrumentId{3}, make_contract("AAPL", kJan, 15000, OptionType::Call));
    chain.add(InstrumentId{4}, make_contract("AAPL", kFeb, 15000, OptionType::Call));

    auto jan = chain.at_expiry(kJan);
    EXPECT_EQ(jan.size(), 3u);

    auto feb = chain.at_expiry(kFeb);
    EXPECT_EQ(feb.size(), 1u);
    EXPECT_EQ(feb[0], InstrumentId{4});
}

TEST(OptionChain, CallsAtAndPutsAt)
{
    OptionChain chain{"AAPL"};
    chain.add(InstrumentId{1}, make_contract("AAPL", kJan, 14500, OptionType::Call));
    chain.add(InstrumentId{2}, make_contract("AAPL", kJan, 14500, OptionType::Put));
    chain.add(InstrumentId{3}, make_contract("AAPL", kJan, 15000, OptionType::Call));

    auto calls = chain.calls_at(kJan);
    EXPECT_EQ(calls.size(), 2u);

    auto puts = chain.puts_at(kJan);
    EXPECT_EQ(puts.size(), 1u);
    EXPECT_EQ(puts[0], InstrumentId{2});
}

TEST(OptionChain, ExpiresReturnsSorted)
{
    OptionChain chain{"AAPL"};
    chain.add(InstrumentId{3}, make_contract("AAPL", kMar, 15000, OptionType::Call));
    chain.add(InstrumentId{1}, make_contract("AAPL", kJan, 15000, OptionType::Call));
    chain.add(InstrumentId{2}, make_contract("AAPL", kFeb, 15000, OptionType::Call));

    auto exps = chain.expiries();
    ASSERT_EQ(exps.size(), 3u);
    EXPECT_EQ(exps[0], kJan);
    EXPECT_EQ(exps[1], kFeb);
    EXPECT_EQ(exps[2], kMar);
}

TEST(OptionChain, ExpiringOnOrBefore)
{
    OptionChain chain{"AAPL"};
    chain.add(InstrumentId{1}, make_contract("AAPL", kJan, 15000, OptionType::Call));
    chain.add(InstrumentId{2}, make_contract("AAPL", kFeb, 15000, OptionType::Call));
    chain.add(InstrumentId{3}, make_contract("AAPL", kMar, 15000, OptionType::Call));

    auto before_feb = chain.expiring_on_or_before(kFeb);
    EXPECT_EQ(before_feb.size(), 2u); // Jan + Feb

    auto before_jan = chain.expiring_on_or_before(kJan);
    EXPECT_EQ(before_jan.size(), 1u); // Jan only

    ExpiryDate dec{2025, 12, 31};
    EXPECT_TRUE(chain.expiring_on_or_before(dec).empty());
}

TEST(OptionChain, Remove)
{
    OptionChain chain{"AAPL"};
    chain.add(InstrumentId{1}, make_contract("AAPL", kJan, 15000, OptionType::Call));
    chain.add(InstrumentId{2}, make_contract("AAPL", kJan, 15000, OptionType::Put));

    EXPECT_TRUE(chain.remove(InstrumentId{1}));
    EXPECT_EQ(chain.size(), 1u);
    EXPECT_FALSE(chain.find(kJan, Price{15000}, OptionType::Call).has_value());
    EXPECT_TRUE(chain.find(kJan, Price{15000}, OptionType::Put).has_value());

    EXPECT_FALSE(chain.remove(InstrumentId{1})); // already removed
}

TEST(OptionChain, RemoveLastContractClearsExpiry)
{
    OptionChain chain{"AAPL"};
    chain.add(InstrumentId{1}, make_contract("AAPL", kJan, 15000, OptionType::Call));
    chain.remove(InstrumentId{1});
    EXPECT_TRUE(chain.expiries().empty());
}
