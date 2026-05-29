#pragma once

#include "ordbk/instruments/chain.hpp"
#include "ordbk/instruments/instrument_spec.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ordbk
{

// Mutable while building, then freeze() makes it read-only.
// All lookups after freeze() are safe to call from any thread.
class InstrumentRegistry
{
public:
    class FrozenError : public std::logic_error
    {
    public:
        FrozenError() : std::logic_error("InstrumentRegistry is frozen") {}
    };
    class DuplicateError : public std::logic_error
    {
    public:
        explicit DuplicateError(const std::string& what) : std::logic_error(what) {}
    };

    void add(InstrumentSpec spec);

    void freeze() noexcept { frozen_ = true; }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }

    [[nodiscard]] const InstrumentSpec* find(InstrumentId id) const noexcept;
    [[nodiscard]] const InstrumentSpec* find(std::string_view symbol) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return instruments_.size(); }
    [[nodiscard]] const std::vector<InstrumentSpec>& all() const noexcept { return instruments_; }

    // Returns nullptr if no options exist for the given underlying.
    [[nodiscard]] const Chain* option_chain(std::string_view underlying) const noexcept;

    [[nodiscard]] std::vector<std::string> option_underlyings() const;

private:
    std::vector<InstrumentSpec> instruments_;
    std::unordered_map<std::uint32_t, std::size_t> id_map_;
    std::unordered_map<std::string, std::size_t> sym_map_;
    std::unordered_map<std::string, Chain> chains_;
    bool frozen_{false};
};

} // namespace ordbk
