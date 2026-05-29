// Unit tests for the gateway Dispatcher - drives Session+MatchingEngine
// without any TCP involved.

#include <gtest/gtest.h>

#include "ordbk/gateway/dispatcher.hpp"
#include "ordbk/utils/order_pool.hpp"

using namespace ordbk;
using namespace ordbk::protocol;

namespace
{

// Pool shared across all tests in this file.
OrderPool g_pool{256};

InstrumentRegistry make_registry()
{
    InstrumentRegistry r;
    r.add(InstrumentSpec{InstrumentId{1}, "AAPL", InstrumentType::Equity, 1, 1, "USD"});
    r.freeze();
    return r;
}

// Push an inbound message into a session by encoding it directly.
void feed(Session& s, const MessageBody& body)
{
    static std::uint32_t seq = 0;
    auto bytes = encode(++seq, body);
    s.on_bytes(bytes);
}

// Decode all outbound bytes into a vector of message types for assertions.
std::vector<DecodedMessage> drain(Session& s)
{
    auto bytes = s.take_outbound();
    Framer f;
    f.feed(bytes);
    std::vector<DecodedMessage> out;
    while (auto m = f.next())
        out.push_back(std::move(*m));
    return out;
}

} // namespace

TEST(Dispatcher, NewOrderBeforeLogonRejected)
{
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Dispatcher d{engine};
    Session s{1};

    feed(s, NewOrderMsg{1, 1, 0, 0, 0, 100, 5});
    d.pump(s);
    auto out = drain(s);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].header.msg_type, MsgType::OrderReject);
    auto* rj = std::get_if<OrderRejectMsg>(&out[0].body);
    ASSERT_NE(rj, nullptr);
    EXPECT_EQ(rj->reason, RejectReason::NotLoggedOn);
}

TEST(Dispatcher, LogonActivatesSession)
{
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Dispatcher d{engine};
    Session s{1};

    feed(s, LogonMsg{42});
    d.pump(s);
    EXPECT_EQ(s.state(), Session::State::Active);
    EXPECT_EQ(s.client_id(), 42u);
    auto out = drain(s);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].header.msg_type, MsgType::Logon);
}

TEST(Dispatcher, NewOrderRestsAndAcks)
{
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Dispatcher d{engine};
    Session s{1};

    feed(s, LogonMsg{42});
    feed(s, NewOrderMsg{1001, 1, 0, 0, 0, 100, 5});
    d.pump(s);

    auto out = drain(s);
    ASSERT_GE(out.size(), 2u);
    EXPECT_EQ(out[1].header.msg_type, MsgType::OrderAck);
    EXPECT_EQ(*engine.book(InstrumentId{1})->best_bid(), Price{100});
}

TEST(Dispatcher, CrossingOrderEmitsFill)
{
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Dispatcher d{engine};
    Session s{1};

    feed(s, LogonMsg{42});
    feed(s, NewOrderMsg{1, 1, 1, 0, 0, 100, 5}); // resting sell @ 100
    feed(s, NewOrderMsg{2, 1, 0, 0, 0, 100, 5}); // crossing buy @ 100
    d.pump(s);

    auto out = drain(s);
    // Logon-echo + ack1 + ack2 + fill
    ASSERT_GE(out.size(), 4u);
    std::size_t fills = 0;
    for (const auto& m : out)
    {
        if (m.header.msg_type == MsgType::Fill)
            ++fills;
    }
    EXPECT_EQ(fills, 1u);
}

TEST(Dispatcher, CancelRemovesRestingOrder)
{
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Dispatcher d{engine};
    Session s{1};

    feed(s, LogonMsg{42});
    feed(s, NewOrderMsg{1, 1, 0, 0, 0, 100, 5});
    feed(s, CancelOrderMsg{1, 1});
    d.pump(s);

    EXPECT_TRUE(engine.book(InstrumentId{1})->empty());
    auto out = drain(s);
    std::size_t cancels = 0;
    for (const auto& m : out)
    {
        if (m.header.msg_type == MsgType::Cancelled)
            ++cancels;
    }
    EXPECT_EQ(cancels, 1u);
}

TEST(Dispatcher, UnknownInstrumentRejected)
{
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Dispatcher d{engine};
    Session s{1};

    feed(s, LogonMsg{42});
    feed(s, NewOrderMsg{1, 999, 0, 0, 0, 100, 5});
    d.pump(s);

    auto out = drain(s);
    auto* rj = std::get_if<OrderRejectMsg>(&out.back().body);
    ASSERT_NE(rj, nullptr);
    EXPECT_EQ(rj->reason, RejectReason::UnknownInstrument);
}

TEST(Dispatcher, LogoutTransitionsToClosing)
{
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Dispatcher d{engine};
    Session s{1};

    feed(s, LogonMsg{42});
    feed(s, LogoutMsg{});
    d.pump(s);
    EXPECT_EQ(s.state(), Session::State::Closing);
}
