#pragma once

#include "velox/core/types.hpp"

namespace velox {

struct Order {
    OrderId       id;
    InstrumentId  instrument;
    ClientId      client;
    Side          side;
    OrderType     type;
    TimeInForce   tif;
    Price         limit_price;  // ignored when type == Market
    Quantity      initial_qty;
    Quantity      filled_qty{kZeroQty};
    Timestamp     ts;
    OrderStatus   status{OrderStatus::New};

    [[nodiscard]] constexpr Quantity remaining() const noexcept {
        return initial_qty - filled_qty;
    }
    [[nodiscard]] constexpr bool is_fully_filled() const noexcept {
        return to_underlying(filled_qty) == to_underlying(initial_qty);
    }
    [[nodiscard]] constexpr bool is_terminal() const noexcept {
        return status == OrderStatus::Filled
            || status == OrderStatus::Cancelled
            || status == OrderStatus::Rejected;
    }
};

}  // namespace velox
