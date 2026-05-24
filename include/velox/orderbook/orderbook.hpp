#pragma once

#include "velox/orderbook/order.hpp"

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

namespace velox {

// Single-instrument order book — pure state container with price-time priority.
// Matching logic lives in MatchingEngine (roadmap §1.3: separate state from matcher).
class OrderBook {
public:
    using OrderPtr  = std::shared_ptr<Order>;
    using OrderList = std::list<OrderPtr>;
    using BidLevels = std::map<Price, OrderList, std::greater<>>;
    using AskLevels = std::map<Price, OrderList, std::less<>>;

    explicit OrderBook(InstrumentId id) noexcept : instrument_(id) {}

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) noexcept = default;
    OrderBook& operator=(OrderBook&&) noexcept = default;

    [[nodiscard]] InstrumentId instrument() const noexcept { return instrument_; }

    // Insert an order at the back of its price level (price-time priority).
    void add_resting(const OrderPtr& order);

    // Remove a resting order by id. Returns true if found.
    bool cancel(OrderId id);

    [[nodiscard]] OrderPtr find(OrderId id) const;

    [[nodiscard]] std::optional<Price> best_bid() const noexcept;
    [[nodiscard]] std::optional<Price> best_ask() const noexcept;

    [[nodiscard]] Quantity bid_qty_at(Price p) const;
    [[nodiscard]] Quantity ask_qty_at(Price p) const;

    [[nodiscard]] std::size_t order_count() const noexcept { return index_.size(); }
    [[nodiscard]] bool empty() const noexcept { return index_.empty(); }

    // --- Matching primitives (used by MatchingEngine) ----------------------

    // The front-most order on the best level of `side`, or nullptr if empty.
    [[nodiscard]] OrderPtr peek_top(Side side) const;

    // Drop the front-most order on the best level of `side` (used after a full
    // maker fill). UB if that side is empty.
    void pop_top(Side side);

    [[nodiscard]] const BidLevels& bids() const noexcept { return bids_; }
    [[nodiscard]] const AskLevels& asks() const noexcept { return asks_; }

private:
    struct Locator {
        Side                 side;
        Price                price;
        OrderList::iterator  it;
    };

    InstrumentId                                instrument_;
    BidLevels                                   bids_;
    AskLevels                                   asks_;
    std::unordered_map<std::uint64_t, Locator>  index_;
};

}  // namespace velox
