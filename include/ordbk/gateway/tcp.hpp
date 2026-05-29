#pragma once

// Thin cross-platform TCP shim. Synchronous blocking I/O. Phase 3 scope.
// epoll / io_uring / IOCP arrive with the performance work in Phase 4.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ordbk::gateway::tcp
{

#ifdef _WIN32
using socket_t = SOCKET;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
inline constexpr socket_t kInvalidSocket = -1;
#endif

// Initialize WSA on Windows (no-op on POSIX). Safe to call multiple times.
void init_once();

// Close socket portably.
void close_socket(socket_t s) noexcept;

// Returns # bytes read, 0 on EOF, -1 on error.
[[nodiscard]] long recv_some(socket_t, std::span<std::byte> out) noexcept;

// Returns true if all bytes were sent.
[[nodiscard]] bool send_all(socket_t, std::span<const std::byte> data) noexcept;

[[nodiscard]] socket_t listen_on(std::uint16_t port, int backlog = 16);
[[nodiscard]] socket_t accept_one(socket_t listener);
[[nodiscard]] socket_t connect_to(std::string_view host, std::uint16_t port);

} // namespace ordbk::gateway::tcp
