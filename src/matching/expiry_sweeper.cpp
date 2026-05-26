#include "velox/matching/expiry_sweeper.hpp"

namespace velox {

void ExpirySweeper::register_instrument(InstrumentId id, ExpiryDate expiry) {
    auto uid = to_underlying(id);
    id_to_expiry_.emplace(uid, expiry);
    by_expiry_.emplace(expiry, id);
}

void ExpirySweeper::unregister(InstrumentId id) {
    auto uid = to_underlying(id);
    auto it = id_to_expiry_.find(uid);
    if (it == id_to_expiry_.end()) return;
    ExpiryDate expiry = it->second;
    auto range = by_expiry_.equal_range(expiry);
    for (auto r = range.first; r != range.second; ++r) {
        if (r->second == id) {
            by_expiry_.erase(r);
            break;
        }
    }
    id_to_expiry_.erase(it);
}

std::vector<InstrumentId> ExpirySweeper::sweep(ExpiryDate today) {
    std::vector<InstrumentId> expired;
    auto it = by_expiry_.begin();
    while (it != by_expiry_.end() && !(today < it->first)) {
        expired.push_back(it->second);
        id_to_expiry_.erase(to_underlying(it->second));
        it = by_expiry_.erase(it);
    }
    if (callback_) {
        for (InstrumentId id : expired) callback_(id);
    }
    return expired;
}

}  // namespace velox
