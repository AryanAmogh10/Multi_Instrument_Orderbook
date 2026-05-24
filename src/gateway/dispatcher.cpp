#include "velox/gateway/dispatcher.hpp"

namespace velox::gateway {

using namespace velox::protocol;

namespace {

RejectReason classify(OrderStatus s, const NewOrderMsg& m, const MatchingEngine& engine) {
    if (s != OrderStatus::Rejected) return RejectReason::Unknown;
    if (engine.book(InstrumentId{m.instrument_id}) == nullptr) {
        return RejectReason::UnknownInstrument;
    }
    if (m.quantity == 0) return RejectReason::InvalidQuantity;
    if (m.order_type == 0 && m.price <= 0) return RejectReason::InvalidPrice;
    if (m.order_type == 1 && (m.tif == 0 || m.tif == 3)) return RejectReason::InvalidTif;
    if (m.tif == 2) return RejectReason::InsufficientLiquidity;
    return RejectReason::Unknown;
}

}  // namespace

void Dispatcher::pump(Session& session) {
    while (auto msg = session.next_inbound()) {
        std::visit([&](const auto& body) {
            using T = std::decay_t<decltype(body)>;
            if constexpr (std::is_same_v<T, LogonMsg>) {
                handle_logon(session, body);
            } else if constexpr (std::is_same_v<T, LogoutMsg>) {
                handle_logout(session);
            } else if constexpr (std::is_same_v<T, HeartbeatMsg>) {
                handle_heartbeat(session);
            } else if constexpr (std::is_same_v<T, NewOrderMsg>) {
                handle_new_order(session, body);
            } else if constexpr (std::is_same_v<T, CancelOrderMsg>) {
                handle_cancel(session, body);
            }
            // Outbound-only message types arriving inbound are silently dropped.
        }, msg->body);
    }
}

void Dispatcher::handle_logon(Session& s, const LogonMsg& m) {
    if (s.state() != Session::State::NotLoggedOn) return;
    s.set_client(m.client_id);
    s.set_state(Session::State::Active);
    s.emit(LogonMsg{m.client_id});  // echo accepts logon
}

void Dispatcher::handle_logout(Session& s) {
    s.set_state(Session::State::Closing);
    s.emit(LogoutMsg{});
}

void Dispatcher::handle_heartbeat(Session& s) {
    s.emit(HeartbeatMsg{});
}

void Dispatcher::handle_new_order(Session& s, const NewOrderMsg& m) {
    if (s.state() != Session::State::Active) {
        s.emit(OrderRejectMsg{m.client_order_id, RejectReason::NotLoggedOn});
        return;
    }

    // Phase 3: use client_order_id as the engine's OrderId. Server-side
    // renumbering is left for later phases (real exchanges do remap).
    const std::uint64_t server_id = m.client_order_id;
    (void)next_server_order_id_;
    auto order = std::make_shared<Order>(Order{
        OrderId{server_id},
        InstrumentId{m.instrument_id},
        ClientId{s.client_id()},
        static_cast<Side>(m.side),
        static_cast<OrderType>(m.order_type),
        static_cast<TimeInForce>(m.tif),
        Price{m.price},
        Quantity{m.quantity},
        kZeroQty,
        Timestamp{0},
        OrderStatus::New,
    });

    auto result = engine_.submit(order);

    if (result.order->status == OrderStatus::Rejected) {
        s.emit(OrderRejectMsg{m.client_order_id, classify(OrderStatus::Rejected, m, engine_)});
        return;
    }

    s.emit(OrderAckMsg{m.client_order_id, server_id});
    for (const auto& t : result.trades) {
        s.emit(FillMsg{
            m.client_order_id,
            server_id,
            to_underlying(t.price),
            to_underlying(t.quantity),
            static_cast<std::uint8_t>(result.order->status),
        });
    }
}

void Dispatcher::handle_cancel(Session& s, const CancelOrderMsg& m) {
    if (s.state() != Session::State::Active) {
        s.emit(OrderRejectMsg{m.client_order_id, RejectReason::NotLoggedOn});
        return;
    }
    const bool ok = engine_.cancel(InstrumentId{m.instrument_id}, OrderId{m.client_order_id});
    if (ok) {
        s.emit(CancelledMsg{m.client_order_id});
    } else {
        s.emit(OrderRejectMsg{m.client_order_id, RejectReason::Unknown});
    }
}

}  // namespace velox::gateway
