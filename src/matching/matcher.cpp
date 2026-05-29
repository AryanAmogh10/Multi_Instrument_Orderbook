#include "ordbk/matching/matcher.hpp"

namespace ordbk
{

bool Matcher::prices_cross(Side taker_side, Price taker_limit, Price maker_price) noexcept
{
    if (taker_side == Side::Buy)
    {
        return to_underlying(maker_price) <= to_underlying(taker_limit);
    }
    return to_underlying(maker_price) >= to_underlying(taker_limit);
}

bool Matcher::can_fully_fill(const Order& taker) const
{
    const Side opp = opposite(taker.side);
    Quantity need = taker.remaining();

    auto walk = [&](const auto& levels) -> bool
    {
        for (const auto& [price, list] : levels)
        {
            if (taker.type == OrderType::Limit &&
                !prices_cross(taker.side, taker.limit_price, price))
            {
                break;
            }
            for (Order* o : list)
            {
                if (to_underlying(o->remaining()) >= to_underlying(need))
                    return true;
                need -= o->remaining();
            }
        }
        return to_underlying(need) == 0;
    };

    if (opp == Side::Buy)
        return walk(book_.bids());
    return walk(book_.asks());
}

void Matcher::match(Order& taker, std::vector<Trade>& trades)
{
    const Side opp = opposite(taker.side);
    while (to_underlying(taker.remaining()) > 0)
    {
        Order* maker = book_.peek_top(opp);
        if (!maker)
            break;

        if (taker.type == OrderType::Limit &&
            !prices_cross(taker.side, taker.limit_price, maker->limit_price))
        {
            break;
        }

        const Quantity fill = qty_min(taker.remaining(), maker->remaining());
        const Price trade_price = maker->limit_price; // maker sets the price

        taker.filled_qty += fill;
        maker->filled_qty += fill;

        trades.push_back(Trade{
            maker->id,
            taker.id,
            taker.instrument,
            trade_price,
            fill,
            taker.ts,
        });

        if (maker->is_fully_filled())
        {
            maker->status = OrderStatus::Filled;
            book_.pop_top(opp);
            pool_.release(maker);
        }
        else
        {
            maker->status = OrderStatus::PartiallyFilled;
        }
    }
}

MatchResult Matcher::submit(Order* order)
{
    const std::uint64_t t0 = now_ns();

    MatchResult result{order, {}};
    auto reject = [&]
    {
        order->status = OrderStatus::Rejected;
        return result;
    };

    if (to_underlying(order->initial_qty) == 0)
        return reject();
    if (order->instrument != book_.instrument())
        return reject();
    if (order->type == OrderType::Limit && to_underlying(order->limit_price) <= 0)
        return reject();
    if (order->type == OrderType::Market &&
        (order->tif == TimeInForce::GTC || order->tif == TimeInForce::Day))
    {
        return reject();
    }

    if (order->tif == TimeInForce::FOK && !can_fully_fill(*order))
        return reject();

    match(*order, result.trades);

    if (order->is_fully_filled())
    {
        order->status = OrderStatus::Filled;
    }
    else if (order->type == OrderType::Market || order->tif == TimeInForce::IOC)
    {
        order->status = to_underlying(order->filled_qty) > 0 ? OrderStatus::PartiallyFilled
                                                             : OrderStatus::Cancelled;
    }
    else
    {
        // GTC limit: rest the unfilled remainder
        order->status =
            to_underlying(order->filled_qty) > 0 ? OrderStatus::PartiallyFilled : OrderStatus::New;
        book_.add_resting(order);
    }

    order->match_complete_ns = Timestamp{static_cast<std::int64_t>(now_ns())};
    stats_.record(now_ns() - t0);

    return result;
}

bool Matcher::cancel(OrderId id)
{
    Order* o = book_.cancel_and_get(id);
    if (!o)
        return false;
    o->status = OrderStatus::Cancelled;
    pool_.release(o);
    return true;
}

void Matcher::cancel_all()
{
    book_.clear_and_drain(
        [this](Order* o)
        {
            o->status = OrderStatus::Cancelled;
            pool_.release(o);
        });
}

} // namespace ordbk
