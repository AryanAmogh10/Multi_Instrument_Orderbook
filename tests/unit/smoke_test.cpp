#include "velox/core/version.hpp"
#include <gtest/gtest.h>

TEST(Smoke, VersionStringIsNonEmpty)
{
    EXPECT_FALSE(velox::kVersion.empty());
}

TEST(Smoke, ArithmeticStillWorks)
{
    EXPECT_EQ(2 + 2, 4);
}
