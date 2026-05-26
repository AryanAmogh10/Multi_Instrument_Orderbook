#pragma once

#include "velox/core/types.hpp"

namespace velox {

// Phase 4 §4.3: fields reordered for cache locality — matching-critical data
// (id, price, qty, status, side) grouped in the first ~48 bytes so a single
// cache-line fetch covers the full hot path.
//
// Phase 4 §4.3: intrusive doubly-linked list pointers (level_prev/level_next)
// replace the separate std::list<OrderPtr> node allocation.  Pointers are
// nullptr when the order is not resting in a book.
struct Order {
    // ── hot fields ────────────────────────────────────────────────────────────
    OrderId       id{};
    Price         limit_price{};    // ignored when type == Market
    Quantity      initial_qty{};
    Quantity      filled_qty{kZeroQty};
    InstrumentId  instrument{};
    ClientId      client{};
    OrderStatus   status{OrderStatus::New};
    Side          side{};
    OrderType     type{};
    TimeInForce   tif{};

    // ── metadata ──────────────────────────────────────────────────────────────
    Timestamp     ts{};

    // Phase 4 §4.5: latency timestamps (nanoseconds, steady_clock).
    // enqueue_ns   — set by Dispatcher when order enters the engine.
    // match_complete_ns — set by BookMatcher when the result is ready.
    Timestamp     enqueue_ns{};
    Timestamp     match_complete_ns{};

    // ── intrusive list links (Phase 4 §4.3) ───────────────────────────────────
    // In use only while resting inside an OrderBook price level.
    Order*        level_prev{nullptr};
    Order*        level_next{nullptr};

    // ── helpers ───────────────────────────────────────────────────────────────
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
