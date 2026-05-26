#pragma once

#include "velox/core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace velox {

enum class OptionType : std::uint8_t { Call, Put };
enum class OptionStyle : std::uint8_t { American, European };
enum class SettlementType : std::uint8_t { Physical, Cash };

// Calendar date for expiry — comparable chronologically.
struct ExpiryDate {
    std::uint16_t year{};
    std::uint8_t  month{};
    std::uint8_t  day{};

    constexpr auto operator<=>(const ExpiryDate&) const noexcept = default;
    constexpr bool operator==(const ExpiryDate&) const noexcept = default;

    std::string to_string() const;
};

// Single-leg options contract.
// Strike stored in price ticks (1 tick = 1 cent), so $150.00 = Price{15000}.
//
// OCC format: "<ROOT 6 chars><YYMMDD><C|P><8-digit strike thousandths>"
struct Contract {
    std::string    underlying;
    Price          strike{};
    ExpiryDate     expiry{};
    OptionType     option_type{OptionType::Call};
    OptionStyle    style{OptionStyle::American};
    std::uint32_t  multiplier{100};
    SettlementType settlement{SettlementType::Physical};

    // Build the OCC-style 21-char symbol string.
    std::string occ_symbol() const;

    // Parse OCC symbol — throws std::invalid_argument on bad input.
    static Contract from_occ(std::string_view occ);
};

// keep old name working
using OptionContract = Contract;

}  // namespace velox
