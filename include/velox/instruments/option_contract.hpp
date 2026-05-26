#pragma once

#include "velox/core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace velox {

enum class OptionType : std::uint8_t { Call, Put };
enum class OptionStyle : std::uint8_t { American, European };
enum class SettlementType : std::uint8_t { Physical, Cash };

// Calendar date for option expiry.  Ordered chronologically.
struct ExpiryDate {
    std::uint16_t year{};
    std::uint8_t  month{};
    std::uint8_t  day{};

    constexpr auto operator<=>(const ExpiryDate&) const noexcept = default;
    constexpr bool operator==(const ExpiryDate&) const noexcept = default;

    // Returns "YYYY-MM-DD".
    std::string to_string() const;
};

// Full specification of a single-leg options contract.
//
// Pricing convention: `strike` is stored in price ticks where 1 tick = 1 cent.
// $150.00 → Price{15000}.
//
// OCC symbol format: "<ROOT padded to 6><YYMMDD><C|P><8-digit-strike-thousandths>"
// Example: "AAPL  260117C00150000" ($150.00 call expiring 2026-01-17).
struct OptionContract {
    std::string    underlying;                          // "AAPL"
    Price          strike{};                            // in price ticks (1 tick = 1 cent)
    ExpiryDate     expiry{};
    OptionType     option_type{OptionType::Call};
    OptionStyle    style{OptionStyle::American};
    std::uint32_t  multiplier{100};                     // 100 for standard equity options
    SettlementType settlement{SettlementType::Physical};

    // Produce an OCC-style 21-character symbol string.
    std::string occ_symbol() const;

    // Parse an OCC-style symbol.
    // Throws std::invalid_argument on malformed input.
    static OptionContract from_occ(std::string_view occ);
};

}  // namespace velox
