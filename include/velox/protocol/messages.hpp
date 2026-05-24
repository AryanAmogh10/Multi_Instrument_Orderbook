#pragma once

#include "velox/core/types.hpp"
#include "velox/protocol/protocol.hpp"

#include <cstdint>
#include <variant>

namespace velox::protocol {

// ---- Session layer --------------------------------------------------------
struct LogonMsg {
    std::uint32_t client_id;
};
struct LogoutMsg {};
struct HeartbeatMsg {};

// ---- Order entry ----------------------------------------------------------
struct NewOrderMsg {
    std::uint64_t  client_order_id;
    std::uint32_t  instrument_id;
    std::uint8_t   side;            // 0 = Buy, 1 = Sell
    std::uint8_t   order_type;      // 0 = Limit, 1 = Market
    std::uint8_t   tif;             // 0 = GTC, 1 = IOC, 2 = FOK, 3 = Day
    std::int64_t   price;
    std::uint64_t  quantity;
};

struct CancelOrderMsg {
    std::uint64_t  client_order_id;
    std::uint32_t  instrument_id;
};

// ---- Outbound -------------------------------------------------------------
struct OrderAckMsg {
    std::uint64_t  client_order_id;
    std::uint64_t  server_order_id;
};

struct OrderRejectMsg {
    std::uint64_t  client_order_id;
    RejectReason   reason;
};

struct FillMsg {
    std::uint64_t  client_order_id;
    std::uint64_t  server_order_id;
    std::int64_t   price;
    std::uint64_t  quantity;
    std::uint8_t   post_status;     // OrderStatus underlying value
};

struct CancelledMsg {
    std::uint64_t  client_order_id;
};

using MessageBody = std::variant<
    LogonMsg, LogoutMsg, HeartbeatMsg,
    NewOrderMsg, CancelOrderMsg,
    OrderAckMsg, OrderRejectMsg, FillMsg, CancelledMsg>;

struct DecodedMessage {
    MessageHeader header;
    MessageBody   body;
};

}  // namespace velox::protocol
