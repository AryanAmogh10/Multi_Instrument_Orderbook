#pragma once

#include "ordbk/core/types.hpp"

namespace ordbk
{

// Core order record. Fields are ordered so the hot matching data
// (id, price, qty, status, side) sits in the first cache line.
struct Order
{
    OrderId id{};
    Price limit_price{}; // ignored for market orders
    Quantity initial_qty{};
    Quantity filled_qty{kZeroQty};
    InstrumentId instrument{};
    ClientId client{};
    OrderStatus status{OrderStatus::New};
    Side side{};
    OrderType type{};
    TimeInForce tif{};

    Timestamp ts{};
    Timestamp enqueue_ns{};        // stamped by the gateway on arrival
    Timestamp match_complete_ns{}; // stamped by the matcher on completion

    // Intrusive doubly-linked list pointers used while the order sits in a
    // price level inside an OrderBook. Null when not resting.
    Order* level_prev{nullptr};
    Order* level_next{nullptr};

    [[nodiscard]] constexpr Quantity remaining() const noexcept { return initial_qty - filled_qty; }
    [[nodiscard]] constexpr bool is_fully_filled() const noexcept
    {
        return to_underlying(filled_qty) == to_underlying(initial_qty);
    }
    [[nodiscard]] constexpr bool is_terminal() const noexcept
    {
        return status == OrderStatus::Filled || status == OrderStatus::Cancelled ||
               status == OrderStatus::Rejected;
    }
};

} // namespace ordbk
