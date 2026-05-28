#include "velox/protocol/codec.hpp"

#include <array>
#include <type_traits>

namespace velox::protocol
{

// ---- BufferWriter --------------------------------------------------------

bool BufferWriter::write_u8(std::uint8_t v) noexcept
{
    if (pos_ + 1 > buf_.size())
        return false;
    buf_[pos_++] = std::byte{v};
    return true;
}

bool BufferWriter::write_u16(std::uint16_t v) noexcept
{
    if (pos_ + 2 > buf_.size())
        return false;
    buf_[pos_++] = static_cast<std::byte>(v & 0xFFu);
    buf_[pos_++] = static_cast<std::byte>((v >> 8) & 0xFFu);
    return true;
}

bool BufferWriter::write_u32(std::uint32_t v) noexcept
{
    if (pos_ + 4 > buf_.size())
        return false;
    for (int i = 0; i < 4; ++i)
    {
        buf_[pos_++] = static_cast<std::byte>((v >> (8 * i)) & 0xFFu);
    }
    return true;
}

bool BufferWriter::write_u64(std::uint64_t v) noexcept
{
    if (pos_ + 8 > buf_.size())
        return false;
    for (int i = 0; i < 8; ++i)
    {
        buf_[pos_++] = static_cast<std::byte>((v >> (8 * i)) & 0xFFu);
    }
    return true;
}

bool BufferWriter::write_i64(std::int64_t v) noexcept
{
    return write_u64(static_cast<std::uint64_t>(v));
}

// ---- BufferReader --------------------------------------------------------

bool BufferReader::read_u8(std::uint8_t& out) noexcept
{
    if (pos_ + 1 > buf_.size())
        return false;
    out = std::to_integer<std::uint8_t>(buf_[pos_++]);
    return true;
}

bool BufferReader::read_u16(std::uint16_t& out) noexcept
{
    if (pos_ + 2 > buf_.size())
        return false;
    const auto a = std::to_integer<std::uint16_t>(buf_[pos_++]);
    const auto b = std::to_integer<std::uint16_t>(buf_[pos_++]);
    out = static_cast<std::uint16_t>(a | (b << 8));
    return true;
}

bool BufferReader::read_u32(std::uint32_t& out) noexcept
{
    if (pos_ + 4 > buf_.size())
        return false;
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
    {
        v |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(buf_[pos_++])) << (8 * i);
    }
    out = v;
    return true;
}

bool BufferReader::read_u64(std::uint64_t& out) noexcept
{
    if (pos_ + 8 > buf_.size())
        return false;
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
    {
        v |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(buf_[pos_++])) << (8 * i);
    }
    out = v;
    return true;
}

bool BufferReader::read_i64(std::int64_t& out) noexcept
{
    std::uint64_t u = 0;
    if (!read_u64(u))
        return false;
    out = static_cast<std::int64_t>(u);
    return true;
}

// ---- Header --------------------------------------------------------------

bool encode_header(BufferWriter& w, const MessageHeader& h) noexcept
{
    return w.write_u8(h.protocol_version) && w.write_u8(static_cast<std::uint8_t>(h.msg_type)) &&
           w.write_u16(h.body_length) && w.write_u32(h.sequence);
}

std::optional<MessageHeader> decode_header(BufferReader& r) noexcept
{
    MessageHeader h{};
    std::uint8_t mt = 0;
    if (!r.read_u8(h.protocol_version))
        return std::nullopt;
    if (!r.read_u8(mt))
        return std::nullopt;
    if (!r.read_u16(h.body_length))
        return std::nullopt;
    if (!r.read_u32(h.sequence))
        return std::nullopt;
    h.msg_type = static_cast<MsgType>(mt);
    return h;
}

// ---- Body encode --------------------------------------------------------

namespace
{

[[nodiscard]] bool encode_body(BufferWriter& w, const MessageBody& body)
{
    return std::visit(
        [&](const auto& m) -> bool
        {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, LogonMsg>)
            {
                return w.write_u32(m.client_id);
            }
            else if constexpr (std::is_same_v<T, LogoutMsg> || std::is_same_v<T, HeartbeatMsg>)
            {
                return true;
            }
            else if constexpr (std::is_same_v<T, NewOrderMsg>)
            {
                return w.write_u64(m.client_order_id) && w.write_u32(m.instrument_id) &&
                       w.write_u8(m.side) && w.write_u8(m.order_type) && w.write_u8(m.tif) &&
                       w.write_u8(0) // padding to 8-byte alignment
                       && w.write_i64(m.price) && w.write_u64(m.quantity);
            }
            else if constexpr (std::is_same_v<T, CancelOrderMsg>)
            {
                return w.write_u64(m.client_order_id) && w.write_u32(m.instrument_id);
            }
            else if constexpr (std::is_same_v<T, OrderAckMsg>)
            {
                return w.write_u64(m.client_order_id) && w.write_u64(m.server_order_id);
            }
            else if constexpr (std::is_same_v<T, OrderRejectMsg>)
            {
                return w.write_u64(m.client_order_id) &&
                       w.write_u8(static_cast<std::uint8_t>(m.reason));
            }
            else if constexpr (std::is_same_v<T, FillMsg>)
            {
                return w.write_u64(m.client_order_id) && w.write_u64(m.server_order_id) &&
                       w.write_i64(m.price) && w.write_u64(m.quantity) && w.write_u8(m.post_status);
            }
            else if constexpr (std::is_same_v<T, CancelledMsg>)
            {
                return w.write_u64(m.client_order_id);
            }
            return false;
        },
        body);
}

