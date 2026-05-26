#include "velox/matching/matching_engine.hpp"

#include <stdexcept>

namespace velox {

MatchingEngine::MatchingEngine(const InstrumentRegistry& registry,
                               OrderPool& pool,
                               InstrumentFilter filter)
    : registry_(registry), pool_(pool) {
    if (!registry.frozen()) {
        throw std::logic_error{"InstrumentRegistry must be frozen before MatchingEngine creation"};
    }
    for (const auto& spec : registry.all()) {
        if (filter && !filter(spec.id)) continue;
        auto book    = std::make_unique<OrderBook>(spec.id);
        auto matcher = std::make_unique<BookMatcher>(*book, pool_);
        books_.emplace(to_underlying(spec.id), book.get());
        slots_.emplace(to_underlying(spec.id), Slot{std::move(book), std::move(matcher)});
    }
}

SubmitResult MatchingEngine::submit(Order* order) {
    auto it = slots_.find(to_underlying(order->instrument));
    if (it == slots_.end()) {
        order->status = OrderStatus::Rejected;
        return SubmitResult{order, {}};
    }
    return it->second.matcher->submit(order);
}

bool MatchingEngine::cancel(InstrumentId instrument, OrderId id) {
    auto it = slots_.find(to_underlying(instrument));
    if (it == slots_.end()) return false;
    return it->second.matcher->cancel(id);
}

bool MatchingEngine::expire_instrument(InstrumentId id) {
    auto it = slots_.find(to_underlying(id));
    if (it == slots_.end()) return false;
    it->second.matcher->cancel_all();
    books_.erase(to_underlying(id));
    slots_.erase(it);
    return true;
}

const OrderBook* MatchingEngine::book(InstrumentId id) const noexcept {
    auto it = books_.find(to_underlying(id));
    return it == books_.end() ? nullptr : it->second;
}

OrderBook* MatchingEngine::book(InstrumentId id) noexcept {
    auto it = books_.find(to_underlying(id));
    return it == books_.end() ? nullptr : it->second;
}

LatencyStats::Snapshot MatchingEngine::latency_snapshot() const noexcept {
    LatencyStats::Snapshot combined{};
    for (const auto& [id, slot] : slots_) {
        const auto s = slot.matcher->latency_stats().snapshot();
        combined.count += s.count;
        combined.max_ns = std::max(combined.max_ns, s.max_ns);
        for (std::size_t i = 0; i < LatencyStats::kBuckets; ++i)
            combined.hist[i] += s.hist[i];
    }
    return combined;
}

}  // namespace velox
