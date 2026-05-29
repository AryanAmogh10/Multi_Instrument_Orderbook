#pragma once

#include "ordbk/core/types.hpp"

namespace ordbk
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

} // namespace ordbk
