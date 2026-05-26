#include "velox/gateway/server.hpp"

#include <algorithm>
#include <array>
#include <mutex>

namespace velox::gateway {

Server::Server(Engine& engine, std::uint16_t port)
    : engine_(engine), dispatcher_(engine), port_(port) {}

Server::~Server() {
    stop();
}

void Server::start() {
    if (running_.exchange(true)) return;
    listener_ = tcp::listen_on(port_);
    accept_thread_ = std::thread{[this] { accept_loop(); }};
}

void Server::stop() {
    if (!running_.exchange(false)) return;
    // Close listener to unblock accept().
    if (listener_ != tcp::kInvalidSocket) {
        tcp::close_socket(listener_);
        listener_ = tcp::kInvalidSocket;
    }
    // Close every live session socket to unblock recv() in worker threads.
    {
        std::lock_guard lk{session_mu_};
        for (auto s : session_sockets_) tcp::close_socket(s);
        session_sockets_.clear();
    }
    if (accept_thread_.joinable()) accept_thread_.join();
    for (auto& t : session_threads_) {
        if (t.joinable()) t.join();
    }
    session_threads_.clear();
}

void Server::accept_loop() {
    while (running_.load(std::memory_order_acquire)) {
        tcp::socket_t s = tcp::accept_one(listener_);
        if (s == tcp::kInvalidSocket) break;
        const std::uint64_t sid = next_session_id_.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard lk{session_mu_};
            session_sockets_.push_back(s);
        }
        session_threads_.emplace_back([this, s, sid] { session_loop(s, sid); });
    }
}

void Server::session_loop(tcp::socket_t sock, std::uint64_t session_id) {
    protocol::Session session{session_id};
    std::array<std::byte, 1024> buf{};

    while (running_.load(std::memory_order_acquire) &&
           session.state() != protocol::Session::State::Closing) {
        const long n = tcp::recv_some(sock, buf);
        if (n <= 0) break;
        session.on_bytes(std::span<const std::byte>{buf.data(), static_cast<std::size_t>(n)});
        if (session.has_error()) break;

        dispatcher_.pump(session);

        auto outbound = session.take_outbound();
        if (!outbound.empty()) {
            if (!tcp::send_all(sock, outbound)) break;
        }
    }
    tcp::close_socket(sock);
    {
        std::lock_guard lk{session_mu_};
        auto it = std::find(session_sockets_.begin(), session_sockets_.end(), sock);
        if (it != session_sockets_.end()) session_sockets_.erase(it);
    }
}

}  // namespace velox::gateway
