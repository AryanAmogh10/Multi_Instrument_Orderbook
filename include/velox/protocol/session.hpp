#pragma once

#include "velox/matching/sharded_engine.hpp"
#include "velox/protocol/codec.hpp"
#include "velox/protocol/framer.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace velox::protocol {

// Per-connection state machine. Decoupled from any specific I/O layer:
//   - feed inbound bytes via on_bytes()
//   - get outbound bytes via take_outbound()
//   - on each fill/ack/reject emitted by the engine, the session encodes
//     the appropriate outbound message
//
// Phase 3 wires submit() through ShardedEngine. Results are observed via a
// callback registered at construction (e.g. by the dispatcher draining
// completed orders). For the integration test we use the simpler approach of
// having the gateway call the engine synchronously and push results back.
class Session {
public:
    enum class State : std::uint8_t { NotLoggedOn, Active, Closing };

    explicit Session(std::uint64_t session_id) noexcept : session_id_(session_id) {}

    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] std::uint64_t id() const noexcept { return session_id_; }
    [[nodiscard]] std::uint32_t client_id() const noexcept { return client_id_; }
    [[nodiscard]] bool has_error() const noexcept { return framer_.has_error(); }

    void on_bytes(std::span<const std::byte> data) { framer_.feed(data); }

    // Pull the next decoded inbound message, or nullopt if incomplete.
    [[nodiscard]] std::optional<DecodedMessage> next_inbound() { return framer_.next(); }

    // Emit a message to be written to the wire.
    void emit(const MessageBody& body);

    // Move all queued outbound bytes out (server I/O loop will write them).
    [[nodiscard]] std::vector<std::byte> take_outbound();

    void set_state(State s) noexcept { state_ = s; }
    void set_client(std::uint32_t cid) noexcept { client_id_ = cid; }

    [[nodiscard]] std::uint32_t next_outbound_seq() noexcept { return ++out_seq_; }

private:
    std::uint64_t          session_id_;
    State                  state_{State::NotLoggedOn};
    std::uint32_t          client_id_{0};
    std::uint32_t          out_seq_{0};
    Framer                 framer_;
    std::vector<std::byte> outbound_;
};

}  // namespace velox::protocol
