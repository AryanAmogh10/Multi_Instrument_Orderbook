#pragma once

#include "velox/core/types.hpp"
#include "velox/instruments/contract.hpp"

#include <optional>
#include <string>

namespace velox {

enum class InstrumentType : std::uint8_t {
    Equity,
    Future,
    Option,
};

struct InstrumentSpec {
    InstrumentId    id;
    std::string     symbol;
    InstrumentType  type;
    std::int64_t    tick_size;   // smallest price increment in price ticks
    std::uint64_t   lot_size;    // minimum tradeable qty
    std::string     currency;    // e.g. "USD"
    // Non-null iff type == Option.
    std::optional<Contract> option{};
};

}  // namespace velox
