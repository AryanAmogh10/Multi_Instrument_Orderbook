#pragma once

#include "velox/core/types.hpp"
#include "velox/instruments/contract.hpp"

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace velox
{

// All option contracts for one underlying, indexed by (expiry, strike, type).
// Each entry maps to the InstrumentId of the corresponding order book.
class Chain
{
public:
    explicit Chain(std::string underlying);

    // Register a contract. Returns false if it's a duplicate or id already exists.
    bool add(InstrumentId id, const Contract& contract);

    // Remove by id. Returns false if not found.
    bool remove(InstrumentId id);

    // Exact lookup.
    [[nodiscard]] std::optional<InstrumentId>
    find(ExpiryDate expiry, Price strike, OptionType type) const noexcept;

    // All contracts (calls + puts) at a given expiry.
    [[nodiscard]] std::vector<InstrumentId> at_expiry(ExpiryDate expiry) const;

    [[nodiscard]] std::vector<InstrumentId> calls_at(ExpiryDate expiry) const;
    [[nodiscard]] std::vector<InstrumentId> puts_at(ExpiryDate expiry) const;

    // Sorted list of distinct expiry dates in the chain.
    [[nodiscard]] std::vector<ExpiryDate> expiries() const;

    // Contracts expiring on or before a given date — used for EOD sweeping.
    [[nodiscard]] std::vector<InstrumentId> expiring_on_or_before(ExpiryDate date) const;

    [[nodiscard]] const std::string& underlying() const noexcept { return underlying_; }
    [[nodiscard]] std::size_t size() const noexcept { return by_id_.size(); }

private:
    struct StrikeSlot
    {
        std::optional<InstrumentId> call;
        std::optional<InstrumentId> put;
    };

    std::string underlying_;
    // levels_[expiry][strike] -> {call id, put id}
    std::map<ExpiryDate, std::map<Price, StrikeSlot>> levels_;
    // id -> (expiry, strike) for O(1) remove
    std::unordered_map<std::uint32_t, std::pair<ExpiryDate, Price>> by_id_;
    std::unordered_map<std::uint32_t, OptionType> id_to_type_;
};

// keep old name
using OptionChain = Chain;

} // namespace velox
