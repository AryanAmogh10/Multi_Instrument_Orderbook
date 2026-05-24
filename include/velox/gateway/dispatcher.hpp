#pragma once

#include "velox/matching/matching_engine.hpp"
#include "velox/protocol/session.hpp"

namespace velox::gateway {

// Translates between Session inbound messages and MatchingEngine calls.
//
// Synchronous (Phase 3): each NewOrder/Cancel is processed inline and
// responses are emitted on the same Session immediately. The ShardedEngine
// from Phase 2 will be wired up in Phase 4 along with proper result routing.
class Dispatcher {
public:
    explicit Dispatcher(MatchingEngine& engine) noexcept : engine_(engine) {}

    // Process every fully framed inbound message currently in `session`.
    void pump(protocol::Session& session);

private:
    void handle_logon(protocol::Session&, const protocol::LogonMsg&);
    void handle_logout(protocol::Session&);
    void handle_heartbeat(protocol::Session&);
    void handle_new_order(protocol::Session&, const protocol::NewOrderMsg&);
    void handle_cancel(protocol::Session&, const protocol::CancelOrderMsg&);

    MatchingEngine& engine_;
    std::uint64_t   next_server_order_id_{1};
};

}  // namespace velox::gateway
