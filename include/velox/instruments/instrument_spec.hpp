#pragma once

#include "velox/core/types.hpp"

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
    std::int64_t    tick_size;   // smallest price increment, in price ticks
    std::uint64_t   lot_size;    // minimum tradeable quantity
    std::string     currency;    // ISO 4217-ish, e.g. "USD"
};

}  // namespace velox
