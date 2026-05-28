#include "velox/gateway/tcp.hpp"

#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>

namespace velox::gateway::tcp
{

#ifdef _WIN32
namespace
{
std::atomic<bool> g_wsa_initialised{false};
}

void init_once()
{
    bool expected = false;
    if (!g_wsa_initialised.compare_exchange_strong(expected, true))
        return;
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        throw std::runtime_error{"WSAStartup failed"};
    }
}

void close_socket(socket_t s) noexcept
{
    ::closesocket(s);
}
#else
void init_once() {}
void close_socket(socket_t s) noexcept
{
    ::close(s);
}
#endif

long recv_some(socket_t s, std::span<std::byte> out) noexcept
{
#ifdef _WIN32
    const int n = ::recv(s, reinterpret_cast<char*>(out.data()), static_cast<int>(out.size()), 0);
    return static_cast<long>(n);
#else
    const auto n = ::recv(s, out.data(), out.size(), 0);
    return static_cast<long>(n);
#endif
}

bool send_all(socket_t s, std::span<const std::byte> data) noexcept
{
    std::size_t sent = 0;
    while (sent < data.size())
    {
#ifdef _WIN32
        const int n = ::send(s,
                             reinterpret_cast<const char*>(data.data() + sent),
                             static_cast<int>(data.size() - sent),
                             0);
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
#else
        const auto n = ::send(s, data.data() + sent, data.size() - sent, 0);
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
#endif
    }
    return true;
}

socket_t listen_on(std::uint16_t port, int backlog)
{
    init_once();
    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == kInvalidSocket)
        throw std::runtime_error{"socket() failed"};

    int reuse = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        close_socket(s);
        throw std::runtime_error{"bind() failed"};
    }
    if (::listen(s, backlog) != 0)
    {
        close_socket(s);
        throw std::runtime_error{"listen() failed"};
    }
    return s;
}

socket_t accept_one(socket_t listener)
{
    sockaddr_in peer{};
#ifdef _WIN32
    int len = static_cast<int>(sizeof(peer));
#else
    socklen_t len = sizeof(peer);
#endif
    return ::accept(listener, reinterpret_cast<sockaddr*>(&peer), &len);
}

socket_t connect_to(std::string_view host, std::uint16_t port)
{
    init_once();
    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == kInvalidSocket)
        throw std::runtime_error{"socket() failed"};

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    const std::string host_s{host};
    if (::inet_pton(AF_INET, host_s.c_str(), &addr.sin_addr) != 1)
    {
        close_socket(s);
        throw std::runtime_error{"inet_pton() failed"};
    }
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        close_socket(s);
        throw std::runtime_error{"connect() failed"};
    }
    return s;
}

} // namespace velox::gateway::tcp
