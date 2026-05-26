#include <gtest/gtest.h>

#include "velox/instruments/option_contract.hpp"

using namespace velox;

// ---- ExpiryDate -----------------------------------------------------------

TEST(ExpiryDate, ToString) {
    ExpiryDate d{2026, 1, 17};
    EXPECT_EQ(d.to_string(), "2026-01-17");
}

TEST(ExpiryDate, Ordering) {
    ExpiryDate jan{2026, 1, 17};
    ExpiryDate feb{2026, 2, 20};
    ExpiryDate dec25{2025, 12, 19};
    EXPECT_TRUE(dec25 < jan);
    EXPECT_TRUE(jan  < feb);
    EXPECT_TRUE(jan  == jan);
    EXPECT_FALSE(feb < jan);
}

// ---- OCC symbol encoding --------------------------------------------------

TEST(OptionContract, OCCCallEncoding) {
    OptionContract c;
    c.underlying  = "AAPL";
    c.strike      = Price{15000};   // $150.00 (1 tick = 1 cent)
    c.expiry      = ExpiryDate{2026, 1, 17};
    c.option_type = OptionType::Call;

    // "AAPL  " + "260117" + "C" + "00150000"
    EXPECT_EQ(c.occ_symbol(), "AAPL  260117C00150000");
}

TEST(OptionContract, OCCPutEncoding) {
    OptionContract c;
    c.underlying  = "SPY";
    c.strike      = Price{55000};   // $550.00
    c.expiry      = ExpiryDate{2026, 3, 20};
    c.option_type = OptionType::Put;

    // "SPY   " + "260320" + "P" + "00550000"
    EXPECT_EQ(c.occ_symbol(), "SPY   260320P00550000");
}

TEST(OptionContract, OCCLongUnderlying) {
    OptionContract c;
    c.underlying  = "GOOGL";
    c.strike      = Price{17000};   // $170.00
    c.expiry      = ExpiryDate{2026, 6, 19};
    c.option_type = OptionType::Call;

    EXPECT_EQ(c.occ_symbol(), "GOOGL 260619C00170000");
}

TEST(OptionContract, OCCRoundTrip) {
    OptionContract orig;
    orig.underlying  = "MSFT";
    orig.strike      = Price{40000};  // $400.00
    orig.expiry      = ExpiryDate{2026, 9, 18};
    orig.option_type = OptionType::Put;

    auto sym = orig.occ_symbol();
    auto parsed = OptionContract::from_occ(sym);

    EXPECT_EQ(parsed.underlying,  orig.underlying);
    EXPECT_EQ(parsed.strike,      orig.strike);
    EXPECT_EQ(parsed.expiry,      orig.expiry);
    EXPECT_EQ(parsed.option_type, orig.option_type);
}

TEST(OptionContract, OCCFromOCCThrowsOnShortInput) {
    EXPECT_THROW(OptionContract::from_occ("AAPL260117C0015000"), std::invalid_argument);
}

TEST(OptionContract, OCCFromOCCThrowsOnBadCP) {
    EXPECT_THROW(OptionContract::from_occ("AAPL  260117X00150000"), std::invalid_argument);
}

TEST(OptionContract, OCCSmallStrike) {
    OptionContract c;
    c.underlying  = "AMD";
    c.strike      = Price{5000};  // $50.00
    c.expiry      = ExpiryDate{2026, 1, 16};
    c.option_type = OptionType::Call;

    EXPECT_EQ(c.occ_symbol(), "AMD   260116C00050000");
    auto p = OptionContract::from_occ(c.occ_symbol());
    EXPECT_EQ(p.strike, c.strike);
}
