#pragma once

#include "ordbk/matching/engine.hpp"
#include "ordbk/protocol/session.hpp"

namespace ordbk::gateway
{

// Translates Session messages into Engine calls.
//
// Synchronous: each NewOrder/Cancel is processed inline on the same Session.
// ShardedMatcher wiring would go here for multi-threaded mode.
class Dispatcher
{
public:
    explicit Dispatcher(Engine& engine) noexcept : engine_(engine) {}

    // Process all fully-framed inbound messages in the session.
    void pump(protocol::Session& session);

private:
    void handle_logon(protocol::Session&, const protocol::LogonMsg&);
    void handle_logout(protocol::Session&);
    void handle_heartbeat(protocol::Session&);
    void handle_new_order(protocol::Session&, const protocol::NewOrderMsg&);
    void handle_cancel(protocol::Session&, const protocol::CancelOrderMsg&);

    Engine& engine_;
    std::uint64_t next_server_order_id_{1};
};

} // namespace ordbk::gateway
