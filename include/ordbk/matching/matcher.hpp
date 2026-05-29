#pragma once

#include "ordbk/orderbook/orderbook.hpp"
#include "ordbk/orderbook/trade.hpp"
#include "ordbk/utils/latency.hpp"
#include "ordbk/utils/pool.hpp"

#include <vector>

namespace ordbk
{

// result from submitting an order
struct MatchResult
{
    Order* order{nullptr}; // the taker - always non-null
    std::vector<Trade> trades;
};

// keep old name working
using SubmitResult = MatchResult;

// Matches incoming orders against a single OrderBook.
//
// Memory contract:
//   - Fully-filled makers get released inside match().
//   - Cancelled resting orders get released inside cancel().
//   - Takers are returned via MatchResult; caller releases if is_terminal().
//
// Supports: Limit (GTC/IOC/FOK/Day), Market (IOC/FOK)
class Matcher
{
public:
    explicit Matcher(OrderBook& book, Pool& pool) noexcept : book_(book), pool_(pool) {}

    // Submit a taker. Caller must release result.order if it's terminal.
    MatchResult submit(Order* order);

    // Cancel a resting order and return its slot to the pool.
    bool cancel(OrderId id);

    // Nuke every resting order - used when an instrument expires.
    void cancel_all();

    [[nodiscard]] const OrderBook& book() const noexcept { return book_; }
    [[nodiscard]] const LatencyTracker& latency_stats() const noexcept { return stats_; }

private:
    [[nodiscard]] bool can_fully_fill(const Order& taker) const;
    void match(Order& taker, std::vector<Trade>& trades);
    [[nodiscard]] static bool
    prices_cross(Side taker_side, Price taker_limit, Price maker_price) noexcept;

    OrderBook& book_;
    Pool& pool_;
    LatencyTracker stats_;
};

// keep old name working
using BookMatcher = Matcher;

} // namespace ordbk
