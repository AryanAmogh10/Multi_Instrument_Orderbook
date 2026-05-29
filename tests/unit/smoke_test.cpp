#include "ordbk/core/version.hpp"
#include <gtest/gtest.h>

TEST(Smoke, VersionStringIsNonEmpty)
{
    EXPECT_FALSE(ordbk::kVersion.empty());
}

TEST(Smoke, ArithmeticStillWorks)
{
    EXPECT_EQ(2 + 2, 4);
}
