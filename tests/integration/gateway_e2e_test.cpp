#include <gtest/gtest.h>

#include "velox/gateway/client.hpp"
#include "velox/gateway/server.hpp"
#include "velox/utils/order_pool.hpp"

#include <chrono>
#include <thread>

using namespace velox;
using namespace velox::protocol;

namespace {

// Pool shared across all e2e tests. The Server/Dispatcher acquires from this
// pool on the session thread and releases terminal orders after each order.
OrderPool g_pool{1024};

InstrumentRegistry make_registry() {
    InstrumentRegistry r;
    r.add(InstrumentSpec{InstrumentId{1}, "AAPL", InstrumentType::Equity, 1, 1, "USD"});
    r.freeze();
    return r;
}

// Pick a high port unlikely to collide. CI runs serialised so this is fine.
constexpr std::uint16_t kPort = 47811;

}  // namespace

TEST(GatewayE2E, LogonNewOrderFillRoundTrip) {
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Server server{engine, kPort};
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    gateway::Client client{"127.0.0.1", kPort};

    // Logon
    ASSERT_TRUE(client.send(LogonMsg{42}));
    auto logon_ack = client.recv();
    ASSERT_TRUE(logon_ack.has_value());
    EXPECT_EQ(logon_ack->header.msg_type, MsgType::Logon);

    // Resting sell @ 100
    ASSERT_TRUE(client.send(NewOrderMsg{1, 1, 1, 0, 0, 100, 5}));
    auto ack1 = client.recv();
    ASSERT_TRUE(ack1.has_value());
    EXPECT_EQ(ack1->header.msg_type, MsgType::OrderAck);

    // Crossing buy @ 100
    ASSERT_TRUE(client.send(NewOrderMsg{2, 1, 0, 0, 0, 100, 5}));
    auto ack2 = client.recv();
    ASSERT_TRUE(ack2.has_value());
    EXPECT_EQ(ack2->header.msg_type, MsgType::OrderAck);
    auto fill = client.recv();
    ASSERT_TRUE(fill.has_value());
    EXPECT_EQ(fill->header.msg_type, MsgType::Fill);
    auto* f = std::get_if<FillMsg>(&fill->body);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->price, 100);
    EXPECT_EQ(f->quantity, 5u);

    server.stop();
}

TEST(GatewayE2E, CancelRoundTrip) {
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Server server{engine, static_cast<std::uint16_t>(kPort + 1)};
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    gateway::Client client{"127.0.0.1", static_cast<std::uint16_t>(kPort + 1)};

    ASSERT_TRUE(client.send(LogonMsg{1}));
    ASSERT_TRUE(client.recv().has_value());

    ASSERT_TRUE(client.send(NewOrderMsg{10, 1, 0, 0, 0, 100, 5}));
    ASSERT_TRUE(client.recv().has_value());   // ack

    ASSERT_TRUE(client.send(CancelOrderMsg{10, 1}));
    auto cx = client.recv();
    ASSERT_TRUE(cx.has_value());
    EXPECT_EQ(cx->header.msg_type, MsgType::Cancelled);

    server.stop();
}

TEST(GatewayE2E, OrderBeforeLogonRejected) {
    auto reg = make_registry();
    MatchingEngine engine{reg, g_pool};
    gateway::Server server{engine, static_cast<std::uint16_t>(kPort + 2)};
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    gateway::Client client{"127.0.0.1", static_cast<std::uint16_t>(kPort + 2)};

    ASSERT_TRUE(client.send(NewOrderMsg{1, 1, 0, 0, 0, 100, 5}));
    auto resp = client.recv();
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->header.msg_type, MsgType::OrderReject);
    auto* rj = std::get_if<OrderRejectMsg>(&resp->body);
    ASSERT_NE(rj, nullptr);
    EXPECT_EQ(rj->reason, RejectReason::NotLoggedOn);

    server.stop();
}
