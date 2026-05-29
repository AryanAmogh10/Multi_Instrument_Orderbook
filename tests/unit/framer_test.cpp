#include <gtest/gtest.h>

#include "ordbk/protocol/framer.hpp"

using namespace ordbk::protocol;

TEST(Framer, EmptyReturnsNullopt)
{
    Framer f;
    EXPECT_FALSE(f.next().has_value());
}

TEST(Framer, SingleCompleteMessage)
{
    Framer f;
    auto bytes = encode(1, MessageBody{HeartbeatMsg{}});
    f.feed(bytes);
    auto m = f.next();
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->header.msg_type, MsgType::Heartbeat);
    EXPECT_FALSE(f.next().has_value());
}

TEST(Framer, MultipleMessagesInOneFeed)
{
    Framer f;
    std::vector<std::byte> combined;
    for (std::uint32_t i = 1; i <= 3; ++i)
    {
        auto b = encode(i, MessageBody{HeartbeatMsg{}});
        combined.insert(combined.end(), b.begin(), b.end());
    }
    f.feed(combined);
    for (std::uint32_t i = 1; i <= 3; ++i)
    {
        auto m = f.next();
        ASSERT_TRUE(m.has_value());
        EXPECT_EQ(m->header.sequence, i);
    }
    EXPECT_FALSE(f.next().has_value());
}

TEST(Framer, SplitMessageAcrossFeeds)
{
    Framer f;
    auto bytes = encode(7, MessageBody{NewOrderMsg{1, 2, 0, 0, 0, 100, 5}});
    // Feed byte-by-byte; nothing until the last byte arrives.
    for (std::size_t i = 0; i + 1 < bytes.size(); ++i)
    {
        f.feed(std::span<const std::byte>{&bytes[i], 1});
        EXPECT_FALSE(f.next().has_value());
    }
    f.feed(std::span<const std::byte>{&bytes.back(), 1});
    auto m = f.next();
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->header.sequence, 7u);
}

TEST(Framer, BadVersionFlagsError)
{
    Framer f;
    auto bytes = encode(1, MessageBody{HeartbeatMsg{}});
    bytes[0] = std::byte{99}; // corrupt version
    f.feed(bytes);
    EXPECT_FALSE(f.next().has_value());
    EXPECT_TRUE(f.has_error());
    EXPECT_EQ(f.error(), Framer::Error::BadVersion);
}

TEST(Framer, ResetClearsError)
{
    Framer f;
    auto bytes = encode(1, MessageBody{HeartbeatMsg{}});
    bytes[0] = std::byte{99};
    f.feed(bytes);
    (void)f.next();
    ASSERT_TRUE(f.has_error());
    f.reset();
    EXPECT_FALSE(f.has_error());
    EXPECT_EQ(f.buffered_bytes(), 0u);
}
