#include "velox/instruments/option_chain.hpp"

namespace velox {

OptionChain::OptionChain(std::string underlying)
    : underlying_(std::move(underlying)) {}

bool OptionChain::add(InstrumentId id, const OptionContract& contract) {
    auto uid = to_underlying(id);
    if (by_id_.count(uid)) return false;

    auto& strike_slot = levels_[contract.expiry][contract.strike];
    if (contract.option_type == OptionType::Call) {
        if (strike_slot.call.has_value()) return false;
        strike_slot.call = id;
    } else {
        if (strike_slot.put.has_value()) return false;
        strike_slot.put = id;
    }

    by_id_.emplace(uid, std::make_pair(contract.expiry, contract.strike));
    id_to_type_.emplace(uid, contract.option_type);
    return true;
}

bool OptionChain::remove(InstrumentId id) {
    auto uid = to_underlying(id);
    auto it = by_id_.find(uid);
    if (it == by_id_.end()) return false;

    auto [expiry, strike] = it->second;
    OptionType type = id_to_type_.at(uid);

    auto& strike_map = levels_.at(expiry);
    auto& slot = strike_map.at(strike);
    if (type == OptionType::Call) slot.call.reset();
    else                          slot.put.reset();

    if (!slot.call && !slot.put) strike_map.erase(strike);
    if (strike_map.empty()) levels_.erase(expiry);

    id_to_type_.erase(uid);
    by_id_.erase(it);
    return true;
}

std::optional<InstrumentId> OptionChain::find(
    ExpiryDate expiry, Price strike, OptionType type) const noexcept
{
    auto exp_it = levels_.find(expiry);
    if (exp_it == levels_.end()) return std::nullopt;
    auto str_it = exp_it->second.find(strike);
    if (str_it == exp_it->second.end()) return std::nullopt;
    const auto& slot = str_it->second;
    return (type == OptionType::Call) ? slot.call : slot.put;
}

std::vector<InstrumentId> OptionChain::at_expiry(ExpiryDate expiry) const {
    std::vector<InstrumentId> result;
    auto exp_it = levels_.find(expiry);
    if (exp_it == levels_.end()) return result;
    for (const auto& [strike, slot] : exp_it->second) {
        if (slot.call) result.push_back(*slot.call);
        if (slot.put)  result.push_back(*slot.put);
    }
    return result;
}

std::vector<InstrumentId> OptionChain::calls_at(ExpiryDate expiry) const {
    std::vector<InstrumentId> result;
    auto exp_it = levels_.find(expiry);
    if (exp_it == levels_.end()) return result;
    for (const auto& [strike, slot] : exp_it->second)
        if (slot.call) result.push_back(*slot.call);
    return result;
}

std::vector<InstrumentId> OptionChain::puts_at(ExpiryDate expiry) const {
    std::vector<InstrumentId> result;
    auto exp_it = levels_.find(expiry);
    if (exp_it == levels_.end()) return result;
    for (const auto& [strike, slot] : exp_it->second)
        if (slot.put) result.push_back(*slot.put);
    return result;
}

std::vector<ExpiryDate> OptionChain::expiries() const {
    std::vector<ExpiryDate> result;
    result.reserve(levels_.size());
    for (const auto& [exp, _] : levels_) result.push_back(exp);
    return result;
}

std::vector<InstrumentId> OptionChain::expiring_on_or_before(ExpiryDate date) const {
    std::vector<InstrumentId> result;
    for (const auto& [exp, strike_map] : levels_) {
        if (date < exp) break;
        for (const auto& [strike, slot] : strike_map) {
            if (slot.call) result.push_back(*slot.call);
            if (slot.put)  result.push_back(*slot.put);
        }
    }
    return result;
}

}  // namespace velox
