#include "velox/instruments/instrument_registry.hpp"

namespace velox {

void InstrumentRegistry::add(InstrumentSpec spec) {
    if (frozen_) throw FrozenError{};
    if (by_id_.contains(to_underlying(spec.id))) {
        throw DuplicateError{"duplicate InstrumentId"};
    }
    if (by_symbol_.contains(spec.symbol)) {
        throw DuplicateError{"duplicate symbol: " + spec.symbol};
    }
    const std::size_t idx = specs_.size();
    by_id_.emplace(to_underlying(spec.id), idx);
    by_symbol_.emplace(spec.symbol, idx);
    specs_.push_back(std::move(spec));
}

const InstrumentSpec* InstrumentRegistry::find(InstrumentId id) const noexcept {
    auto it = by_id_.find(to_underlying(id));
    if (it == by_id_.end()) return nullptr;
    return &specs_[it->second];
}

const InstrumentSpec* InstrumentRegistry::find(std::string_view symbol) const noexcept {
    auto it = by_symbol_.find(std::string{symbol});
    if (it == by_symbol_.end()) return nullptr;
    return &specs_[it->second];
}

}  // namespace velox
