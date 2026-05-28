#include "velox/gateway/client.hpp"

#include <array>

namespace velox::gateway
{

Client::Client(std::string_view host, std::uint16_t port) : sock_(tcp::connect_to(host, port)) {}

Client::~Client()
{
    if (sock_ != tcp::kInvalidSocket)
        tcp::close_socket(sock_);
}

bool Client::send(const protocol::MessageBody& body)
{
    auto bytes = protocol::encode(++out_seq_, body);
    return tcp::send_all(sock_, bytes);
}

std::optional<protocol::DecodedMessage> Client::recv()
{
    std::array<std::byte, 1024> buf{};
    while (true)
    {
        if (auto m = framer_.next())
            return m;
        if (framer_.has_error())
            return std::nullopt;
        const long n = tcp::recv_some(sock_, buf);
        if (n <= 0)
            return std::nullopt;
        framer_.feed(std::span<const std::byte>{buf.data(), static_cast<std::size_t>(n)});
    }
}

} // namespace velox::gateway
