#pragma once

#include "velox/orderbook/orderbook.hpp"
#include "velox/orderbook/trade.hpp"
#include "velox/utils/latency_stats.hpp"
#include "velox/utils/order_pool.hpp"

#include <vector>

namespace velox {

struct SubmitResult {
    Order*             order{nullptr};  // the taker — always non-null
    std::vector<Trade> trades;
};

// Matches incoming orders against a single OrderBook.
//
// Memory management contract:
//   - Fully-filled makers are released to the pool inside match().
//   - Cancelled resting orders are released inside cancel().
//   - Terminal takers are returned to the caller via SubmitResult;
//     the caller must release them after reading the result fields.
//
// Supported order types:
//   Limit  (GTC, IOC, FOK, Day)
//   Market (IOC, FOK)
class BookMatcher {
public:
    explicit BookMatcher(OrderBook& book, OrderPool& pool) noexcept
        : book_(book), pool_(pool) {}

    // Submit a taker order. The caller owns the returned order and must
    // release it to the pool if result.order->is_terminal().
    SubmitResult submit(Order* order);

    // Cancel a resting order and return the slot to the pool.
    bool cancel(OrderId id);

    // Cancel every resting order in the book and recycle all slots.
    // Called by MatchingEngine::expire_instrument().
    void cancel_all();

    [[nodiscard]] const OrderBook& book() const noexcept { return book_; }
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
