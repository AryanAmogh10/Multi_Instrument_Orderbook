#pragma once

#include <cstdint>

namespace velox::protocol
{

inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::size_t kHeaderSize = 8;
inline constexpr std::size_t kMaxBodySize = 4096;

enum class MsgType : std::uint8_t
{
    Logon = 0x01,
    Logout = 0x02,
    Heartbeat = 0x03,
    NewOrder = 0x10,
    CancelOrder = 0x11,
    OrderAck = 0x20,
    OrderReject = 0x21,
    Fill = 0x22,
    Cancelled = 0x23,
};

enum class RejectReason : std::uint8_t
{
    Unknown = 0,
    UnknownInstrument = 1,
    InvalidQuantity = 2,
    InvalidPrice = 3,
    InvalidTif = 4,
    NotLoggedOn = 5,
    DuplicateOrderId = 6,
    InsufficientLiquidity = 7,
};

struct MessageHeader
{
    std::uint8_t protocol_version;
    MsgType msg_type;
    std::uint16_t body_length;
    std::uint32_t sequence;
};

} // namespace velox::protocol
