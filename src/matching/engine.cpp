#include "ordbk/matching/engine.hpp"

#include <stdexcept>

namespace ordbk
{

Engine::Engine(const InstrumentRegistry& registry, Pool& pool, ShardFilter filter)
    : registry_(registry), pool_(pool)
{
    if (!registry.frozen())
    {
        throw std::logic_error{"registry must be frozen before Engine creation"};
    }
    for (const auto& spec : registry.all())
    {
        if (filter && !filter(spec.id))
            continue;
        auto book = std::make_unique<OrderBook>(spec.id);
        auto matcher = std::make_unique<Matcher>(*book, pool_);
        book_ptrs_.emplace(to_underlying(spec.id), book.get());
        matchers_.emplace(to_underlying(spec.id), Slot{std::move(book), std::move(matcher)});
    }
}

MatchResult Engine::submit(Order* order)
{
    auto it = matchers_.find(to_underlying(order->instrument));
    if (it == matchers_.end())
    {
        order->status = OrderStatus::Rejected;
        return MatchResult{order, {}};
    }
    return it->second.matcher->submit(order);
}

bool Engine::cancel(InstrumentId instrument, OrderId id)
{
    auto it = matchers_.find(to_underlying(instrument));
    if (it == matchers_.end())
        return false;
    return it->second.matcher->cancel(id);
}

bool Engine::expire_instrument(InstrumentId id)
{
    auto it = matchers_.find(to_underlying(id));
    if (it == matchers_.end())
        return false;
    it->second.matcher->cancel_all();
    book_ptrs_.erase(to_underlying(id));
    matchers_.erase(it);
    return true;
}

const OrderBook* Engine::book(InstrumentId id) const noexcept
{
    auto it = book_ptrs_.find(to_underlying(id));
    return it == book_ptrs_.end() ? nullptr : it->second;
}

OrderBook* Engine::book(InstrumentId id) noexcept
{
    auto it = book_ptrs_.find(to_underlying(id));
    return it == book_ptrs_.end() ? nullptr : it->second;
}

LatencyTracker::Snapshot Engine::latency_snapshot() const noexcept
{
    LatencyTracker::Snapshot combined{};
    for (const auto& [id, slot] : matchers_)
    {
        const auto s = slot.matcher->latency_stats().snapshot();
        combined.count += s.count;
        combined.max_ns = std::max(combined.max_ns, s.max_ns);
        for (std::size_t i = 0; i < LatencyTracker::kBuckets; ++i)
            combined.hist[i] += s.hist[i];
    }
    return combined;
}

} // namespace ordbk
