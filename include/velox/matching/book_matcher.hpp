#pragma once

#include "velox/orderbook/orderbook.hpp"
#include "velox/orderbook/trade.hpp"

#include <vector>

namespace velox {

struct SubmitResult {
    OrderBook::OrderPtr  order;
    std::vector<Trade>   trades;
};

// Matches orders against a single OrderBook. Phase 1 scope:
//   - OrderType::Limit  with TIF: GTC, IOC, FOK, Day  (Day == GTC at this phase)
//   - OrderType::Market with TIF: IOC / FOK
//
// Validation rejects: zero quantity, non-positive limit price, instrument
// mismatch, market orders with GTC/Day.
class BookMatcher {
public:
    explicit BookMatcher(OrderBook& book) noexcept : book_(book) {}

    SubmitResult submit(OrderBook::OrderPtr order);
    bool cancel(OrderId id) { return book_.cancel(id); }

    [[nodiscard]] const OrderBook& book() const noexcept { return book_; }

private:
    [[nodiscard]] bool can_fully_fill(const Order& taker) const;
    void match(Order& taker, std::vector<Trade>& trades);
    [[nodiscard]] static bool prices_cross(Side taker_side, Price taker_limit,
                                           Price maker_price) noexcept;

    OrderBook& book_;
};

}  // namespace velox
