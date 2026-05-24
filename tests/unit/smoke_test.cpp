#include <gtest/gtest.h>
#include "velox/core/version.hpp"

TEST(Smoke, VersionStringIsNonEmpty) {
    EXPECT_FALSE(velox::kVersion.empty());
}

TEST(Smoke, ArithmeticStillWorks) {
    EXPECT_EQ(2 + 2, 4);
}
