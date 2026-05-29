#pragma once

#include "ordbk/gateway/dispatcher.hpp"
#include "ordbk/gateway/tcp.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace ordbk::gateway
{

// Thread-per-connection TCP server. A production exchange would use
// epoll / io_uring / IOCP instead; this keeps the I/O layer simple.
class Server
{
public:
    Server(Engine& engine, std::uint16_t port);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();
    void stop();

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
    void accept_loop();
    void session_loop(tcp::socket_t sock, std::uint64_t session_id);

    Engine& engine_;
    Dispatcher dispatcher_;
    std::uint16_t port_;
    tcp::socket_t listener_{tcp::kInvalidSocket};
    std::thread accept_thread_;
    std::vector<std::thread> session_threads_;
    std::mutex session_mu_;
    std::vector<tcp::socket_t> session_sockets_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> next_session_id_{1};
};

} // namespace ordbk::gateway
