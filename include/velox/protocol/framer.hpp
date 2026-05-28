#pragma once

#include "velox/protocol/codec.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace velox::protocol
{

// Stream framer: accepts arbitrary chunks of bytes (TCP delivers no message
// boundaries) and yields one DecodedMessage at a time.
//
// On a malformed header (wrong version, oversize body) the framer flags an
// error and rejects all further input until reset(). Real exchanges would
// drop the connection; we expose `has_error()` and let the caller decide.
class Framer
{
public:
    enum class Error
    {
        None,
        BadVersion,
        OversizeBody,
        BadBody
    };

    void feed(std::span<const std::byte> data);
    [[nodiscard]] std::optional<DecodedMessage> next();

    void reset() noexcept
    {
        buf_.clear();
        error_ = Error::None;
    }

    [[nodiscard]] bool has_error() const noexcept { return error_ != Error::None; }
    [[nodiscard]] Error error() const noexcept { return error_; }
    [[nodiscard]] std::size_t buffered_bytes() const noexcept { return buf_.size(); }

private:
    std::vector<std::byte> buf_;
    Error error_{Error::None};
};

} // namespace velox::protocol
