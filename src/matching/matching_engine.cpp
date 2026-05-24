#include "velox/matching/matching_engine.hpp"

#include <stdexcept>

namespace velox {

MatchingEngine::MatchingEngine(const InstrumentRegistry& registry, InstrumentFilter filter)
    : registry_(registry) {
    if (!registry.frozen()) {
        throw std::logic_error{"InstrumentRegistry must be frozen before MatchingEngine creation"};
    }
    for (const auto& spec : registry.all()) {
        if (filter && !filter(spec.id)) continue;
        auto book = std::make_unique<OrderBook>(spec.id);
        auto matcher = std::make_unique<BookMatcher>(*book);
        books_.emplace(to_underlying(spec.id), book.get());
        slots_.emplace(to_underlying(spec.id), Slot{std::move(book), std::move(matcher)});
    }
}

SubmitResult MatchingEngine::submit(OrderBook::OrderPtr order) {
    auto it = slots_.find(to_underlying(order->instrument));
    if (it == slots_.end()) {
        order->status = OrderStatus::Rejected;
        return SubmitResult{order, {}};
    }
    return it->second.matcher->submit(std::move(order));
}

bool MatchingEngine::cancel(InstrumentId instrument, OrderId id) {
    auto it = slots_.find(to_underlying(instrument));
    if (it == slots_.end()) return false;
    return it->second.matcher->cancel(id);
}

const OrderBook* MatchingEngine::book(InstrumentId id) const noexcept {
    auto it = books_.find(to_underlying(id));
    return it == books_.end() ? nullptr : it->second;
}

OrderBook* MatchingEngine::book(InstrumentId id) noexcept {
    auto it = books_.find(to_underlying(id));
    return it == books_.end() ? nullptr : it->second;
}

}  // namespace velox
