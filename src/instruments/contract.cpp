#include "velox/instruments/contract.hpp"

#include <cstdio>
#include <stdexcept>

namespace velox {

std::string ExpiryDate::to_string() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  static_cast<int>(year),
                  static_cast<int>(month),
                  static_cast<int>(day));
    return buf;
}

std::string Contract::occ_symbol() const {
    // Root: left-justified, space-padded to 6 chars
    char root[7] = "      ";
    for (std::size_t i = 0; i < underlying.size() && i < 6; ++i)
        root[i] = underlying[i];

    char date[12];
    std::snprintf(date, sizeof(date), "%02d%02d%02d",
                  static_cast<int>(expiry.year % 100),
                  static_cast<int>(expiry.month),
                  static_cast<int>(expiry.day));

    char cp = (option_type == OptionType::Call) ? 'C' : 'P';

    // strike in thousandths (1 tick = 1 cent = 10 thousandths)
    std::int64_t thousandths = to_underlying(strike) * 10;
    char strike_str[9];
    std::snprintf(strike_str, sizeof(strike_str), "%08lld",
                  static_cast<long long>(thousandths));

    std::string result;
    result.reserve(21);
    result.append(root, 6);
    result.append(date, 6);
    result += cp;
    result.append(strike_str, 8);
    return result;
}

Contract Contract::from_occ(std::string_view occ) {
    if (occ.size() < 21) {
        throw std::invalid_argument{"OCC symbol too short: must be at least 21 chars"};
    }

    auto parse_int = [&](std::string_view sv) -> long long {
        long long v = 0;
        for (char ch : sv) {
            if (ch < '0' || ch > '9')
                throw std::invalid_argument{"Non-digit in OCC symbol"};
            v = v * 10 + (ch - '0');
        }
        return v;
    };

    Contract c;

    // Root: first 6 chars, trim trailing spaces
    c.underlying = std::string(occ.substr(0, 6));
    while (!c.underlying.empty() && c.underlying.back() == ' ')
        c.underlying.pop_back();

    int yy = static_cast<int>(parse_int(occ.substr(6, 2)));
    int mm = static_cast<int>(parse_int(occ.substr(8, 2)));
    int dd = static_cast<int>(parse_int(occ.substr(10, 2)));
    c.expiry = ExpiryDate{
        static_cast<std::uint16_t>(2000 + yy),
        static_cast<std::uint8_t>(mm),
        static_cast<std::uint8_t>(dd)
    };

    char cp = occ[12];
    if (cp == 'C')      c.option_type = OptionType::Call;
    else if (cp == 'P') c.option_type = OptionType::Put;
    else throw std::invalid_argument{"OCC symbol: expected 'C' or 'P' at position 12"};

    // strike: 8 digits at positions 13-20, in thousandths -> divide by 10 to get cents
    long long thousandths = parse_int(occ.substr(13, 8));
    c.strike = Price{thousandths / 10};

    return c;
}

}  // namespace velox
