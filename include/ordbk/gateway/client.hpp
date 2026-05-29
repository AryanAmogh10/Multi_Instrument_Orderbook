#pragma once

#include "ordbk/gateway/tcp.hpp"
#include "ordbk/protocol/codec.hpp"
#include "ordbk/protocol/framer.hpp"

#include <cstdint>
#include <string_view>

namespace ordbk::gateway
{

// Thin synchronous TCP client. Send a message, drain the framer until a
// response arrives. Mirror image of the server's session_loop.
class Client
{
public:
    Client(std::string_view host, std::uint16_t port);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    [[nodiscard]] bool send(const protocol::MessageBody& body);

    // Blocks until the next complete message arrives or the socket closes.
    [[nodiscard]] std::optional<protocol::DecodedMessage> recv();

private:
    tcp::socket_t sock_;
    protocol::Framer framer_;
    std::uint32_t out_seq_{0};
};

} // namespace ordbk::gateway
