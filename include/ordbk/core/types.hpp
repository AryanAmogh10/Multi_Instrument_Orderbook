#pragma once

#include <cstdint>
#include <type_traits>

namespace ordbk
{

// Strong types - prevent accidental mixing of e.g. Price and Quantity at compile time.
// Integer ticks only, never floats.
enum class Price : std::int64_t
{
};
enum class Quantity : std::uint64_t
{
};
enum class OrderId : std::uint64_t
{
};
enum class InstrumentId : std::uint32_t
{
};
enum class ClientId : std::uint32_t
{
};
enum class Timestamp : std::int64_t
{
};

template <class E>
constexpr auto to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

constexpr Quantity kZeroQty{0};

constexpr Quantity operator+(Quantity a, Quantity b) noexcept
{
    return Quantity{to_underlying(a) + to_underlying(b)};
}
constexpr Quantity operator-(Quantity a, Quantity b) noexcept
{
    return Quantity{to_underlying(a) - to_underlying(b)};
}
constexpr Quantity& operator+=(Quantity& a, Quantity b) noexcept
{
    a = a + b;
    return a;
}
constexpr Quantity& operator-=(Quantity& a, Quantity b) noexcept
{
    a = a - b;
    return a;
}

constexpr Quantity qty_min(Quantity a, Quantity b) noexcept
{
    return to_underlying(a) < to_underlying(b) ? a : b;
}

enum class Side : std::uint8_t
{
    Buy,
    Sell
};

enum class OrderType : std::uint8_t
{
    Limit,
    Market
};

enum class TimeInForce : std::uint8_t
{
    GTC,
    IOC,
    FOK,
    Day
};

enum class OrderStatus : std::uint8_t
{
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected,
};

constexpr Side opposite(Side s) noexcept
{
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

} // namespace ordbk
