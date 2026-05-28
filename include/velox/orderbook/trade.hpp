#pragma once

#include "velox/core/types.hpp"

namespace velox
{

struct Trade
{
    OrderId maker_id;
    OrderId taker_id;
    InstrumentId instrument;
    Price price;
    Quantity quantity;
    Timestamp ts;
};

} // namespace velox
