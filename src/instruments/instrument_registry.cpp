#include "velox/instruments/instrument_registry.hpp"

namespace velox
{

void InstrumentRegistry::add(InstrumentSpec spec)
{
    if (frozen_)
        throw FrozenError{};
    if (id_map_.contains(to_underlying(spec.id)))
    {
        throw DuplicateError{"duplicate InstrumentId"};
    }
    if (sym_map_.contains(spec.symbol))
    {
        throw DuplicateError{"duplicate symbol: " + spec.symbol};
    }
    // build chain index for option instruments
    if (spec.type == InstrumentType::Option && spec.option.has_value())
    {
        const std::string& underlying = spec.option->underlying;
        auto it = chains_.find(underlying);
        if (it == chains_.end())
        {
            it = chains_.emplace(underlying, Chain{underlying}).first;
        }
        it->second.add(spec.id, *spec.option);
    }

    const std::size_t idx = instruments_.size();
    id_map_.emplace(to_underlying(spec.id), idx);
    sym_map_.emplace(spec.symbol, idx);
    instruments_.push_back(std::move(spec));
}

const Chain* InstrumentRegistry::option_chain(std::string_view underlying) const noexcept
{
    auto it = chains_.find(std::string{underlying});
    return it == chains_.end() ? nullptr : &it->second;
}

std::vector<std::string> InstrumentRegistry::option_underlyings() const
{
    std::vector<std::string> result;
    result.reserve(chains_.size());
    for (const auto& [sym, chain] : chains_)
    {
        (void)chain;
        result.push_back(sym);
    }
    return result;
}

const InstrumentSpec* InstrumentRegistry::find(InstrumentId id) const noexcept
{
    auto it = id_map_.find(to_underlying(id));
    if (it == id_map_.end())
        return nullptr;
    return &instruments_[it->second];
}

const InstrumentSpec* InstrumentRegistry::find(std::string_view symbol) const noexcept
{
    auto it = sym_map_.find(std::string{symbol});
    if (it == sym_map_.end())
        return nullptr;
    return &instruments_[it->second];
}

} // namespace velox
