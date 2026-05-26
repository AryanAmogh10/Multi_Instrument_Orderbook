#pragma once

#include "velox/core/types.hpp"
#include "velox/instruments/option_contract.hpp"

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace velox {

// Organizes all option contracts for a single underlying symbol.
// Indexed by (expiry, strike, OptionType) for efficient chain queries.
//
// Each entry holds the InstrumentId of the corresponding OrderBook so the
// matching engine can route orders to the right book.
class OptionChain {
public:
    explicit OptionChain(std::string underlying);

    // Register a contract.  Returns false if an identical (expiry,strike,type)
    // entry already exists or the InstrumentId is already registered.
    bool add(InstrumentId id, const OptionContract& contract);

    // Remove a contract by InstrumentId.  Returns false if not found.
    bool remove(InstrumentId id);

    // Exact lookup: (expiry, strike, type) → InstrumentId.
    [[nodiscard]] std::optional<InstrumentId> find(
        ExpiryDate expiry, Price strike, OptionType type) const noexcept;

    // All contracts (calls + puts, all strikes) at a given expiry.
    [[nodiscard]] std::vector<InstrumentId> at_expiry(ExpiryDate expiry) const;

    // Only calls / only puts at a given expiry.
    [[nodiscard]] std::vector<InstrumentId> calls_at(ExpiryDate expiry) const;
    [[nodiscard]] std::vector<InstrumentId> puts_at(ExpiryDate expiry) const;

    // Sorted list of distinct expiry dates present in the chain.
    [[nodiscard]] std::vector<ExpiryDate> expiries() const;

    // All contracts whose expiry is on or before `date` (for sweeping expired
    // contracts at end of day).
    [[nodiscard]] std::vector<InstrumentId> expiring_on_or_before(ExpiryDate date) const;

    [[nodiscard]] const std::string& underlying() const noexcept { return underlying_; }
    [[nodiscard]] std::size_t size() const noexcept { return by_id_.size(); }

private:
    struct StrikeSlot {
        std::optional<InstrumentId> call;
        std::optional<InstrumentId> put;
    };

    std::string underlying_;
    // levels_[expiry][strike] → {call id, put id}
    std::map<ExpiryDate, std::map<Price, StrikeSlot>> levels_;
    // id → (expiry, strike) for O(1) remove
    std::unordered_map<std::uint32_t, std::pair<ExpiryDate, Price>> by_id_;
    std::unordered_map<std::uint32_t, OptionType> id_to_type_;
};

}  // namespace velox
