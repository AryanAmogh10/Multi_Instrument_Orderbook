#include "velox/protocol/session.hpp"

namespace velox::protocol {

void Session::emit(const MessageBody& body) {
    auto bytes = encode(next_outbound_seq(), body);
    outbound_.insert(outbound_.end(), bytes.begin(), bytes.end());
}

std::vector<std::byte> Session::take_outbound() {
    std::vector<std::byte> out;
    out.swap(outbound_);
    return out;
}

}  // namespace velox::protocol