[[nodiscard]] MsgType type_of(const MessageBody& body) noexcept
{
    return std::visit(
        [](const auto& m) -> MsgType
        {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, LogonMsg>)
                return MsgType::Logon;
            else if constexpr (std::is_same_v<T, LogoutMsg>)
                return MsgType::Logout;
            else if constexpr (std::is_same_v<T, HeartbeatMsg>)
                return MsgType::Heartbeat;
            else if constexpr (std::is_same_v<T, NewOrderMsg>)
                return MsgType::NewOrder;
            else if constexpr (std::is_same_v<T, CancelOrderMsg>)
                return MsgType::CancelOrder;
            else if constexpr (std::is_same_v<T, OrderAckMsg>)
                return MsgType::OrderAck;
            else if constexpr (std::is_same_v<T, OrderRejectMsg>)
                return MsgType::OrderReject;
            else if constexpr (std::is_same_v<T, FillMsg>)
                return MsgType::Fill;
            else
                return MsgType::Cancelled;
        },
        body);
}

} // namespace

std::size_t encode(std::span<std::byte> out, std::uint32_t seq, const MessageBody& body)
{
    if (out.size() < kHeaderSize)
        return 0;

    // Encode body to a scratch buffer first to know its length.
    std::array<std::byte, kMaxBodySize> scratch{};
    BufferWriter bw{scratch};
    if (!encode_body(bw, body))
        return 0;
    const std::size_t body_len = bw.pos();
    if (out.size() < kHeaderSize + body_len)
        return 0;
    if (body_len > kMaxBodySize)
        return 0;

    MessageHeader h{
        kProtocolVersion,
        type_of(body),
        static_cast<std::uint16_t>(body_len),
        seq,
    };
    BufferWriter hw{out};
    if (!encode_header(hw, h))
        return 0;
    for (std::size_t i = 0; i < body_len; ++i)
    {
        out[kHeaderSize + i] = scratch[i];
    }
    return kHeaderSize + body_len;
}

std::vector<std::byte> encode(std::uint32_t seq, const MessageBody& body)
{
    std::vector<std::byte> out(kHeaderSize + kMaxBodySize);
    const std::size_t n = encode(out, seq, body);
    out.resize(n);
    return out;
}

// ---- Body decode --------------------------------------------------------

std::optional<MessageBody> decode_body(const MessageHeader& header,
                                       std::span<const std::byte> body) noexcept
{
    BufferReader r{body};
    switch (header.msg_type)
    {
    case MsgType::Logon:
    {
        LogonMsg m{};
        if (!r.read_u32(m.client_id))
            return std::nullopt;
        return MessageBody{m};
    }
    case MsgType::Logout:
        return MessageBody{LogoutMsg{}};
    case MsgType::Heartbeat:
        return MessageBody{HeartbeatMsg{}};
    case MsgType::NewOrder:
    {
        NewOrderMsg m{};
        std::uint8_t pad = 0;
        if (!r.read_u64(m.client_order_id))
            return std::nullopt;
        if (!r.read_u32(m.instrument_id))
            return std::nullopt;
        if (!r.read_u8(m.side))
            return std::nullopt;
        if (!r.read_u8(m.order_type))
            return std::nullopt;
        if (!r.read_u8(m.tif))
            return std::nullopt;
        if (!r.read_u8(pad))
            return std::nullopt;
        if (!r.read_i64(m.price))
            return std::nullopt;
        if (!r.read_u64(m.quantity))
            return std::nullopt;
        return MessageBody{m};
    }
    case MsgType::CancelOrder:
    {
        CancelOrderMsg m{};
        if (!r.read_u64(m.client_order_id))
            return std::nullopt;
        if (!r.read_u32(m.instrument_id))
            return std::nullopt;
        return MessageBody{m};
    }
    case MsgType::OrderAck:
    {
        OrderAckMsg m{};
        if (!r.read_u64(m.client_order_id))
            return std::nullopt;
        if (!r.read_u64(m.server_order_id))
            return std::nullopt;
        return MessageBody{m};
    }
    case MsgType::OrderReject:
    {
        OrderRejectMsg m{};
        std::uint8_t reason = 0;
        if (!r.read_u64(m.client_order_id))
            return std::nullopt;
        if (!r.read_u8(reason))
            return std::nullopt;
        m.reason = static_cast<RejectReason>(reason);
        return MessageBody{m};
    }
    case MsgType::Fill:
    {
        FillMsg m{};
        if (!r.read_u64(m.client_order_id))
            return std::nullopt;
        if (!r.read_u64(m.server_order_id))
            return std::nullopt;
        if (!r.read_i64(m.price))
            return std::nullopt;
        if (!r.read_u64(m.quantity))
            return std::nullopt;
        if (!r.read_u8(m.post_status))
            return std::nullopt;
        return MessageBody{m};
    }
    case MsgType::Cancelled:
    {
        CancelledMsg m{};
        if (!r.read_u64(m.client_order_id))
            return std::nullopt;
        return MessageBody{m};
    }
    }
    return std::nullopt;
}

} // namespace velox::protocol
