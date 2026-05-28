#include <gtest/gtest.h>

#include "velox/matching/expiry_sweeper.hpp"

using namespace velox;

namespace
{
constexpr ExpiryDate kJan{2026, 1, 17};
constexpr ExpiryDate kFeb{2026, 2, 20};
constexpr ExpiryDate kMar{2026, 3, 20};
} // namespace

TEST(ExpirySweeper, EmptySweepReturnsNothing)
{
    ExpirySweeper sw;
    auto expired = sw.sweep(kJan);
    EXPECT_TRUE(expired.empty());
}

TEST(ExpirySweeper, SweepOnExactDate)
{
    ExpirySweeper sw;
    sw.register_instrument(InstrumentId{1}, kJan);
    auto expired = sw.sweep(kJan);
    ASSERT_EQ(expired.size(), 1u);
    EXPECT_EQ(expired[0], InstrumentId{1});
    EXPECT_EQ(sw.size(), 0u);
}

TEST(ExpirySweeper, SweepBeforeDateKeepsInstrument)
{
    ExpirySweeper sw;
    sw.register_instrument(InstrumentId{1}, kFeb);
    auto expired = sw.sweep(kJan); // Jan < Feb → not expired yet
    EXPECT_TRUE(expired.empty());
    EXPECT_EQ(sw.size(), 1u);
}

TEST(ExpirySweeper, SweepCollectsMultiple)
{
    ExpirySweeper sw;
    sw.register_instrument(InstrumentId{1}, kJan);
    sw.register_instrument(InstrumentId{2}, kFeb);
    sw.register_instrument(InstrumentId{3}, kMar);

    auto expired = sw.sweep(kFeb); // Jan + Feb expire
    EXPECT_EQ(expired.size(), 2u);
    EXPECT_EQ(sw.size(), 1u); // Mar still pending
}

TEST(ExpirySweeper, SweepIsIdempotentAfterExpiry)
{
    ExpirySweeper sw;
    sw.register_instrument(InstrumentId{1}, kJan);
    sw.sweep(kJan);
    auto again = sw.sweep(kJan); // already swept
    EXPECT_TRUE(again.empty());
}

TEST(ExpirySweeper, CallbackInvoked)
{
    ExpirySweeper sw;
    sw.register_instrument(InstrumentId{1}, kJan);
    sw.register_instrument(InstrumentId{2}, kFeb);

    std::vector<InstrumentId> seen;
    sw.set_callback([&](InstrumentId id) { seen.push_back(id); });

    sw.sweep(kJan);
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], InstrumentId{1});
}

TEST(ExpirySweeper, UnregisterPreventsExpiry)
{
    ExpirySweeper sw;
    sw.register_instrument(InstrumentId{1}, kJan);
    sw.unregister(InstrumentId{1});
    EXPECT_EQ(sw.size(), 0u);
    auto expired = sw.sweep(kJan);
    EXPECT_TRUE(expired.empty());
}

TEST(ExpirySweeper, MultipleAtSameDate)
{
    ExpirySweeper sw;
    sw.register_instrument(InstrumentId{1}, kJan);
    sw.register_instrument(InstrumentId{2}, kJan);
    sw.register_instrument(InstrumentId{3}, kJan);

    auto expired = sw.sweep(kJan);
    EXPECT_EQ(expired.size(), 3u);
    EXPECT_EQ(sw.size(), 0u);
}
