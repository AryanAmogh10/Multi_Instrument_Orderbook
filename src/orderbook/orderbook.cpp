#include "velox/orderbook/orderbook.hpp"

namespace velox {

void OrderBook::add_resting(Order* order) {
    if (order->side == Side::Buy) {
        auto& list = bids_[order->limit_price];
        list.push_back(order);
        index_.emplace(to_underlying(order->id),
                       Locator{Side::Buy, order->limit_price, order});
    } else {
        auto& list = asks_[order->limit_price];
        list.push_back(order);
        index_.emplace(to_underlying(order->id),
                       Locator{Side::Sell, order->limit_price, order});
    }
}

bool OrderBook::cancel(OrderId id) {
    return cancel_and_get(id) != nullptr;
}

Order* OrderBook::cancel_and_get(OrderId id) {
    auto idx_it = index_.find(to_underlying(id));
    if (idx_it == index_.end()) return nullptr;

    const Locator loc = idx_it->second;
    index_.erase(idx_it);

    if (loc.side == Side::Buy) {
        auto level_it = bids_.find(loc.price);
        level_it->second.erase(loc.order);
        if (level_it->second.empty()) bids_.erase(level_it);
    } else {
        auto level_it = asks_.find(loc.price);
        level_it->second.erase(loc.order);
        if (level_it->second.empty()) asks_.erase(level_it);
    }
    return loc.order;
}

Order* OrderBook::find(OrderId id) const {
    auto idx_it = index_.find(to_underlying(id));
    return idx_it == index_.end() ? nullptr : idx_it->second.order;
}

std::optional<Price> OrderBook::best_bid() const noexcept {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const noexcept {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

Quantity OrderBook::bid_qty_at(Price p) const {
    auto it = bids_.find(p);
    if (it == bids_.end()) return kZeroQty;
    Quantity total = kZeroQty;
    for (Order* o : it->second) total += o->remaining();
    return total;
}

Quantity OrderBook::ask_qty_at(Price p) const {
    auto it = asks_.find(p);
    if (it == asks_.end()) return kZeroQty;
    Quantity total = kZeroQty;
    for (Order* o : it->second) total += o->remaining();
    return total;
}

Order* OrderBook::peek_top(Side side) const {
    if (side == Side::Buy) {
        return bids_.empty() ? nullptr : bids_.begin()->second.front();
    }
    return asks_.empty() ? nullptr : asks_.begin()->second.front();
}

void OrderBook::pop_top(Side side) {
    if (side == Side::Buy) {
        auto level_it = bids_.begin();
        auto& list = level_it->second;
        index_.erase(to_underlying(list.front()->id));
        list.pop_front();
        if (list.empty()) bids_.erase(level_it);
    } else {
        auto level_it = asks_.begin();
        auto& list = level_it->second;
        index_.erase(to_underlying(list.front()->id));
        list.pop_front();
        if (list.empty()) asks_.erase(level_it);
    }
}

}  // namespace velox
