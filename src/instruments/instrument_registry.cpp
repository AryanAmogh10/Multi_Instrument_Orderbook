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
    // Phase 5: build option chain index for option instruments.
    if (spec.type == InstrumentType::Option && spec.option.has_value()) {
        const std::string& underlying = spec.option->underlying;
        auto it = option_chains_.find(underlying);
        if (it == option_chains_.end()) {
            it = option_chains_.emplace(underlying, OptionChain{underlying}).first;
        }
        it->second.add(spec.id, *spec.option);
    }

    const std::size_t idx = specs_.size();
    by_id_.emplace(to_underlying(spec.id), idx);
    by_symbol_.emplace(spec.symbol, idx);
    specs_.push_back(std::move(spec));
}

const OptionChain* InstrumentRegistry::option_chain(std::string_view underlying) const noexcept {
    auto it = option_chains_.find(std::string{underlying});
    return it == option_chains_.end() ? nullptr : &it->second;
}

std::vector<std::string> InstrumentRegistry::option_underlyings() const {
    std::vector<std::string> result;
    result.reserve(option_chains_.size());
    for (const auto& [sym, _] : option_chains_) result.push_back(sym);
    return result;
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
