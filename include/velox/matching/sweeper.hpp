#pragma once

#include "velox/core/types.hpp"
#include "velox/instruments/contract.hpp"

#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

namespace velox {

// Tracks options by expiry and returns the ones that have expired when sweep() is called.
//
// Usage:
//   sweeper.register_instrument(id, expiry);
//   auto expired = sweeper.sweep(today);
//   for (auto id : expired) engine.expire_instrument(id);
class Sweeper {
public:
    using ExpireCallback = std::function<void(InstrumentId)>;

    void set_callback(ExpireCallback cb) { callback_ = std::move(cb); }

    void register_instrument(InstrumentId id, ExpiryDate expiry);

    // Remove without triggering expiry (e.g. manual withdrawal).
    void unregister(InstrumentId id);

    // Returns all instruments with expiry <= today, fires callback, removes them.
    std::vector<InstrumentId> sweep(ExpiryDate today);

    [[nodiscard]] std::size_t size() const noexcept { return id_to_expiry_.size(); }

private:
    std::multimap<ExpiryDate, InstrumentId>       by_expiry_;
    std::unordered_map<std::uint32_t, ExpiryDate> id_to_expiry_;
    ExpireCallback                                callback_;
};

// keep old name
using ExpirySweeper = Sweeper;

}  // namespace velox
