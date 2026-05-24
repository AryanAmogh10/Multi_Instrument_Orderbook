#include "velox/matching/book_matcher.hpp"

namespace velox {

bool BookMatcher::prices_cross(Side taker_side, Price taker_limit, Price maker_price) noexcept {
    if (taker_side == Side::Buy) {
        return to_underlying(maker_price) <= to_underlying(taker_limit);
    }
    return to_underlying(maker_price) >= to_underlying(taker_limit);
}

bool BookMatcher::can_fully_fill(const Order& taker) const {
    const Side opp = opposite(taker.side);
    Quantity need = taker.remaining();

    auto walk = [&](const auto& levels) -> bool {
        for (const auto& [price, list] : levels) {
            if (taker.type == OrderType::Limit &&
                !prices_cross(taker.side, taker.limit_price, price)) {
                break;
            }
            for (const auto& o : list) {
                if (to_underlying(o->remaining()) >= to_underlying(need)) return true;
                need -= o->remaining();
            }
        }
        return to_underlying(need) == 0;
    };

    if (opp == Side::Buy) return walk(book_.bids());
    return walk(book_.asks());
}

void BookMatcher::match(Order& taker, std::vector<Trade>& trades) {
    const Side opp = opposite(taker.side);
    while (to_underlying(taker.remaining()) > 0) {
        auto maker = book_.peek_top(opp);
        if (!maker) break;

        if (taker.type == OrderType::Limit &&
            !prices_cross(taker.side, taker.limit_price, maker->limit_price)) {
            break;
        }

        const Quantity fill = qty_min(taker.remaining(), maker->remaining());
        const Price trade_price = maker->limit_price;  // resting price wins

        taker.filled_qty += fill;
        maker->filled_qty += fill;

        trades.push_back(Trade{
            maker->id, taker.id, taker.instrument, trade_price, fill, taker.ts,
        });

        if (maker->is_fully_filled()) {
            maker->status = OrderStatus::Filled;
            book_.pop_top(opp);
        } else {
            maker->status = OrderStatus::PartiallyFilled;
        }
    }
}

SubmitResult BookMatcher::submit(OrderBook::OrderPtr order) {
    SubmitResult result{order, {}};
    auto reject = [&] { order->status = OrderStatus::Rejected; return result; };

    if (to_underlying(order->initial_qty) == 0) return reject();
    if (order->instrument != book_.instrument()) return reject();
    if (order->type == OrderType::Limit && to_underlying(order->limit_price) <= 0) return reject();
    if (order->type == OrderType::Market &&
        (order->tif == TimeInForce::GTC || order->tif == TimeInForce::Day)) {
        return reject();
    }

    if (order->tif == TimeInForce::FOK && !can_fully_fill(*order)) return reject();

    match(*order, result.trades);

    if (order->is_fully_filled()) {
        order->status = OrderStatus::Filled;
        return result;
    }

    if (order->type == OrderType::Market || order->tif == TimeInForce::IOC) {
        order->status = to_underlying(order->filled_qty) > 0
                            ? OrderStatus::PartiallyFilled
                            : OrderStatus::Cancelled;
        return result;
    }

    order->status = to_underlying(order->filled_qty) > 0
                        ? OrderStatus::PartiallyFilled
                        : OrderStatus::New;
    book_.add_resting(order);
    return result;
}

}  // namespace velox
