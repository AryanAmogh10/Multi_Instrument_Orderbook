#pragma once

#include "velox/orderbook/orderbook.hpp"
#include "velox/orderbook/trade.hpp"
#include "velox/utils/latency_stats.hpp"
#include "velox/utils/order_pool.hpp"

#include <vector>

namespace velox {

struct SubmitResult {
    Order*             order{nullptr};  // the submitted (taker) order — always valid
    std::vector<Trade> trades;
};

// Matches orders against a single OrderBook.
//
// Phase 4: receives a reference to the OrderPool so it can release Orders
// back to the pool when they are fully consumed:
//   • Fully-filled makers are released inside match().
//   • Cancelled resting orders are released inside cancel().
//   • Rejected / IOC-cancelled takers are returned to the caller via
//     SubmitResult so the caller can read the status before releasing.
//
// Supported order types (same as Phase 3):
//   OrderType::Limit  with TIF: GTC, IOC, FOK, Day (Day == GTC at this phase)
//   OrderType::Market with TIF: IOC / FOK
class BookMatcher {
public:
    explicit BookMatcher(OrderBook& book, OrderPool& pool) noexcept
        : book_(book), pool_(pool) {}

    // Submit a taker order.  The caller is responsible for releasing the
    // returned order back to the pool if result.order->is_terminal().
    SubmitResult submit(Order* order);

    // Cancel a resting order.  On success the order is released to the pool.
    bool cancel(OrderId id);

    [[nodiscard]] const OrderBook& book() const noexcept { return book_; }

    // Phase 4 §4.5: cumulative latency statistics for this book.
    [[nodiscard]] const LatencyStats& latency_stats() const noexcept { return stats_; }

private:
    [[nodiscard]] bool can_fully_fill(const Order& taker) const;
    void match(Order& taker, std::vector<Trade>& trades);
    [[nodiscard]] static bool prices_cross(Side taker_side, Price taker_limit,
                                           Price maker_price) noexcept;

    OrderBook&   book_;
    OrderPool&   pool_;
    LatencyStats stats_;
};

}  // namespace velox
