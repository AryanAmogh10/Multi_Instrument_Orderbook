#pragma once

#include "velox/orderbook/order.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

namespace velox {

// Phase 4 §4.3: Intrusive doubly-linked list for orders at a single price
// level.  Uses the level_prev / level_next pointers embedded in Order so
// there is zero per-node allocation — the only heap memory is the Order
// slab itself (owned by OrderPool).
struct LevelList {
    Order* head{nullptr};
    Order* tail{nullptr};

    [[nodiscard]] bool empty() const noexcept { return head == nullptr; }
    [[nodiscard]] Order* front() const noexcept { return head; }

    void push_back(Order* o) noexcept {
        o->level_prev = tail;
        o->level_next = nullptr;
        if (tail) tail->level_next = o;
        else      head = o;
        tail = o;
    }

    // Remove the front order (called after a full maker fill).
    void pop_front() noexcept {
        Order* o = head;
        head = o->level_next;
        if (head) head->level_prev = nullptr;
        else      tail = nullptr;
        o->level_prev = o->level_next = nullptr;
    }

    // O(1) removal by pointer (iterator-free thanks to intrusive links).
    void erase(Order* o) noexcept {
        if (o->level_prev) o->level_prev->level_next = o->level_next;
        else               head = o->level_next;
        if (o->level_next) o->level_next->level_prev = o->level_prev;
        else               tail = o->level_prev;
        o->level_prev = o->level_next = nullptr;
    }

    // Support range-for so tests and the invariant walker can iterate levels.
    struct iterator {
        Order* cur;
        explicit iterator(Order* o) noexcept : cur(o) {}
        Order* operator*() const noexcept { return cur; }
        iterator& operator++() noexcept { cur = cur->level_next; return *this; }
        bool operator!=(const iterator& rhs) const noexcept { return cur != rhs.cur; }
    };
    [[nodiscard]] iterator begin() const noexcept { return iterator{head}; }
    [[nodiscard]] iterator end()   const noexcept { return iterator{nullptr}; }
};

// Single-instrument order book — pure state container with price-time priority.
// Matching logic lives in BookMatcher (roadmap §1.3).
//
// Phase 4: OrderPtr is now a raw, non-owning pointer into an OrderPool slab.
// The pool caller must ensure orders outlive any reference held by this book.
class OrderBook {
public:
    using OrderPtr  = Order*;                                       // non-owning
    using OrderList = LevelList;
    using BidLevels = std::map<Price, LevelList, std::greater<>>;
    using AskLevels = std::map<Price, LevelList, std::less<>>;

    explicit OrderBook(InstrumentId id) noexcept : instrument_(id) {}

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) noexcept = default;
    OrderBook& operator=(OrderBook&&) noexcept = default;

    [[nodiscard]] InstrumentId instrument() const noexcept { return instrument_; }

    // Insert an order at the back of its price level (price-time priority).
    void add_resting(Order* order);

    // Remove a resting order by id. Returns true if found.
    bool cancel(OrderId id);

    // Remove a resting order by id and return its pointer.
    // Returns nullptr if not found.  Used by MatchingEngine to recycle the
    // Order back to the pool.
    [[nodiscard]] Order* cancel_and_get(OrderId id);

    [[nodiscard]] Order* find(OrderId id) const;

    [[nodiscard]] std::optional<Price> best_bid() const noexcept;
    [[nodiscard]] std::optional<Price> best_ask() const noexcept;

    [[nodiscard]] Quantity bid_qty_at(Price p) const;
    [[nodiscard]] Quantity ask_qty_at(Price p) const;

    [[nodiscard]] std::size_t order_count() const noexcept { return index_.size(); }
    [[nodiscard]] bool empty() const noexcept { return index_.empty(); }

    // --- Matching primitives (used by BookMatcher) ---------------------------

    // The front-most order on the best level of `side`, or nullptr if empty.
    [[nodiscard]] Order* peek_top(Side side) const;

    // Drop the front-most order on the best level of `side`.
    // UB if that side is empty.  Does NOT release the order to any pool —
    // the caller (BookMatcher) is responsible for that.
    void pop_top(Side side);

    [[nodiscard]] const BidLevels& bids() const noexcept { return bids_; }
    [[nodiscard]] const AskLevels& asks() const noexcept { return asks_; }

    // Phase 5: visit every resting order via `callback`, then clear the book.
    // Used by BookMatcher::cancel_all() during instrument expiry.
    void clear_and_drain(const std::function<void(Order*)>& callback);

private:
    struct Locator {
        Side   side;
        Price  price;
        Order* order;   // direct pointer — no iterator needed
    };

    InstrumentId                                instrument_;
    BidLevels                                   bids_;
    AskLevels                                   asks_;
    std::unordered_map<std::uint64_t, Locator>  index_;
};

}  // namespace velox
