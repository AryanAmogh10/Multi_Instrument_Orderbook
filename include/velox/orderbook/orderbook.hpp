#pragma once

#include "velox/orderbook/order.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

namespace velox {

// Intrusive doubly-linked list for orders at a single price level.
// Uses the level_prev / level_next pointers embedded directly in Order,
// so there's no separate node allocation per order.
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

    void pop_front() noexcept {
        Order* o = head;
        head = o->level_next;
        if (head) head->level_prev = nullptr;
        else      tail = nullptr;
        o->level_prev = o->level_next = nullptr;
    }

    // O(1) removal anywhere in the list thanks to the intrusive links.
    void erase(Order* o) noexcept {
        if (o->level_prev) o->level_prev->level_next = o->level_next;
        else               head = o->level_next;
        if (o->level_next) o->level_next->level_prev = o->level_prev;
        else               tail = o->level_prev;
        o->level_prev = o->level_next = nullptr;
    }

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

// Single-instrument order book. Pure state container — matching logic lives in
// BookMatcher so the two can be tested independently.
//
// OrderPtr is a raw non-owning pointer into an OrderPool slab.
// The pool caller must keep orders alive for as long as they're in this book.
class OrderBook {
public:
    using OrderPtr  = Order*;
    using OrderList = LevelList;
    using BidLevels = std::map<Price, LevelList, std::greater<>>;
    using AskLevels = std::map<Price, LevelList, std::less<>>;

    explicit OrderBook(InstrumentId id) noexcept : instrument_(id) {}

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) noexcept = default;
    OrderBook& operator=(OrderBook&&) noexcept = default;

    [[nodiscard]] InstrumentId instrument() const noexcept { return instrument_; }

    void add_resting(Order* order);
    bool cancel(OrderId id);

    // Remove by id and return the pointer — caller is responsible for recycling it.
    [[nodiscard]] Order* cancel_and_get(OrderId id);

    [[nodiscard]] Order* find(OrderId id) const;

    [[nodiscard]] std::optional<Price> best_bid() const noexcept;
    [[nodiscard]] std::optional<Price> best_ask() const noexcept;

    [[nodiscard]] Quantity bid_qty_at(Price p) const;
    [[nodiscard]] Quantity ask_qty_at(Price p) const;

    [[nodiscard]] std::size_t order_count() const noexcept { return index_.size(); }
    [[nodiscard]] bool empty() const noexcept { return index_.empty(); }

    [[nodiscard]] Order* peek_top(Side side) const;
    void pop_top(Side side);

    [[nodiscard]] const BidLevels& bids() const noexcept { return bids_; }
    [[nodiscard]] const AskLevels& asks() const noexcept { return asks_; }

    // Visit every resting order via callback, then empty the book.
    // Used during instrument expiry to recycle all slots back to the pool.
    void clear_and_drain(const std::function<void(Order*)>& callback);

private:
    struct Locator {
        Side   side;
        Price  price;
        Order* order;
    };

    InstrumentId                                instrument_;
    BidLevels                                   bids_;
    AskLevels                                   asks_;
    std::unordered_map<std::uint64_t, Locator>  index_;
};

}  // namespace velox
