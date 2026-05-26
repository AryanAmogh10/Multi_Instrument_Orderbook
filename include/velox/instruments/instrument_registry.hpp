#pragma once

#include "velox/instruments/instrument_spec.hpp"
#include "velox/instruments/option_chain.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace velox {

// Registry is mutable while building (at startup), then frozen. After freeze()
// all lookups are safe to call concurrently from any thread.
class InstrumentRegistry {
public:
    class FrozenError : public std::logic_error {
    public:
        FrozenError() : std::logic_error("InstrumentRegistry is frozen") {}
    };
    class DuplicateError : public std::logic_error {
    public:
        explicit DuplicateError(const std::string& what) : std::logic_error(what) {}
    };

    void add(InstrumentSpec spec);

    void freeze() noexcept { frozen_ = true; }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }

    [[nodiscard]] const InstrumentSpec* find(InstrumentId id) const noexcept;
    [[nodiscard]] const InstrumentSpec* find(std::string_view symbol) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return specs_.size(); }
    [[nodiscard]] const std::vector<InstrumentSpec>& all() const noexcept { return specs_; }

    // Option chain queries (Phase 5).
    // Returns nullptr if no option contracts exist for the given underlying.
    [[nodiscard]] const OptionChain* option_chain(std::string_view underlying) const noexcept;

    // All underlying symbols that have at least one option contract registered.
    [[nodiscard]] std::vector<std::string> option_underlyings() const;

private:
    std::vector<InstrumentSpec>                         specs_;
    std::unordered_map<std::uint32_t, std::size_t>      by_id_;
    std::unordered_map<std::string, std::size_t>        by_symbol_;
    std::unordered_map<std::string, OptionChain>        option_chains_;
    bool                                                frozen_{false};
};

}  // namespace velox
