#include <gtest/gtest.h>

#include "ordbk/protocol/codec.hpp"

using namespace ordbk::protocol;

TEST(Codec, BufferWriterWritesLE)
{
    std::array<std::byte, 8> buf{};
    BufferWriter w{buf};
    ASSERT_TRUE(w.write_u32(0x04030201u));
    EXPECT_EQ(std::to_integer<std::uint8_t>(buf[0]), 0x01);
    EXPECT_EQ(std::to_integer<std::uint8_t>(buf[1]), 0x02);
    EXPECT_EQ(std::to_integer<std::uint8_t>(buf[2]), 0x03);
    EXPECT_EQ(std::to_integer<std::uint8_t>(buf[3]), 0x04);
}

TEST(Codec, RoundTripIntegers)
{
    std::array<std::byte, 32> buf{};
    BufferWriter w{buf};
    ASSERT_TRUE(w.write_u8(0xAB));
    ASSERT_TRUE(w.write_u16(0xCAFE));
    ASSERT_TRUE(w.write_u32(0xDEADBEEFu));
    ASSERT_TRUE(w.write_u64(0xFEEDFACEDEADBEEFull));
    ASSERT_TRUE(w.write_i64(-12345));

    BufferReader r{buf};
    std::uint8_t a{};
    std::uint16_t b{};
    std::uint32_t c{};
    std::uint64_t d{};
    std::int64_t e{};
    ASSERT_TRUE(r.read_u8(a));
    EXPECT_EQ(a, 0xAB);
    ASSERT_TRUE(r.read_u16(b));
    EXPECT_EQ(b, 0xCAFE);
    ASSERT_TRUE(r.read_u32(c));
    EXPECT_EQ(c, 0xDEADBEEFu);
    ASSERT_TRUE(r.read_u64(d));
    EXPECT_EQ(d, 0xFEEDFACEDEADBEEFull);
    ASSERT_TRUE(r.read_i64(e));
    EXPECT_EQ(e, -12345);
}

TEST(Codec, OverflowDetected)
{
    std::array<std::byte, 2> buf{};
    BufferWriter w{buf};
    ASSERT_TRUE(w.write_u8(1));
    ASSERT_TRUE(w.write_u8(2));
    EXPECT_FALSE(w.write_u8(3));  // full
    EXPECT_FALSE(w.write_u32(0)); // partial overflow
}

TEST(Codec, HeaderRoundTrip)
{
    std::array<std::byte, 8> buf{};
    BufferWriter w{buf};
    MessageHeader h{kProtocolVersion, MsgType::NewOrder, 32, 0xABCDEFu};
    ASSERT_TRUE(encode_header(w, h));
    BufferReader r{buf};
    auto out = decode_header(r);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->protocol_version, kProtocolVersion);
    EXPECT_EQ(out->msg_type, MsgType::NewOrder);
    EXPECT_EQ(out->body_length, 32);
    EXPECT_EQ(out->sequence, 0xABCDEFu);
}

TEST(Codec, NewOrderRoundTrip)
{
    NewOrderMsg msg{
        .client_order_id = 42,
        .instrument_id = 7,
        .side = 0,
        .order_type = 0,
        .tif = 1,
        .price = 10000,
        .quantity = 50,
    };
    auto bytes = encode(1, MessageBody{msg});
    ASSERT_GE(bytes.size(), kHeaderSize);

    BufferReader hr{std::span<const std::byte>{bytes.data(), kHeaderSize}};
    auto h = decode_header(hr);
    ASSERT_TRUE(h.has_value());
    EXPECT_EQ(h->msg_type, MsgType::NewOrder);

    auto body =
        decode_body(*h, std::span<const std::byte>{bytes.data() + kHeaderSize, h->body_length});
    ASSERT_TRUE(body.has_value());
    auto* parsed = std::get_if<NewOrderMsg>(&*body);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->client_order_id, 42u);
    EXPECT_EQ(parsed->instrument_id, 7u);
    EXPECT_EQ(parsed->price, 10000);
    EXPECT_EQ(parsed->quantity, 50u);
    EXPECT_EQ(parsed->tif, 1);
}

TEST(Codec, FillRoundTrip)
{
    FillMsg fill{12, 99, -1234, 7, 1};
    auto bytes = encode(5, MessageBody{fill});

    BufferReader hr{std::span<const std::byte>{bytes.data(), kHeaderSize}};
    auto h = decode_header(hr);
    ASSERT_TRUE(h);
    auto body =
        decode_body(*h, std::span<const std::byte>{bytes.data() + kHeaderSize, h->body_length});
    auto* parsed = std::get_if<FillMsg>(&*body);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->client_order_id, 12u);
    EXPECT_EQ(parsed->server_order_id, 99u);
    EXPECT_EQ(parsed->price, -1234);
    EXPECT_EQ(parsed->quantity, 7u);
}

TEST(Codec, EmptyBodyMessages)
{
    for (auto mt : {MsgType::Logout, MsgType::Heartbeat})
    {
        MessageBody body =
            (mt == MsgType::Logout) ? MessageBody{LogoutMsg{}} : MessageBody{HeartbeatMsg{}};
        auto bytes = encode(1, body);
        EXPECT_EQ(bytes.size(), kHeaderSize);
        BufferReader hr{bytes};
        auto h = decode_header(hr);
        ASSERT_TRUE(h);
        EXPECT_EQ(h->body_length, 0);
        EXPECT_EQ(h->msg_type, mt);
    }
}

TEST(Codec, RejectReasonRoundTrip)
{
    OrderRejectMsg rj{100, RejectReason::UnknownInstrument};
    auto bytes = encode(1, MessageBody{rj});
    BufferReader hr{std::span<const std::byte>{bytes.data(), kHeaderSize}};
    auto h = decode_header(hr);
    auto body =
        decode_body(*h, std::span<const std::byte>{bytes.data() + kHeaderSize, h->body_length});
    auto* parsed = std::get_if<OrderRejectMsg>(&*body);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->reason, RejectReason::UnknownInstrument);
}
