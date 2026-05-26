#pragma once

#include "velox/core/types.hpp"
#include "velox/instruments/option_contract.hpp"

#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

namespace velox {

// Tracks options instruments by expiry date and returns those that have
// expired when sweep() is called.
//
// Intended usage:
//   sweeper.register_instrument(id, expiry);
//   ...
//   auto expired = sweeper.sweep(today);   // returns ids with expiry <= today
//   for (auto id : expired) engine.expire_instrument(id);
class ExpirySweeper {
public:
    using ExpireCallback = std::function<void(InstrumentId)>;

    // Optional callback invoked per expired instrument inside sweep().
    void set_callback(ExpireCallback cb) { callback_ = std::move(cb); }

    // Register an instrument that expires on `expiry`.
    void register_instrument(InstrumentId id, ExpiryDate expiry);

    // Remove an instrument without triggering expiry (e.g. on manual withdrawal).
    void unregister(InstrumentId id);

    // Collect all instruments with expiry <= today, invoke callback for each,
    // remove them from the sweeper, and return their ids.
    std::vector<InstrumentId> sweep(ExpiryDate today);

    [[nodiscard]] std::size_t size() const noexcept { return id_to_expiry_.size(); }

private:
    std::multimap<ExpiryDate, InstrumentId>       by_expiry_;
    std::unordered_map<std::uint32_t, ExpiryDate> id_to_expiry_;
    ExpireCallback                                callback_;
};

}  // namespace velox
